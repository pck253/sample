#pragma once

class MemoryPool final
{
public:
	inline static constexpr std::size_t ALIGNMENT{ 8 };

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
			const auto adjustSize{ AdjustSize(_size) };
			const auto index{ MakePoolIndex(adjustSize) };
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
			const auto adjustSize{ AdjustSize(_size) };
			const auto index{ MakePoolIndex(adjustSize) };
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
	inline static constexpr std::size_t POOL_COUNT{ 64 * 1024 / ALIGNMENT }; // 8B ~ 64KB

	static auto AdjustSize(const std::size_t _s) { return (_s % ALIGNMENT != 0) ? (((_s / ALIGNMENT) + 1) * ALIGNMENT) : _s; }
	static auto MakePoolIndex(const std::size_t _adjustedSize) { return (_adjustedSize / ALIGNMENT) - 1; }

	Concurrency::concurrent_queue<uint8_t*> m_pools[POOL_COUNT];	// 8B ~ 64KB
};

inline MemoryPool* g_memoryPool = nullptr;