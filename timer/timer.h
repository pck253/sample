#pragma once

static_assert(TIMER_MODULE == 1);


class Timer : public Module
{
    friend Module;
public:
    explicit Timer(const std::string& _configFilePath);
    ~Timer();

    virtual bool IsBusinessModule() final { return false; }
    virtual EModule GetModuleType() final { return EModule::Timer; }

    virtual void Shutdown() final;

    TimerJobManagerAllocator& GetTimerJobManagerAllocator() { return m_timerJobManagerAllocator; }

private:
    virtual Result InitImpl() final;

private:
    TimerJobManagerAllocator m_timerJobManagerAllocator;
};