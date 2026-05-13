#pragma once

using TimerJobId_t = StrongId<uint64_t, 0ui64>;

struct TimerJobAccessor
{
	const TimerJobId_t id;
	std::atomic_bool canceled;	// check 3 times in TimerJobManager : check expire time, before execute job, before push repeat job
	explicit TimerJobAccessor(const TimerJobId_t& _id)
		: id(_id), canceled(false)
	{
	}
	TimerJobAccessor() = delete;
	TimerJobAccessor(const TimerJobAccessor&) = delete;
	TimerJobAccessor(TimerJobAccessor&&) = delete;
	const TimerJobAccessor& operator =(const TimerJobAccessor&) = delete;
	const TimerJobAccessor& operator =(TimerJobAccessor&&) = delete;
};
using TimerJobAccessor_t = std::shared_ptr<TimerJobAccessor>;

static const TimerJobAccessor_t INVALID_TIMER_JOB_ACCESSOR;

// ----------------------------------------------------------------

enum class ETimerJobRepeatMode
{
	None,
	AfterJob,
	OnTime
};

class TimerJobManager : public std::enable_shared_from_this<TimerJobManager>, public UseShutdown
{
public:
	virtual ~TimerJobManager() = default;

	auto Get() { return shared_from_this(); }

	virtual TimerJobAccessor_t PushTimerJob(ThreadPool::JobInst_t&& _jobInst, const TickTime_t& _elapsedTickTime, const ETimerJobRepeatMode& _repeatMode = ETimerJobRepeatMode::None) = 0;
	virtual TimerJobAccessor_t PushTimerJob(ThreadPool::JobInst_t&& _jobInst, const std::string& _cronString, const ETimerJobRepeatMode& _repeatMode = ETimerJobRepeatMode::None) = 0;

protected:
	TimerJobManager()
	{
	}
};
using TimerJobManagerShared_t = std::shared_ptr<TimerJobManager>;

// ----------------------------------------------------------------

class TimerAccessor : public ModuleAccessor
{
public:
	TimerAccessor() = default;
	virtual ~TimerAccessor() = default;

	virtual TimerJobManagerShared_t AllocateTimerJobManager(ThreadPool& _threadPool) = 0;
};