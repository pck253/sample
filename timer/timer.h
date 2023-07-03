#pragma once

struct TimerJob
{
    TickTime_t expireTickTime = 0;
    TimerIdentity_t identity = INVALID_TIMER_IDENTITY;

    bool operator() (const TimerJob& _a, const TimerJob& _b)
    {
        return (_a.expireTickTime > _b.expireTickTime);
    }
};

// ------------------------------------------------------------------------------------------

class TimerTickerImpl : public TimerTicker
{
public:
    static TimerTickerShared_t Create(const std::chrono::milliseconds& _timerResolution, OnTimeHandler_t&& _onTimeHandler)
    {
        auto conn = TimerTickerShared_t(new TimerTickerImpl(_timerResolution, std::move(_onTimeHandler)));
        return conn;
    }
    virtual ~TimerTickerImpl() = default;

    void Shutdown();

    virtual TimerIdentity_t AllocTimerIdentity() final;
    virtual void RegisterTimer(const TimerIdentity_t& _identity, const TickTime_t& _elapsedTickTime) final;

private:
    TimerTickerImpl(const std::chrono::milliseconds& _timerResolution, OnTimeHandler_t&& _onTimeHandler);

    std::atomic_bool m_shutdown = false;

    OnTimeHandler_t m_onTimeHandler;

    std::atomic<TimerIdentity_t> m_sequence = 0;

    std::chrono::milliseconds m_timerResolution;
    std::mutex m_mutex;
    std::thread m_tickThread;
    std::condition_variable m_conditionVariable;

    Concurrency::concurrent_priority_queue<TimerJob, TimerJob> m_timerJobs;
};

// ------------------------------------------------------------------------------------------

class Timer : public Module
{
    friend Module;
public:
    ~Timer();

    virtual bool IsBusinessModule() final { return false; }
    virtual EModule GetModuleType() final { return EModule::Timer; }
    virtual const char* GetModuleName() final { return "Timer"; }

    virtual void Shutdown() final;

    TimerTickerShared_t AllocTimerTicker(const std::chrono::milliseconds& _timerResolution, OnTimeHandler_t&& _onTimeHandler);

private:
    Timer(const std::string& _configFilePath);

    virtual Result InitImpl() final;

private:
    std::atomic_bool m_shutdown = false;

    std::shared_mutex m_mutex;
    our::vector<TimerTickerShared_t> m_tickers;
};