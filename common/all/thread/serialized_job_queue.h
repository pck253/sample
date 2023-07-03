#pragma once

extern thread_local SerializedJobQueueShared_t g_currentSerializedJobQueue;

class SerializedJobQueue : public std::enable_shared_from_this<SerializedJobQueue>
{
public:
    using Job_t = std::function<void()>;

    template<class CT = SerializedJobQueue> requires std::derived_from<CT, SerializedJobQueue>
    static std::shared_ptr<CT> Create(ThreadPool& _threadPool)
    {
        return std::shared_ptr<CT>(new CT(_threadPool));
    }

    virtual ~SerializedJobQueue()
    {
        if (!m_jobQueue.empty())
        {
            m_jobQueue.clear();
        }
    }

    template<class T = SerializedJobQueue> requires std::derived_from<T, SerializedJobQueue>
    std::shared_ptr<T> Get()
    {
        auto self = this->shared_from_this();
        return std::shared_ptr<T>(std::move(self), static_cast<T*>(self.get()));
    }

    inline void Shutdown() { m_shutdown = true; }
    inline bool IsShutdown() { return m_shutdown; }

    void SetOnEmptyJob(std::function<void()>&& _onEmptyJob) { m_onEmptyJob = std::move(_onEmptyJob); }

    void PushJob(Job_t&& _job)
    {
        if (m_shutdown)
        {
            return;
        }
        m_jobQueue.push(std::move(_job));

        auto old = m_jobCount.fetch_add(1);
        if (old == 0)
        {
            if (m_pause)
            {
                return;
            }
            m_threadPool.PushJob([_self = Get()]()
            {
                _self->ProcessJob();
            });
        }
    }

    void ProcessJob()
    {
        auto self = Get();

        Job_t job;
        if (m_jobQueue.try_pop(job))
        {
            g_currentSerializedJobQueue = self;
            job();
            g_currentSerializedJobQueue.reset();
        }

        auto old = m_jobCount.fetch_add(-1);
        if (old != 1)   // if old is 1, current is 0.
        {
            if (m_pause)
            {
                return;
            }
            m_threadPool.PushJob([self]()
            {
                    self->ProcessJob();
            });
        }
        else if (m_onEmptyJob)
        {
            m_onEmptyJob();
        }
    }

    void Pause()
    {
        // this function is called in SerializedJobQueue::ProcessJob
        bool expect = false;
        if (this != g_currentSerializedJobQueue.get() || !m_pause.compare_exchange_strong(expect, true))
        {
            assert(this == g_currentSerializedJobQueue.get());
            return;
        }

        // don't push calling SerializedJobQueue::ProcessJob job from SerializedJobQueue::PushJob called by other threads.
        m_jobQueue.push([]() {});
        m_jobCount.fetch_add(1);
    }

    void Resume()
    {
        bool expect = true;
        if (m_pause.compare_exchange_strong(expect, false))
        {
            m_threadPool.PushJob([_self = Get()]()
                {
                    _self->ProcessJob();
                });
        }
    }

protected:
    SerializedJobQueue(ThreadPool& _threadPool)
        : m_threadPool(_threadPool) {}

    std::function<void()> m_onEmptyJob;

    std::atomic_bool m_shutdown = false;
    ThreadPool& m_threadPool;

    std::atomic_bool m_pause = false;

    std::atomic_int32_t m_jobCount = 0;
    Concurrency::concurrent_queue<Job_t> m_jobQueue;
};