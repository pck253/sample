#pragma once

static_assert(TIMER_MODULE == 1);

struct TimerJob
{
    TimerJobAccessor_t accessor;
    TickTime_t expireTickTime = 0;
    TickTime_t _elapsedTickTime = 0;
    cron::cronexpr cronExpr;
    ThreadPool::JobInst_t jobInst;
    ETimerJobRepeatMode repeatMode = ETimerJobRepeatMode::None;
};
using TimerJobShared_t = std::shared_ptr<TimerJob>;

struct TimerJobComp
{
    bool operator() (const TimerJobShared_t& _a, const TimerJobShared_t& _b)
    {
        return (_a->expireTickTime > _b->expireTickTime);
    }
};

// ------------------------------------------------------------------------------------------

class TimerJobManagerImpl;
using TimerJobManagerImplShared_t = std::shared_ptr<TimerJobManagerImpl>;

class TimerJobManagerImpl : public TimerJobManager
{
public:
    static TimerJobManagerImplShared_t Create(const std::chrono::milliseconds& _timerResolution, ThreadPool& _threadPoolRef)
    {
        auto ticker = TimerJobManagerImplShared_t(new TimerJobManagerImpl(_timerResolution, _threadPoolRef));
        return ticker;
    }

    ~TimerJobManagerImpl() = default;

    virtual TimerJobAccessor_t PushTimerJob(ThreadPool::JobInst_t&& _jobInst, const TickTime_t& _elapsedTickTime, const ETimerJobRepeatMode& _repeatMode = ETimerJobRepeatMode::None) final;
    virtual TimerJobAccessor_t PushTimerJob(ThreadPool::JobInst_t&& _jobInst, const std::string& _cronString, const ETimerJobRepeatMode& _repeatMode = ETimerJobRepeatMode::None) final;

private:
    TimerJobManagerImpl(const std::chrono::milliseconds& _timerResolution, ThreadPool& _threadPoolRef);

    TimerJobAccessor_t PushTimerJobImpl(ThreadPool::JobInst_t&& _jobInst, const TickTime_t& _elapsedTickTime, const std::string& _cronString, const ETimerJobRepeatMode& _repeatMode);
    void RepeatTimerJob(const TimerJobShared_t& _timerJob, const TickTime_t& _nowTickTime);

    void OnTime(TimerJobShared_t&& _job);

    std::atomic<TimerJobId_t::IdType> m_sequence{ TimerJobId_t::GetInvalidValue() };

    std::chrono::milliseconds m_timerResolution;
    std::thread m_tickThread;
    std::mutex m_conditionVariableMutex;
    std::condition_variable m_conditionVariable;

    ThreadPool& m_threadPoolRef;
    Concurrency::concurrent_priority_queue<TimerJobShared_t, TimerJobComp> m_timerJobs;
};

// ------------------------------------------------------------------------------------------
class TimerJobManagerAllocator : public UseShutdown
{
public:
    TimerJobManagerAllocator();
    ~TimerJobManagerAllocator() = default;

    TimerJobManagerImplShared_t AllocTimerJobManager(const std::chrono::milliseconds& _timerResolution, ThreadPool& _threadPool);

private:
    std::shared_mutex m_mutex;
    std::vector<TimerJobManagerImplShared_t> m_timerJobManagers;
};