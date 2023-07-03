#pragma once

#define INVALID_TIMER_IDENTITY 0
using TimerIdentity_t = uint64_t;

class TimerTicker;
using TimerTickerShared_t = std::shared_ptr<TimerTicker>;

class TimerTicker : public std::enable_shared_from_this<TimerTicker>
{
public:
	TimerTickerShared_t Get() {
		return shared_from_this();
	}
	virtual ~TimerTicker() = default;

	virtual TimerIdentity_t AllocTimerIdentity() = 0;
	virtual void RegisterTimer(const TimerIdentity_t& _identity, const TickTime_t& _elapsedTickTime) = 0;

protected:
	TimerTicker() = default;
};

// -----------------------------------------------------------

using OnTimeHandler_t = std::function<void(const TimerIdentity_t&)>;
class TimerAccessor : public ModuleAccessor
{
public:
	TimerAccessor() = default;
	virtual ~TimerAccessor() = default;

	virtual TimerTickerShared_t AllocTimerTicker(const std::chrono::milliseconds& _timerResolution, OnTimeHandler_t&& _onTimeHandler) = 0;
};

// -----------------------------------------------------------
// create TimerJobManager instance in your module for using.
// 
// TimerJobManager::m_timerJobs : registered timer job with TimerIdentity_t value.
// on timer handler : find timer job from TimerJobManager::m_timerJobs by TimerIdentity_t value -> pass found timer job to thread pool.
// -----------------------------------------------------------

class TimerJobManager;
using TimerJobManagerShared_t = std::shared_ptr<TimerJobManager>;

class TimerJobManager : public SerializedJobQueue
{
	friend SerializedJobQueue;
	using Super_t = SerializedJobQueue;

public:
	~TimerJobManager() = default;

	void Init(TimerAccessor* _timerAccessor)
	{
		// config : timer resolution
		m_timerTicker = _timerAccessor->AllocTimerTicker(
			std::chrono::milliseconds(16),				// timer tick resolution
			[_self = Get(), this](const TimerIdentity_t& _identity)	// handler on time
			{
				Super_t::PushJob([this, _identity]()	// push calling timer job to TimerJobManager's job queue
					{
						auto found = m_timerJobs.find(_identity);
						if (found != m_timerJobs.end())
						{
							m_threadPool.PushJob(std::move(found->second));
							m_timerJobs.erase(found);
						}
					});
			});
	}

	bool PushJob(ThreadPool::Job_t&& _job, const TickTime_t& _elapsedTickTime)
	{
		if (m_shutdown || m_timerTicker == nullptr)
		{
			return false;
		}

		auto identity = m_timerTicker->AllocTimerIdentity();
		if (identity == INVALID_TIMER_IDENTITY)
		{
			return false;
		}

		Super_t::PushJob([this, identity, _job = std::move(_job), _elapsedTickTime]() mutable
			{
				m_timerJobs.emplace(identity, std::move(_job));
				m_timerTicker->RegisterTimer(identity, _elapsedTickTime);
			});

		return true;
	}

private:
	TimerJobManager(ThreadPool& _threadPool) : Super_t(_threadPool)
	{
		SetOnEmptyJob([this]()
			{
				if (m_shutdown)
				{
					m_timerJobs.clear();
				}
			});
	}

	TimerTickerShared_t m_timerTicker;

	our::unordered_map<TimerIdentity_t, ThreadPool::Job_t> m_timerJobs;
};