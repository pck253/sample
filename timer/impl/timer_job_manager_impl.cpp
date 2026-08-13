#include "pch.h"

static_assert(TIMER_MODULE == 1);

TimerJobManagerImpl::TimerJobManagerImpl(const std::chrono::milliseconds& _timerResolution, ThreadPool& _threadPoolRef)
	: m_timerResolution(_timerResolution), m_threadPoolRef(_threadPoolRef)
{
	m_tickThread = std::thread([this]()
		{
			auto waitDuration = m_timerResolution;
			while (!IsShutdownStarted())
			{
				std::ignore = m_tickSem.try_acquire_for(waitDuration);
				waitDuration = m_timerResolution;

				TimerJobShared_t job;
				SteadyTime_t current = GetSteadyTime();
				while (m_timerJobs.try_pop(job) && !IsShutdownStarted())
				{
					if (job->accessor->canceled)
					{
						continue;
					}
					if (job->expireTickTime > current)
					{
						m_timerJobs.push(job);
						waitDuration = std::chrono::milliseconds(job->expireTickTime - current);
						break;
					}

					OnTime(std::move(job));
				}
			}
		});
}

void TimerJobManagerImpl::RegisterShutdownSteps(ShutdownCoordinator& _coordinator)
{
	_coordinator.PushStopping("timer job manager release tick thread", [this]()
		{
			m_tickSem.release();

			return EStepResult::Done;
		});

	_coordinator.PushStopping("timer job manager join tick thread", [this]()
		{
			if (m_tickThread.joinable())
			{
				m_tickThread.join();
			}

			return EStepResult::Done;
		});
}

TimerJobAccessor_t TimerJobManagerImpl::PushTimerJob(ThreadPool::JobInst_t&& _jobInst, const SteadyTime_t& _elapsedTickTime, const ETimerJobRepeatMode& _repeatMode)
{
	return PushTimerJobImpl(std::move(_jobInst), _elapsedTickTime, "", _repeatMode);
}

TimerJobAccessor_t TimerJobManagerImpl::PushTimerJob(ThreadPool::JobInst_t&& _jobInst, const std::string& _cronString, const ETimerJobRepeatMode& _repeatMode)
{
	return PushTimerJobImpl(std::move(_jobInst), 0, _cronString, _repeatMode);
}

TimerJobAccessor_t TimerJobManagerImpl::PushTimerJobImpl(ThreadPool::JobInst_t&& _jobInst, const SteadyTime_t& _elapsedTickTime, const std::string& _cronString, const ETimerJobRepeatMode& _repeatMode)
{
	if (IsShutdownStarted())
	{
		return nullptr;
	}

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
		auto const nextTimeSec = cron::cron_next(timerJob->cronExpr, GetTime() / static_cast<SteadyTime_t>(TimeUnit::SEC_TO_MS));
		if (nextTimeSec == cron::INVALID_TIME)
		{
			return INVALID_TIMER_JOB_ACCESSOR;
		}
		timerJob->expireTickTime = (nextTimeSec * static_cast<SteadyTime_t>(TimeUnit::SEC_TO_MS));
	}
	else
	{
		timerJob->expireTickTime = _elapsedTickTime + GetSteadyTime();
	}

	m_timerJobs.push(timerJob);
	m_tickSem.release();

	return timerJob->accessor;
}

void TimerJobManagerImpl::RepeatTimerJob(const TimerJobShared_t& _timerJob, const SteadyTime_t& _nowTickTime)
{
	if (IsShutdownStarted())
	{
		return;
	}

	if (_timerJob->accessor->canceled)
	{
		return;
	}

	if (!cron::to_cronstr(_timerJob->cronExpr).empty())
	{
		auto const nextTimeSec = cron::cron_next(_timerJob->cronExpr, _nowTickTime / static_cast<SteadyTime_t>(TimeUnit::SEC_TO_MS));
		if (nextTimeSec == cron::INVALID_TIME)
		{
			return;
		}
		_timerJob->expireTickTime = (nextTimeSec * static_cast<SteadyTime_t>(TimeUnit::SEC_TO_MS));
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
		self->RepeatTimerJob(_timerJob, GetSteadyTime());
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
					self->RepeatTimerJob(timerJob, GetSteadyTime());
				}
				break;
			}
		};
	m_threadPoolRef.PushJob(std::move(job));
}

// ------------------------------------------------------------------------------------------

TimerJobManagerAllocator::TimerJobManagerAllocator()
{
}

void TimerJobManagerAllocator::RegisterShutdownSteps(ShutdownCoordinator& _coordinator)
{
	// AllocTimerJobManager checks the shutdown state under the same lock,
	// so anything not moved out here shuts itself down.
	decltype(m_timerJobManagers) targets;
	{
		SCOPED_WRITE_LOCK(m_mutex);
		targets = std::move(m_timerJobManagers);
	}

	for (auto& manager : targets)
	{
		manager->Shutdown(_coordinator, "timer job manager wait shutdown counter.");
	}

	_coordinator.Push("timer job manager allocator release", [targets = std::move(targets)]() mutable
		{
			targets.clear();

			return EStepResult::Done;
		});
}

TimerJobManagerImplShared_t TimerJobManagerAllocator::AllocTimerJobManager(const std::chrono::milliseconds& _timerResolution, ThreadPool& _threadPool)
{
	SCOPED_WRITE_LOCK(m_mutex);
	if (IsShutdownStarted())
	{
		return TimerJobManagerImplShared_t();
	}

	// create in the lock. a manager made outside could miss the registration
	// and would have to be shut down on the caller's thread.
	auto manager = TimerJobManagerImpl::Create(_timerResolution, _threadPool);
	m_timerJobManagers.emplace_back(manager);

	return manager;
}