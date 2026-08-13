#pragma once

// utc
using ProfileTime_t = std::time_t;	// nanosecond for utc 0
using Time_t = std::time_t;	// millisecond for utc 0

// tick
using ProfileSteadyTime_t = std::time_t;	// nanosecond for tick
using SteadyTime_t = std::time_t;	// millisecond for tick

// duration
using DurationTimeSec_t = std::time_t;
using DurationTimeMs_t = std::time_t;

namespace TimeUnit
{
	constexpr double US_TO_NS = 1000.0f;	// microsecond -> nanosecond
	constexpr double MS_TO_US = 1000.0f;	// millisecond -> microsecond
	constexpr double SEC_TO_MS = 1000.0f;	// second -> millisecond
	constexpr double MINUTE_TO_SEC = 60.0f;
	constexpr double HOUR_TO_SEC = 60.0f * MINUTE_TO_SEC;
	constexpr double DAY_TO_SEC = 24.0f * MINUTE_TO_SEC;
}

struct StartTime
{
	// for utc 0
	std::chrono::system_clock::time_point startTime = std::chrono::system_clock::now();	// 1 is 100 nanosecond
	std::chrono::steady_clock::time_point startTick = std::chrono::steady_clock::now();	// 1 is 1 nanosecond
	Time_t startLocalMidNightUtc = 0;
	int startLocalWeekDay = 0;

	StartTime()
	{
		auto startSeconds = std::chrono::duration_cast<std::chrono::seconds>(startTime.time_since_epoch()).count();
		tm localTm{};
		localtime_s(&localTm, &startSeconds);
		startLocalWeekDay = localTm.tm_wday;

		localTm.tm_hour = 0;
		localTm.tm_min = 0;
		localTm.tm_sec = 0;
		startLocalMidNightUtc = std::mktime(&localTm);
	}

	Time_t GetLocalMidNightUtc(const SteadyTime_t& _time) const
	{
		static std::time_t startMilliSeconds = std::chrono::duration_cast<std::chrono::milliseconds>(startTime.time_since_epoch()).count();
		static constexpr auto DAY_TO_MILLI_SECONDS = (TimeUnit::DAY_TO_SEC * TimeUnit::SEC_TO_MS);

		auto dayDiff = static_cast<Time_t>((_time - startMilliSeconds) / DAY_TO_MILLI_SECONDS);
		return startLocalMidNightUtc + static_cast<Time_t>((dayDiff * DAY_TO_MILLI_SECONDS) / TimeUnit::SEC_TO_MS);
	}
};

inline StartTime g_startTime;

// utc(ProfileTime_t, Time_t)
inline auto GetProfileTime()
{
	// nanosecond for utc 0
	return (g_startTime.startTime + (std::chrono::steady_clock::now() - g_startTime.startTick)).time_since_epoch().count();
}
inline auto GetTime()
{
	// millisecond for utc 0
	return std::chrono::duration_cast<std::chrono::milliseconds>((g_startTime.startTime + (std::chrono::steady_clock::now() - g_startTime.startTick)).time_since_epoch()).count();
}

// tick(ProfileSteadyTime_t, SteadyTime_t)
inline auto GetProfileSteadyTime()
{
	// nanosecond for tick
	return std::chrono::steady_clock::now().time_since_epoch().count();
}
inline auto GetSteadyTime()
{
	// millisecond for tick
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
