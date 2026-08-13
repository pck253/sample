#pragma once

template <typename T>
inline void SafeDelete(T*& _p)
{
	delete _p;
	_p = nullptr;
}

template <typename T>
inline void SafeArrayDelete(T*& _p)
{
	delete[] _p;
	_p = nullptr;
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(*(arr)))

// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

#define SCOPED_READ_LOCK(_sharedMutex) std::shared_lock<std::shared_mutex> readLock(_sharedMutex);
#define SCOPED_WRITE_LOCK(_sharedMutex) std::unique_lock<std::shared_mutex> writeLock(_sharedMutex);

// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

template <typename T_FUNC>
class FinalJob
{
public:
	FinalJob() = delete;
	FinalJob(T_FUNC&& _job) : m_job(std::move(_job)) {};
	virtual ~FinalJob() { m_job(); }

private:
	T_FUNC m_job;
};

// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

class CaseMaker
{
public:
	CaseMaker() = default;
	~CaseMaker() = default;

	static void MakeCase(const int32_t& number, std::list<std::vector<int>>& results)
	{
		int seed = number;
		while (0 < seed)
		{
			auto const remain = number - seed;

			if (0 == remain)
			{
				results.emplace_back(std::vector{ seed });
			}
			else if (1 == seed)
			{
				std::vector<int> temp(number, 1);
				results.emplace_back(std::move(temp));
			}
			else
			{
				std::list<std::vector<int>> tempResults;
				MakeCase(remain, tempResults);
				for (const auto& r : tempResults)
				{
					if (seed < r.front())
					{
						continue;
					}
					results.emplace_back(std::vector{ seed });
					results.back().append_range(r);
				}
			}
			--seed;
		}
	}
};

// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------