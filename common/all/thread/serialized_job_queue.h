#pragma once

class SerializedJobQueue : public std::enable_shared_from_this<SerializedJobQueue>, public UseShutdown
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
                    static_assert(false);
                }
            }
            else
            {
                static_assert(false);
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
        assert(IsShutdown());

        if (!m_isFromCreateFunc)
        {
            LogWarning("this was not created from \"Create\" function.");
        }

        IJobWrapper* job{};
        while (m_jobQueue.try_pop(job))
        {
            delete job;
        }
    }

    auto& GetThreadPool() { return m_threadPoolRef; }

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

    void SetOnEmptyJob(std::function<void()>&& _onEmptyJob) { m_onEmptyJob = std::move(_onEmptyJob); }

    template<typename T_FUNC>
    void PushJob(T_FUNC&& _job)
    {
        // possible call PushJob after Shutdown.
        // but, delete remain job in destructor.
        m_jobQueue.push(new JobWrapper(std::move(_job)));

        auto old = m_jobCount.fetch_add(1);
        if (old == 0)
        {
            m_threadPoolRef.PushJob([_self = Get()]()
            {
                _self->ProcessJob();
            });
        }
    }

    void PushJob(IJobWrapper* _job)
    {
        if (nullptr == _job)
        {
            return;
        }
        // possible call PushJob after Shutdown.
        // but, delete remain job in destructor.
        m_jobQueue.push(_job);

        auto old = m_jobCount.fetch_add(1);
        if (old == 0)
        {
            m_threadPoolRef.PushJob([_self = Get()]()
                {
                    _self->ProcessJob();
                });
        }
    }

    void ProcessJob()
    {
        const auto shutdownMode{ m_shutdownMode.load()};
        if (EShutdownMode::RightNow == shutdownMode || m_isStopProcessJob)
        {
            return;
        }

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

        m_isStopProcessJob = (EShutdownMode::CurrentJob == shutdownMode);
        if (m_isStopProcessJob)
        {
            return;
        }

        if (old != count)   // if old is count, current is 0.
        {
            m_threadPoolRef.PushJob([jobQueue = std::move(self)]()
                {
                    jobQueue->ProcessJob();
                });
            return;
        }

        m_isStopProcessJob = (EShutdownMode::EmptyJob == shutdownMode);
        if (m_onEmptyJob)
        {
            m_onEmptyJob();
        }
    }

protected:
    explicit SerializedJobQueue(ThreadPool& _threadPool) : m_threadPoolRef(_threadPool) {}

    bool m_isFromCreateFunc{};

    std::function<void()> m_onEmptyJob;

    ThreadPool& m_threadPoolRef;

    std::atomic_int32_t m_jobCount{ 0 };
    Concurrency::concurrent_queue<IJobWrapper*> m_jobQueue;

    bool m_isStopProcessJob{};
};