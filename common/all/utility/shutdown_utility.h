#pragma once

class UseShutdown
{
public:
	UseShutdown() = default;
	virtual ~UseShutdown()
	{
		const auto state = m_shutdownState.load(std::memory_order_acquire);
		if (EShutdownState::Completed != state)
		{
			LogError("destroyed. shutdown is not completed : {}", magic_enum::enum_name(state));
			assert(EShutdownState::Completed == state);
		}
	}

	void Shutdown(ShutdownCoordinator& _coordinator, const char* _msg = nullptr)
	{
		if (!EnterShutdown(_msg))
		{
			return;
		}

		ShutdownCoordinator::TargetSetter targetSetter(_coordinator, m_shutdownState, _msg);
		RegisterShutdownSteps(_coordinator);
	}

	inline bool IsShutdownStarted() const { return m_shutdownState.load() != EShutdownState::Running; }
	inline bool IsShutdownStopping() const { return m_shutdownState.load(std::memory_order_acquire) >= EShutdownState::Stopping; }

protected:
	using EStepResult = ShutdownCoordinator::EStepResult;

	virtual void RegisterShutdownSteps(ShutdownCoordinator& _coordinator) {}

	// the CAS pairs with IsShutdownStarted() on the other side. both must stay seq_cst.
	bool EnterShutdown(const char* _msg)
	{
		auto expectedState = EShutdownState::Running;
		if (!m_shutdownState.compare_exchange_strong(expectedState, EShutdownState::Requested))
		{
			LogWarning("Shutdown is ignored. already {} : {}", magic_enum::enum_name(expectedState), _msg ? _msg : "no message");
			return false;
		}

		return true;
	}

protected:
	std::atomic<EShutdownState> m_shutdownState{ EShutdownState::Running };
};
