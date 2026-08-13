#pragma once

template <typename T_TAG, typename T_ID, T_ID INVALID_VALUE> requires std::is_integral_v<T_ID>
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

#ifdef USE_ZPP_BITS
	template <typename T_ARCHIVE, typename T_SELF>
	static auto serialize(T_ARCHIVE& _ar, T_SELF& _self)
	{
		return _ar(_self.id);
	}
#endif
};

#define DEFINE_STRONG_ID(name, underlyingType, invalidValue)		\
    struct name##Tag {};											\
    using name = StrongId<name##Tag, underlyingType, invalidValue>;

namespace std
{
	template<typename T_TAG, typename T_ID, T_ID INVALID_VALUE>
	struct hash<StrongId<T_TAG, T_ID, INVALID_VALUE>>
	{
		size_t operator()(const StrongId<T_TAG, T_ID, INVALID_VALUE>& _strongId) const noexcept {
			return std::hash<T_ID>{}(_strongId.id);
		}
	};
}

template <typename T_TAG, typename T_ID, T_ID INVALID_VALUE>
struct std::formatter<StrongId<T_TAG, T_ID, INVALID_VALUE>, char> : std::formatter<int, char>
{
	template <class T_FORMAT_CONTEXT>
	auto format(const StrongId<T_TAG, T_ID, INVALID_VALUE>& _val, T_FORMAT_CONTEXT& _formatCtx) const
	{
		return std::format_to(_formatCtx.out(), "{}", _val.id);
	}
};
// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

class UniqueIdGenerator
{
	using SrcType_t = uint64_t;
public:
	UniqueIdGenerator() = default;
	~UniqueIdGenerator() = default;

	Result Init(const uint8_t& _prefixByte, const uint32_t& _prefixValue, const std::chrono::nanoseconds& _timeUnit = MIN_TIME_UNIT)
	{
		SrcType_t maxPrefix = 2 ^ (_prefixByte * 8);
		if (maxPrefix <= _prefixValue)
		{
			return EError::InvalidUniqueIdPrefix;
		}

		if (MIN_TIME_UNIT > _timeUnit)
		{
			return EError::InvalidUniqueIdTimeUnit;
		}

		// GetProfileTime() : nano seconds
		SrcType_t base = GetProfileTime() / MIN_TIME_UNIT.count();
		base = base / (_timeUnit.count() / MIN_TIME_UNIT.count());

		SrcType_t prefixFilter = std::numeric_limits<SrcType_t>::max() >> (_prefixByte * 8);
		base = base & prefixFilter;

		SrcType_t prefix = _prefixValue;
		prefix = prefix << ((sizeof(SrcType_t) * 8) - (_prefixByte * 8));
		m_sequence = base | prefix;

		return EError::Success;
	}

	SrcType_t Alloc() { return ++m_sequence; }

private:
	inline static constexpr auto MIN_TIME_UNIT{ std::chrono::nanoseconds(100) };	// utc-0 1 tick is 100 nano seconds
	std::atomic<SrcType_t> m_sequence{ 0 };
};
