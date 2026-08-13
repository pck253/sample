#pragma once

class ThreadPool final : public UseShutdown
{
public:
	using Super_t = UseShutdown;

	class IJobWrapper : public MemPoolInstance
	{
	public:
		IJobWrapper() = default;
		virtual ~IJobWrapper() = default;
		virtual void operator ()() = 0;
	};

	template<typename T_FUNC>
	class JobWrapper final : public IJobWrapper
	{
	public:
		JobWrapper() = delete;
		JobWrapper(T_FUNC&& _job) : m_job(std::move(_job)) {}
		~JobWrapper() = default;

		void operator ()() override
		{
			m_job();
		}
	private:
		T_FUNC m_job;
	};
	using JobInst_t = std::unique_ptr<IJobWrapper>;

	ThreadPool(const std::string& _name)
		: m_name(_name)
	{
	}
	~ThreadPool()
	{
		IJobWrapper* job{};
		while (m_jobs.try_pop(job))
		{
			delete job;
			job = {};
		}
	}

	void Init(const uint16_t& _threadCount)
	{
		for (uint16_t i = 0; i < _threadCount; ++i)
		{
			m_threads.emplace_back([this]()
				{
					while (true)
					{
						m_sem.acquire();

						IJobWrapper* job{};
						if (!m_jobs.try_pop(job))
						{
							continue;
						}

						(*job)();
						delete job;

						if (IsShutdownStopping())
						{
							break;
						}
					}
					Log("terminate thread in pool");
				});
		}
	}

	template<typename T_FUNC>
	void PushJob(T_FUNC&& _newJob)
	{
		static_assert(!std::is_lvalue_reference_v<T_FUNC>, "PushJob only accepts rvalue callables");

		m_jobs.push(new JobWrapper(std::move(_newJob)));
		m_sem.release(1);
	}

	void PushJob(IJobWrapper* _newJob)
	{
		if (nullptr == _newJob)
		{
			return;
		}

		m_jobs.push(_newJob);
		m_sem.release(1);
	}

	template<typename T_FUNC>
	static JobInst_t MakeJobInst(T_FUNC&& _newJob)
	{
		static_assert(!std::is_lvalue_reference_v<T_FUNC>, "PushJob only accepts rvalue callables");

		return std::make_unique<JobWrapper<T_FUNC>>(std::move(_newJob));
	}

	uint16_t ThreadCount() const { return static_cast<uint16_t>(m_threads.size()); }

protected:
	virtual void RegisterShutdownSteps(ShutdownCoordinator& _coordinator) override
	{
		_coordinator.Push(m_name + " drain",
			[this]()
			{
				return m_jobs.empty() ? EStepResult::Done : EStepResult::Wait;
			},
			[this]() { return std::format("remain={}", m_jobs.unsafe_size()); });

		_coordinator.PushStopping(m_name + " release threads", [this]()
			{
				// if thread currently processing a task,
				//   - It is terminating due to IsShutdownStopping().
				//   - The added job is deleted in the destructor.
				// else
				//   - After passing through the semaphore, the job is processed and then terminates due to IsShutdownStopping().
				for (auto& thread : m_threads)
				{
					m_jobs.push(new JobWrapper([] {}));
				}
				m_sem.release(m_threads.size());

				return EStepResult::Done;
			});

		_coordinator.PushStopping(m_name + " join threads", [this]()
			{
				for (auto& thread : m_threads)
				{
					if (thread.joinable())
					{
						thread.join();
					}
				}

				return EStepResult::Done;
			});
	}

private:
	const std::string m_name;

	std::vector<std::thread> m_threads;
	concurrency::concurrent_queue<IJobWrapper*> m_jobs;
	std::counting_semaphore<std::numeric_limits<std::ptrdiff_t>::max()> m_sem{ 0 };
};