#pragma once

#define ALIGNMENT 8
#define POOL_COUNT (64 * 1024 / ALIGNMENT)	// 8B ~ 64KB
#define ADJUST_SIZE(s) ((s % ALIGNMENT != 0) ? (((s / ALIGNMENT) + 1) * ALIGNMENT) : s)
#define MAKE_POOL_INDEX(ajs) ((ajs / ALIGNMENT) - 1)

class MemoryPool
{
public:
	MemoryPool()
	{

	}
	~MemoryPool()
	{
		auto threadCount = std::thread::hardware_concurrency();

		auto offset = POOL_COUNT / threadCount;
		std::vector<std::thread> freeThreads;
		for (decltype(threadCount) i = 0; i < threadCount; ++i)
		{
			if (i + 1 == threadCount && POOL_COUNT % threadCount == 0)
			{
				offset += (POOL_COUNT % threadCount);
			}
			freeThreads.emplace_back([this, threadIndex = i, offset]()
				{
					auto start = threadIndex * offset;
					auto end = (std::min)(start + offset, (decltype(start))POOL_COUNT);
					for (; start < end; ++start)
					{
						uint8_t* ptr = nullptr;
						while (m_pools[start].try_pop(ptr))
						{
							free(ptr);
						}
					}
				});
		}

		for (auto& th : freeThreads)
		{
			th.join();
		}
	}

	uint8_t* allocate(const size_t _size)
	{
		uint8_t* ptr = nullptr;
		if (_size > 0)
		{
			auto adjustSize = ADJUST_SIZE(_size);
			auto index = MAKE_POOL_INDEX(adjustSize);
			if (index < POOL_COUNT)
			{
				if (!m_pools[index].try_pop(ptr))
				{
					ptr = static_cast<uint8_t*>(malloc(adjustSize));
				}
			}
			else
			{
				// too big
				ptr = static_cast<uint8_t*>(malloc(_size));
			}
		}
		return ptr;
	}

	void deallocate(uint8_t* const _ptr, const size_t _size)
	{
		if (_ptr && _size > 0)
		{
			auto adjustSize = ADJUST_SIZE(_size);
			auto index = MAKE_POOL_INDEX(adjustSize);
			if (index < POOL_COUNT)
			{
				m_pools[index].push(_ptr);
			}
			else
			{
				// too big
				free(_ptr);
			}
		}
	}

private:
	Concurrency::concurrent_queue<uint8_t*> m_pools[POOL_COUNT];	// 8B ~ 64KB
};

extern MemoryPool* g_memoryPool;