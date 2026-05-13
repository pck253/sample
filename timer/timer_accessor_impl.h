#pragma once

static_assert(TIMER_MODULE == 1);

class Timer;
class TimerAccessorImpl : public TimerAccessor
{
public:
	static TimerAccessorImpl* Create(Timer* _timerModule)
	{
		return new TimerAccessorImpl(_timerModule);
	}
	~TimerAccessorImpl() = default;

	virtual TimerJobManagerShared_t AllocateTimerJobManager(ThreadPool& _threadPool) final;

private:
	explicit TimerAccessorImpl(Timer* _timerModule) : m_timerModule(*_timerModule) {};

	Timer& m_timerModule;
};