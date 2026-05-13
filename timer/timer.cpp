#include "pch.h"

static_assert(TIMER_MODULE == 1);

MODULE_STATIC_IMPL(Timer);

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
	m_timerJobManagerAllocator.Shutdown(EShutdownMode::RightNow, "timer ticker allocator shutdown.");
}