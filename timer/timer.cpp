#include "pch.h"

MODULE_STATIC_IMPL(Timer);

// ------------------------------------------------------------------------------------------

TimerTickerImpl::TimerTickerImpl(const std::chrono::milliseconds& _timerResolution, OnTimeHandler_t&& _onTimeHandler)
	: m_timerResolution(_timerResolution), m_onTimeHandler(std::move(_onTimeHandler))
{
	m_tickThread = std::thread([this]()
		{
			while (!m_shutdown)
			{
				std::unique_lock lock(m_mutex);
				m_conditionVariable.wait_for(lock, m_timerResolution);

				TimerJob job;
				TickTime_t current = GET_TICK();
				while (m_timerJobs.try_pop(job))
				{
					if (job.expireTickTime > current)
					{
						m_timerJobs.push(job);
						break;
					}

					m_onTimeHandler(job.identity);
				}
			}
			m_onTimeHandler = nullptr;
		});
}

void TimerTickerImpl::Shutdown()
{
	m_shutdown = true;

	m_conditionVariable.notify_all();

	m_tickThread.join();
	m_timerJobs.clear();
}


TimerIdentity_t TimerTickerImpl::AllocTimerIdentity()
{
	if (m_shutdown)
	{
		return INVALID_TIMER_IDENTITY;
	}

	auto identity = (++m_sequence);
	if (identity == INVALID_TIMER_IDENTITY)
	{
		identity = (++m_sequence);
	}
	return identity;
}

void TimerTickerImpl::RegisterTimer(const TimerIdentity_t& _identity, const TickTime_t& _elapsedTickTime)
{
	if (m_shutdown || _identity == INVALID_TIMER_IDENTITY)
	{
		return;
	}

	m_timerJobs.push(TimerJob{ _elapsedTickTime + GET_TICK(), _identity });

	return;
}

// ------------------------------------------------------------------------------------------

Timer::Timer(const std::string& _configFilePath)
	: Module(_configFilePath)
{
	m_accessor = TimerAccessorImpl::Create(this);
}

Timer::~Timer()
{
}

Result Timer::InitImpl()
{
	return EError::Success;
}

void Timer::Shutdown()
{
	m_shutdown = true;

	decltype(m_tickers) temp;
	{
		SCOPED_WRITE_LOCK(m_mutex);
		temp = std::move(m_tickers);
	}

	for (auto& ticker : temp)
	{
		TimerTickerImpl* impl = static_cast<TimerTickerImpl*>(ticker.get());
		impl->Shutdown();
	}

	temp.clear();
}

TimerTickerShared_t Timer::AllocTimerTicker(const std::chrono::milliseconds& _timerResolution, OnTimeHandler_t&& _onTimeHandler)
{
	if (m_shutdown)
	{
		return nullptr;
	}

	auto ticker = TimerTickerImpl::Create(_timerResolution, std::move(_onTimeHandler));

	{
		SCOPED_WRITE_LOCK(m_mutex);
		m_tickers.emplace_back(ticker);
	}

	return ticker;
}