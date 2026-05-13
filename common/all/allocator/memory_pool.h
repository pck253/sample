#pragma once

#define ALIGNMENT 8
#define POOL_COUNT (64 * 1024 / ALIGNMENT)	// 8B ~ 64KB
#define ADJUST_SIZE(s) ((s % ALIGNMENT != 0) ? (((s / ALIGNMENT) + 1) * ALIGNMENT) : s)
#define MAKE_POOL_INDEX(ajs) ((ajs / ALIGNMENT) - 1)

class MemoryPool final
{
public:
	MemoryPool()
	{

	}
	~MemoryPool()
	{
		const auto threadCount{ std::thread::hardware_concurrency() };

		auto offset{ POOL_COUNT / threadCount };
		std::vector<std::thread> freeThreads;
		decltype(offset) start{};
		for (std::remove_const_t<decltype(threadCount)> i{}; i < threadCount; ++i)
		{
			if (i + 1 == threadCount && 0 != POOL_COUNT % threadCount)
			{
				offset += (POOL_COUNT % threadCount);
			}
			freeThreads.emplace_back([this, start, offset]() mutable
				{
					const auto end = (std::min)(start + offset, (decltype(start))POOL_COUNT);
					for (; start < end; ++start)
					{
						uint8_t* ptr{};
						while (m_pools[start].try_pop(ptr))
						{
							free(ptr);
						}
					}
				});
			start += offset;
		}

		for (auto& th : freeThreads)
		{
			th.join();
		}
	}

	uint8_t* allocate(const size_t _size)
	{
		uint8_t* ptr{};
		if (0 < _size)
		{
			const auto adjustSize{ ADJUST_SIZE(_size) };
			const auto index{ MAKE_POOL_INDEX(adjustSize) };
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
		if (_ptr && 0 < _size)
		{
			const auto adjustSize{ ADJUST_SIZE(_size) };
			const auto index{ MAKE_POOL_INDEX(adjustSize) };
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