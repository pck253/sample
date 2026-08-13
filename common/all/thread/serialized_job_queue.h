#pragma once

inline thread_local SerializedJobQueue* g_currentSerializedJobQueue = nullptr;

class SerializedJobQueue : public std::enable_shared_from_this<SerializedJobQueue>
{
public:
    class IJobWrapper : public MemPoolInstance
    {
    public:
        IJobWrapper() = default;
        virtual ~IJobWrapper() = default;
        virtual void operator ()(const std::shared_ptr<SerializedJobQueue>& _jobQueue) = 0;
    };

    template<typename T_FUNC>
    class JobWrapper final : public IJobWrapper
    {
    public:
        JobWrapper() = delete;
        JobWrapper(T_FUNC&& _job) : m_job(std::move(_job)) {}
        ~JobWrapper() = default;

        void operator()(const std::shared_ptr<SerializedJobQueue>& _jobQueue) override
        {
            using DecayedFunc_t = std::remove_reference_t<T_FUNC>;
            using ArgTypes_t = typename FunctionTraits<decltype(&DecayedFunc_t::operator())>::ArgTypes;

            constexpr size_t count = std::tuple_size_v<ArgTypes_t>;
            if constexpr (1 == count)
            {
                //using Target_t = typename ExtractSharedPtrInner<std::tuple_element_t<0, ArgTypes_t>>::Type;
                using Target_t = std::tuple_element_t<0, ArgTypes_t>;
                if constexpr (std::is_same_v<Target_t, SerializedJobQueue>)
                {
                    m_job(*_jobQueue.get());
                }
                else if constexpr (std::derived_from<Target_t, SerializedJobQueue>)
                {
                    m_job(_jobQueue->As<Target_t>());
                }
                else
                {
                    static_assert(0 == sizeof(T_FUNC));
                }
            }
            else
            {
                static_assert(0 == sizeof(T_FUNC));
            }
        }
    private:
        T_FUNC m_job;
    };

    template<class T = SerializedJobQueue, typename... T_ARGS> requires std::derived_from<T, SerializedJobQueue>
    static std::shared_ptr<T> Create(ThreadPool& _threadPool, T_ARGS&& ... _args)
    {
        auto instance = std::shared_ptr<T>(new T(_threadPool, std::forward<T_ARGS>(_args)...));
        instance->m_isFromCreateFunc = true;
        return instance;
    }

    virtual ~SerializedJobQueue()
    {
        if (!m_isFromCreateFunc)
        {
            LogWarning("this was not created from \"Create\" function.");
        }

        if (!m_pushStopped.load(std::memory_order_acquire))
        {
            LogError("destroyed. push is not stopped.");
            assert(m_pushStopped.load(std::memory_order_acquire));
        }

        IJobWrapper* job{};
        while (m_jobQueue.try_pop(job))
        {
            delete job;
        }
    }

    void StopPush(const char* _msg = nullptr)
    {
        bool expected = false;
        if (!m_pushStopped.compare_exchange_strong(expected, true))
        {
            LogWarning("StopPush is ignored. already stopped : {}", _msg ? _msg : "no message");
            return;
        }

        if (_msg)
        {
            Log("StopPush : {}", _msg);
        }
    }

    inline bool IsPushStopped() const { return m_pushStopped.load(); }

    template<class T = SerializedJobQueue> requires std::derived_from<T, SerializedJobQueue>
    std::shared_ptr<T> Get()
    {
        if constexpr (std::is_same<T, SerializedJobQueue>::value)
        {
            return this->shared_from_this();
        }
        return std::dynamic_pointer_cast<T>(this->shared_from_this());
    }

    template<class T = SerializedJobQueue> requires std::derived_from<T, SerializedJobQueue>
    const T& As() const
    {
        return dynamic_cast<const T&>(*this);
    }

    template<class T = SerializedJobQueue> requires std::derived_from<T, SerializedJobQueue>
    T& As()
    {
        return dynamic_cast<T&>(*this);
    }

    template<typename T_FUNC>
    void PushJob(T_FUNC&& _job)
    {
        static_assert(!std::is_lvalue_reference_v<T_FUNC>, "PushJob only accepts rvalue callables");

        if (IsPushStopped())
        {
            return;
        }

        // possible call PushJob after StopPush.
        // but, delete remain job in destructor.
        m_jobQueue.push(new JobWrapper(std::move(_job)));

        const auto old = m_jobCount.fetch_add(1);
        if (0 == old)
        {
            m_threadPoolRef.PushJob([_self = Get()]()
            {
                _self->ProcessJob();
            });
        }
    }

    void ProcessJob()
    {
        g_currentSerializedJobQueue = this;
        const auto count{ m_jobCount.load() };
        auto self{ Get() };

        int32_t processed{};
        IJobWrapper* job{};
        for (const auto i : std::ranges::iota_view(0, count))
        {
            if (not m_jobQueue.try_pop(job))
            {
                continue;
            }
            ++processed;

            (*job)(self);
            delete job;
        }
        const auto old{ m_jobCount.fetch_add(-processed) };

        if (old != count)   // if old is equal count, current job count is 0 after fetach_add
        {
            m_threadPoolRef.PushJob([jobQueue = std::move(self)]()
                {
                    jobQueue->ProcessJob();
                });
        }
        g_currentSerializedJobQueue = nullptr;
    }

protected:
    explicit SerializedJobQueue(ThreadPool& _threadPool) : m_threadPoolRef(_threadPool) {}

    bool CheckInProcess() const { return (this == g_currentSerializedJobQueue); }

protected:
    bool m_isFromCreateFunc{};
    std::atomic_bool m_pushStopped{ false };

    ThreadPool& m_threadPoolRef;

    std::atomic_int32_t m_jobCount{ 0 };
    Concurrency::concurrent_queue<IJobWrapper*> m_jobQueue;
};