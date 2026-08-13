#pragma once

inline std::random_device g_randomDevice;
inline std::mt19937 g_randomGen(g_randomDevice());

template<typename T>
inline T GetRandomValue(const T& _min, const T& _max)
{
    if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>)
    {
        static_assert(0 == sizeof(T), "Not a floating-point type.");
        return {};
    }
    else if constexpr (1 == sizeof(T))
    {
        static_assert(0 == sizeof(T), "Type size cannot be 1 byte");
        return {};
    }

    std::uniform_int_distribution<T> dis(_min, _max);
    return dis(g_randomGen);
}