#pragma once

enum class EShutdownState : uint8_t
{
	Running = 0,
	Requested,
	Stopping,		// before shutdown is done. thread can be terminated
	Completed,
};

class ShutdownCoordinator
{
public:
	enum class EStepResult : uint8_t
	{
		Done = 0,
		Wait,		// not finished yet. the step is called again
	};
	using Step_t = std::function<EStepResult()>;

	// called only when the waiting log is about to be written
	using WaitLogMaker_t = std::function<std::string()>;

private:
	// reference_wrapper : a target always has a state, but m_currentTarget is rebound
	struct Target_t
	{
		std::reference_wrapper<std::atomic<EShutdownState>> state;
		const char* msg = nullptr;
	};

public:

	// setters nest : a target registers its child's steps in the middle of its own.
	// (world -> tick scheduler, allocator -> managers, connection manager -> thread pool)
	// the previous is restored so that the steps pushed after the child still
	// belong to the parent. resetting to the dummy instead would leave the parent
	// without a step of its own, and it would never reach Completed.
	class TargetSetter
	{
	public:
		TargetSetter(ShutdownCoordinator& _coordinator, std::atomic<EShutdownState>& _state, const char* _msg)
			: m_coordinatorRef(_coordinator)
			, m_previousTarget(_coordinator.m_currentTarget)
			, m_previousPushedState(_coordinator.m_currentPushedState)
		{
			m_coordinatorRef.m_currentTarget = Target_t{ _state, _msg };
			m_coordinatorRef.m_currentPushedState = EShutdownState::Requested;
		}
		~TargetSetter()
		{
			// a target without a step would never reach Completed
			if (!m_coordinatorRef.HasRemainStep(m_coordinatorRef.m_currentTarget))
			{
				m_coordinatorRef.Push("completed", []() { return EStepResult::Done; });
			}

			m_coordinatorRef.m_currentTarget = m_previousTarget;
			m_coordinatorRef.m_currentPushedState = m_previousPushedState;
		}

	private:
		ShutdownCoordinator& m_coordinatorRef;
		Target_t m_previousTarget;
		EShutdownState m_previousPushedState;
	};

	ShutdownCoordinator()
		: m_currentTarget{ m_dummyState, nullptr }
	{
	}

	// when EShutdownState::Requested
	void Push(std::string _name, Step_t&& _step, WaitLogMaker_t&& _waitLogMaker = nullptr)
	{
		PushStep(std::move(_name), std::move(_step), std::move(_waitLogMaker), EShutdownState::Requested);
	}

	// when EShutdownState::Stopping
	void PushStopping(std::string _name, Step_t&& _step, WaitLogMaker_t&& _waitLogMaker = nullptr)
	{
		PushStep(std::move(_name), std::move(_step), std::move(_waitLogMaker), EShutdownState::Stopping);
	}

	// must not run on a thread owned by the objects being shut down. (self join)
	void Run()
	{
		SteadyTime_t nextLogTime{};
		while (!m_steps.empty())
		{
			auto& front = m_steps.front();

			EnterState(front);

			auto result = EStepResult::Done;
			try
			{
				result = front.step();
			}
			catch (const std::exception& _ex)
			{
				LogError("shutdown step failed - {} : {}", front.name, _ex.what());
			}

			if (EStepResult::Wait == result)
			{
				const auto current = GetSteadyTime();
				if (0 == nextLogTime)
				{
					// a step that waits briefly is normal. only report the ones that hang on
					nextLogTime = current + WAIT_LOG_INTERVAL_MS;
				}
				else if (current >= nextLogTime)
				{
					nextLogTime = current + WAIT_LOG_INTERVAL_MS;
					Log("shutdown : waiting - {} {}", front.name, front.waitLogMaker ? front.waitLogMaker() : std::string());
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_INTERVAL_MS));
				continue;
			}

			const Target_t target = front.target;
			m_steps.pop_front();
			nextLogTime = {};

			CompleteTarget(target);
		}
	}

private:
	static constexpr SteadyTime_t RETRY_INTERVAL_MS = 1;
	static constexpr SteadyTime_t WAIT_LOG_INTERVAL_MS = 1000;

	struct NamedStep_t
	{
		NamedStep_t(std::string&& _name, Step_t&& _step, WaitLogMaker_t&& _waitLogMaker, const Target_t& _target, const EShutdownState& _state)
			: name(std::move(_name)), step(std::move(_step)), waitLogMaker(std::move(_waitLogMaker)), target(_target), state(_state)
		{
		}

		std::string name;
		Step_t step;
		WaitLogMaker_t waitLogMaker;

		Target_t target;
		EShutdownState state;	// the target is moved to this before the step runs
	};

	// the state only moves forward, so a step registered below the state already
	// pushed for this target would run after its threads were let go.
	void PushStep(std::string&& _name, Step_t&& _step, WaitLogMaker_t&& _waitLogMaker, const EShutdownState& _state)
	{
		if (m_currentPushedState < _state)
		{
			m_currentPushedState = _state;
		}
		else if (_state < m_currentPushedState)
		{
			LogError("shutdown step is registered out of order - {} : {} after {}",
				_name, magic_enum::enum_name(_state), magic_enum::enum_name(m_currentPushedState));
			assert(!"shutdown step is registered out of order");
		}

		m_steps.emplace_back(std::move(_name), std::move(_step), std::move(_waitLogMaker), m_currentTarget, m_currentPushedState);
	}

	bool HasRemainStep(const Target_t& _target) const
	{
		for (const auto& step : m_steps)
		{
			if (&step.target.state.get() == &_target.state.get())
			{
				return true;
			}
		}

		return false;
	}

	void EnterState(const NamedStep_t& _step)
	{
		auto& state = _step.target.state.get();
		if (state.load(std::memory_order_acquire) < _step.state)
		{
			state.store(_step.state, std::memory_order_release);
		}
	}

	void CompleteTarget(const Target_t& _target)
	{
		if (HasRemainStep(_target))
		{
			return;
		}

		_target.state.get().store(EShutdownState::Completed, std::memory_order_release);
		if (_target.msg)
		{
			Log("Shutdown : {}", _target.msg);
		}
	}

	// the steps pushed outside a TargetSetter have no state to move. nobody reads this
	std::atomic<EShutdownState> m_dummyState{ EShutdownState::Running };

	Target_t m_currentTarget;
	EShutdownState m_currentPushedState = EShutdownState::Requested;

	std::deque<NamedStep_t> m_steps;
};
