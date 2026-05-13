#include "pch.h"

static_assert(TIMER_MODULE == 1);

TimerJobManagerImpl::TimerJobManagerImpl(const std::chrono::milliseconds& _timerResolution, ThreadPool& _threadPoolRef)
	: m_timerResolution(_timerResolution), m_threadPoolRef(_threadPoolRef)
{
	m_beforeShutdown = [this](const EShutdownMode _reqShutdownMode)
		{
			switch (_reqShutdownMode)
			{
			case EShutdownMode::RightNow:
				break;
			case EShutdownMode::EmptyJob:
			case EShutdownMode::CurrentJob:
				{
					LogError("timer job manager not support ShutdownMode::CurrentJob & EShutdownMode::EmptyJob.");
				}
				return Result(EError::NotSupportShutdownMode);
			default:
				{
					LogError("wrong ShutdownMode!");
				}
				return Result(EError::NotSupportShutdownMode);
			}
			return Result();
		};

	m_afterShutdown = [this]()
		{
			m_conditionVariable.notify_all();

			m_tickThread.join();
		};

	m_tickThread = std::thread([this]()
		{
			while (!IsShutdown())
			{
				std::unique_lock lock(m_conditionVariableMutex);
				m_conditionVariable.wait_for(lock, m_timerResolution);

				TimerJobShared_t job;
				TickTime_t current = GET_TICK();
				while (m_timerJobs.try_pop(job) && !IsShutdown())
				{
					if (job->accessor->canceled)
					{
						continue;
					}
					if (job->expireTickTime >= current)
					{
						m_timerJobs.push(job);
						break;
					}

					OnTime(std::move(job));
				}
			}
		});
}

TimerJobAccessor_t TimerJobManagerImpl::PushTimerJob(ThreadPool::JobInst_t&& _jobInst, const TickTime_t& _elapsedTickTime, const ETimerJobRepeatMode& _repeatMode)
{
	return PushTimerJobImpl(std::move(_jobInst), _elapsedTickTime, "", _repeatMode);
}

TimerJobAccessor_t TimerJobManagerImpl::PushTimerJob(ThreadPool::JobInst_t&& _jobInst, const std::string& _cronString, const ETimerJobRepeatMode& _repeatMode)
{
	return PushTimerJobImpl(std::move(_jobInst), 0, _cronString, _repeatMode);
}

TimerJobAccessor_t TimerJobManagerImpl::PushTimerJobImpl(ThreadPool::JobInst_t&& _jobInst, const TickTime_t& _elapsedTickTime, const std::string& _cronString, const ETimerJobRepeatMode& _repeatMode)
{
	TimerJobId_t identity = (++m_sequence);
	if (!identity())
	{
		identity = (++m_sequence);
	}

	auto accessor = TimerJobAccessor_t(new TimerJobAccessor(identity));
	auto timerJob = TimerJobShared_t(new TimerJob{ std::move(accessor),
												0,
												_elapsedTickTime,
												_cronString.empty() ? cron::cronexpr() : cron::make_cron(_cronString),
												std::move(_jobInst),
												_repeatMode});

	if (!_cronString.empty())
	{
		auto const nextTimeSec = cron::cron_next(timerJob->cronExpr, GET_TIME() / static_cast<TickTime_t>(TimeUnit::SecToMs));
		if (nextTimeSec == cron::INVALID_TIME)
		{
			return INVALID_TIMER_JOB_ACCESSOR;
		}
		timerJob->expireTickTime = (nextTimeSec * static_cast<TickTime_t>(TimeUnit::SecToMs));
	}
	else
	{
		timerJob->expireTickTime = _elapsedTickTime + GET_TICK();
	}

	m_timerJobs.push(timerJob);

	return timerJob->accessor;
}

void TimerJobManagerImpl::RepeatTimerJob(const TimerJobShared_t& _timerJob, const TickTime_t& _nowTickTime)
{
	if (_timerJob->accessor->canceled)
	{
		return;
	}

	if (!cron::to_cronstr(_timerJob->cronExpr).empty())
	{
		auto const nextTimeSec = cron::cron_next(_timerJob->cronExpr, _nowTickTime / static_cast<TickTime_t>(TimeUnit::SecToMs));
		if (nextTimeSec == cron::INVALID_TIME)
		{
			return;
		}
		_timerJob->expireTickTime = (nextTimeSec * static_cast<TickTime_t>(TimeUnit::SecToMs));
	}
	else
	{
		_timerJob->expireTickTime = _timerJob->_elapsedTickTime + _nowTickTime;
	}
	m_timerJobs.push(_timerJob);
}

void TimerJobManagerImpl::OnTime(TimerJobShared_t&& _timerJob)
{
	auto self = std::static_pointer_cast<TimerJobManagerImpl>(Get());

	if (ETimerJobRepeatMode::OnTime == _timerJob->repeatMode)
	{
		self->RepeatTimerJob(_timerJob, GET_TICK());
	}

	auto job = [timerJob = std::move(_timerJob), self]()
		{
			if (timerJob->accessor->canceled)
			{
				return;
			}

			switch (timerJob->repeatMode)
			{
			case ETimerJobRepeatMode::None:
			case ETimerJobRepeatMode::OnTime:
				{
					timerJob->jobInst->operator()();
				}
				break;
			case ETimerJobRepeatMode::AfterJob:
				{
					timerJob->jobInst->operator()();
					self->RepeatTimerJob(timerJob, GET_TICK());
				}
				break;
			}
		};
	m_threadPoolRef.PushJob(std::move(job));
}

// ------------------------------------------------------------------------------------------

TimerJobManagerAllocator::TimerJobManagerAllocator()
{
	m_afterShutdown = [this]()
		{
			decltype(m_timerJobManagers) temp;
			{
				SCOPED_WRITE_LOCK(m_mutex);
				temp = std::move(m_timerJobManagers);
			}

			for (auto& manager : temp)
			{
				manager->Shutdown(m_shutdownMode, "timer job manager wait shutdown counter.");
			}

			temp.clear();
		};
}

TimerJobManagerImplShared_t TimerJobManagerAllocator::AllocTimerJobManager(const std::chrono::milliseconds& _timerResolution, ThreadPool& _threadPool)
{
	if (IsShutdown())
	{
		return TimerJobManagerImplShared_t();
	}

	auto manager = TimerJobManagerImpl::Create(_timerResolution, _threadPool);

	{
		SCOPED_WRITE_LOCK(m_mutex);
		m_timerJobManagers.emplace_back(manager);
	}

	return manager;
}