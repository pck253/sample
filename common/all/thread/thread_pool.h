#pragma once

class ThreadPool
{
public:
	using Job_t = std::function<void()>;

	ThreadPool()
	{
	}
	virtual ~ThreadPool()
	{
	}

	void Init(const uint16_t& _threadCount)
	{
		for (uint16_t i = 0; i < _threadCount; ++i)
		{
			m_threads.emplace_back([this]()
				{
					while (!m_stopThread)
					{
						Job_t job;
						{
							std::unique_lock<std::mutex> lock(m_mutex);
							m_conditionVariable.wait(lock, [this]() {
									// here is before wait. so, already own lock.
									return (m_stopThread || !m_jobs.empty());
								});

							if (m_jobs.empty())
							{
								continue;
							}
							
							m_jobs.try_pop(job);
						}

						if (job)
						{
							job();
						}
					}
					Log("terminate thread in pool");
				});
		}
	}

	void Shutdown()
	{
		m_shutdown = true;

		m_stopThread = true;
		m_conditionVariable.notify_all();
		for (auto& thread : m_threads)
		{
			thread.join();
		}

		if (!m_jobs.empty())
		{
			m_jobs.clear();
		}
	}

	void PushJob(Job_t&& _newJob, const bool& _notifyAll = false)
	{
		if (m_shutdown)
		{
			return;
		}

		m_jobs.push(std::move(_newJob));

		if (_notifyAll)
		{
			m_conditionVariable.notify_all();
		}
		else
		{
			m_conditionVariable.notify_one();
		}
	}

	bool IsEmpty()
	{
		return m_jobs.empty();
	}

private:
	std::atomic_bool m_shutdown = false;

	std::mutex m_mutex;
	std::condition_variable m_conditionVariable;
	our::vector<std::thread> m_threads;
	std::atomic_bool m_stopThread = false;

	Concurrency::concurrent_queue<Job_t> m_jobs;
};