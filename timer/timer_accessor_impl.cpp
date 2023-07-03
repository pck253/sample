#include "pch.h"

TimerTickerShared_t TimerAccessorImpl::AllocTimerTicker(const std::chrono::milliseconds& _timerResolution, OnTimeHandler_t&& _onTimeHandler)
{
	return m_timerModule.AllocTimerTicker(_timerResolution, std::move(_onTimeHandler));
}