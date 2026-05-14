#pragma once

template<typename T>
concept HasIterator = requires
{
	typename T::iterator;
};

template<class T>
struct IsPair : public std::false_type
{
};

template<class T1, class T2>
struct IsPair<std::pair<T1, T2>> : public std::true_type
{
	// ex
	// if constexpr (IsPair<std::map<~~>::iterator::value_type>::value)
};

template<typename T>
bool IsInRange(const T& _rangeContainer, size_t index)
{
	return (0 <= index && index < _rangeContainer.size());
}

template <class T>
inline void HashCombine(std::size_t& _seed, const T& _v)
{
	_seed ^= std::hash<T>{}(_v) + 0x9e3779b9 + (_seed << 6) + (_seed >> 2);
}

// FunctionTraits
// base
template<typename T>
struct FunctionTraits;

// function pointer
template<typename R, typename... Args>
struct FunctionTraits<R(*)(Args...)>
{
    using ArgTypes = std::tuple<std::remove_cvref_t<Args>...>;
    using ReturnType = R;
};

// member function (non-const)
template<typename C, typename R, typename... Args>
struct FunctionTraits<R(C::*)(Args...)>
{
    using ArgTypes = std::tuple<std::remove_cvref_t<Args>...>;
    using ReturnType = R;
    using ClassType = C;
};

// member function (const)
template<typename C, typename R, typename... Args>
struct FunctionTraits<R(C::*)(Args...) const>
{
    using ArgTypes = std::tuple<std::remove_cvref_t<Args>...>;
    using ReturnType = R;
    using ClassType = C;
};

// functor / lambda
template<typename T>
struct FunctionTraits
    : FunctionTraits<decltype(&T::operator())>
{
};
// FunctionTraits

// ExtractSharedPtrInner
template<typename T>
struct ExtractSharedPtrInner;

template<typename T>
struct ExtractSharedPtrInner<std::shared_ptr<T>>
{
    using Type = T;
};

template<typename T>
struct ExtractSharedPtrInner<const std::shared_ptr<T>&>
{
    using Type = T;
};

template<typename T>
struct ExtractSharedPtrInner<std::shared_ptr<T>&&>
{
    using Type = T;
};
// ExtractSharedPtrInner