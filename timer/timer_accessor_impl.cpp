#include "pch.h"

static_assert(TIMER_MODULE == 1);

TimerJobManagerShared_t TimerAccessorImpl::AllocateTimerJobManager(ThreadPool& _threadPool)
{
	// std::chrono::milliseconds(16) : temp value
	return m_timerModule.GetTimerJobManagerAllocator().AllocTimerJobManager(std::chrono::milliseconds(16), _threadPool)->Get();
}