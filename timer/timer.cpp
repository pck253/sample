#include "pch.h"

static_assert(TIMER_MODULE == 1);

MODULE_STATIC_IMPL(Timer);

Timer::Timer(Application& _application, const std::string& _configFilePath)
	: Module(_application, _configFilePath)
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
	m_timerJobManagerAllocator.Shutdown(m_shutdownCoordinator, "timer ticker allocator shutdown.");

	m_shutdownCoordinator.Run();
}