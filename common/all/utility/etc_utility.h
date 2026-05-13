#pragma once

#define SAFE_DELETE(p) { if (p) delete p; p = nullptr; }
#define SAFE_ARRAY_DELETE(p) { if (p) delete [] p; p = nullptr; }

// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

#define SCOPED_READ_LOCK(_sharedMutex) std::shared_lock<std::shared_mutex> readLock(_sharedMutex);
#define SCOPED_WRITE_LOCK(_sharedMutex) std::unique_lock<std::shared_mutex> writeLock(_sharedMutex);

// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
template <typename T_ID, T_ID INVALID_VALUE> requires std::is_integral_v<T_ID>
struct StrongId
{
	using IdType = T_ID;

	T_ID id{};

	StrongId() = default;
	StrongId(const T_ID _id)
	{
		id = _id;
	}

	static T_ID GetInvalidValue()
	{
		return INVALID_VALUE;
	}

	operator T_ID() const
	{
		return id;
	}
	void operator = (const T_ID _id)
	{
		id = _id;
	}
	void operator = (const StrongId& _strongId)
	{
		id = _strongId.id;
	}
	bool operator == (const T_ID _rhs) const
	{
		return id == _rhs;
	}
	bool operator != (const T_ID _rhs) const
	{
		return !(operator==(_rhs));
	}
	bool operator == (const StrongId& _rhs) const
	{
		return id == _rhs.id;
	}
	bool operator != (const StrongId& _rhs) const
	{
		return !(operator==(_rhs));
	}
	bool operator ()() const
	{
		return (INVALID_VALUE != id);
	}

	// for zpp::bits
	template <typename Archive, typename Self>
	static auto serialize(Archive& ar, Self& self)
	{
		return ar(self.id);
	}
};

namespace std
{
	template<typename T_ID, T_ID INVALID_VALUE>
	struct hash<StrongId<T_ID, INVALID_VALUE>>
	{
		size_t operator()(const StrongId<T_ID, INVALID_VALUE>& _strongId) const noexcept {
			return std::hash<T_ID>{}(_strongId.id);
		}
	};
}

template <typename T_ID, T_ID INVALID_VALUE>
struct std::formatter<StrongId<T_ID, INVALID_VALUE>, char> : std::formatter<int, char>
{
	template <class FormatContext>
	auto format(const StrongId<T_ID, INVALID_VALUE>& _val, FormatContext& _formatCtx) const
	{
		return std::format_to(_formatCtx.out(), "{}", _val.id);
	}
};
// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

using UniqueId_t = StrongId<uint64_t, 0ui64>;
class UniqueIdGenerator
{
	#define MIN_TIME_UNIT std::chrono::nanoseconds(100)	// utc-0 1 tick is 100 nano seconds

public:
	UniqueIdGenerator() = default;
	~UniqueIdGenerator() = default;

	Result Init(const uint8_t& _prefixByte, const uint32_t& _prefixValue, const std::chrono::nanoseconds& _timeUnit = MIN_TIME_UNIT)
	{
		UniqueId_t maxPrefix = 2 ^ (_prefixByte * 8);
		if (maxPrefix <= _prefixValue)
		{
			return EError::InvalidUniqueIdPrefix;
		}

		if (MIN_TIME_UNIT > _timeUnit)
		{
			return EError::InvalidUniqueIdTimeUnit;
		}

		// GET_PROFILE_TIME() : nano seconds
		UniqueId_t base = GET_PROFILE_TIME() / MIN_TIME_UNIT.count();
		base = base / (_timeUnit.count() / MIN_TIME_UNIT.count());

		UniqueId_t prefixFilter = std::numeric_limits<UniqueId_t>::max() >> (_prefixByte * 8);
		base = base & prefixFilter;

		UniqueId_t prefix = _prefixValue;
		prefix = prefix << ((sizeof(UniqueId_t) * 8) - (_prefixByte * 8));
		m_sequence = base | prefix;

		return EError::Success;
	}

	UniqueId_t Alloc() { return ++m_sequence; }

private:
	std::atomic<UniqueId_t::IdType> m_sequence{ UniqueId_t::GetInvalidValue() };
};

// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

class FinalJob
{
public:
	FinalJob(std::function<void()>&& _job) : m_job(std::move(_job)) {};
	virtual ~FinalJob() { if (m_job) m_job(); }

private:
	std::function<void()> m_job;
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