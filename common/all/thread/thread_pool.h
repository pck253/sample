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
		m_beforeShutdown = [this](const EShutdownMode _reqShutdownMode)
			{
				switch (_reqShutdownMode)
				{
				case EShutdownMode::RightNow:
					break;
				case EShutdownMode::EmptyJob:
					{
						while(not m_jobs.empty())
						{
							Log("{} Shutdown : waiting until thread pool is empty.", m_name);
						}
					}
					break;
				case EShutdownMode::CurrentJob:
					{
						LogError("thread pool not support ShutdownMode::CurrentJob.");
					}
					return Result(EError::NotSupportShutdownMode);
				default:
					{
						LogError("wrong ShutdownMode!");
					}
					return Result(EError::NotSupportShutdownMode);
				}
				return Result();
			};

		m_afterShutdown = [this]()
			{
				for (auto& thread : m_threads)
				{
					// If the thread is currently executing a job, it does not recognize 'nodify_all'.
					//   - After job execution is complete, if empty job, thread is blocked at 'm_jobCount.wait'.
					// so, push dummy job
					//   - if thread execute, while loop is break because of "IsShutdown()"
					//   - if thread block at 'm_jobCount.wait', thread wakeup by 'nodify_all'.
					m_jobs.push(new JobWrapper([] {}));
					m_jobCount.fetch_add(1, std::memory_order_release);
				}

				m_jobCount.notify_all();

				for (auto& thread : m_threads)
				{
					thread.join();
				}
			};
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
					while (not IsShutdown())
					{
						IJobWrapper* job{};
						if (m_jobs.try_pop(job)) {
							m_jobCount.fetch_sub(1, std::memory_order_acquire);
							(*job)();
							delete job;
							continue;
						}

						auto const count{ m_jobCount.load(std::memory_order_acquire) };
						if (0 == count) {
							m_jobCount.wait(count);
						}
					}
					Log("terminate thread in pool");
				});
		}
	}

	template<typename T_FUNC>
	void PushJob(T_FUNC&& _newJob)
	{
		m_jobs.push(new JobWrapper(std::move(_newJob)));
		m_jobCount.fetch_add(1, std::memory_order_release);

		m_jobCount.notify_one();
	}

	void PushJob(IJobWrapper* _newJob)
	{
		if (nullptr == _newJob)
		{
			return;
		}

		m_jobs.push(_newJob);
		m_jobCount.fetch_add(1, std::memory_order_release);

		m_jobCount.notify_one();
	}

	template<typename T_FUNC>
	static JobInst_t MakeJobInst(T_FUNC&& _newJob)
	{
		return std::make_unique<JobWrapper<T_FUNC>>(std::move(_newJob));
	}

private:
	const std::string m_name;

	std::vector<std::thread> m_threads;
	concurrency::concurrent_queue<IJobWrapper*> m_jobs;
	std::atomic_int64_t m_jobCount{};
};