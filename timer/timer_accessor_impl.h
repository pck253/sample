#pragma once

class Timer;
class TimerAccessorImpl : public TimerAccessor
{
public:
	static TimerAccessorImpl* Create(Timer* _timerModule)
	{
		return new TimerAccessorImpl(_timerModule);
	}
	~TimerAccessorImpl() = default;

	virtual TimerTickerShared_t AllocTimerTicker(const std::chrono::milliseconds& _timerResolution, OnTimeHandler_t&& _onTimeHandler) final;

private:
	TimerAccessorImpl(Timer* _timerModule) : m_timerModule(*_timerModule) {};

	Timer& m_timerModule;
};