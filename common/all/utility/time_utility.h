#pragma once

// utc
using ProfileTime_t = std::time_t;	// nanosecond for utc 0
using Time_t = std::time_t;	// millisecond for utc 0

// tick
using ProfileTickTime_t = std::time_t;	// nanosecond for tick
using TickTime_t = std::time_t;	// millisecond for tick

// duration
using DurationTimeSec_t = std::time_t;
using DurationTimeMs_t = std::time_t;

namespace TimeUnit
{
	constexpr float UsToNs = 1000.0f;	// microsecond -> nanosecond
	constexpr float MsToUs = 1000.0f;   // millisecond -> microsecond
	constexpr float SecToMs = 1000.0f;	// second -> millisecond
	constexpr float MinuteToSec = 60.0f;
	constexpr float HourToSec = 60.0f * MinuteToSec;
	constexpr float DayToSec = 24.0f * MinuteToSec;
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

	Time_t GetLocalMidNightUtc(const TickTime_t& _time)
	{
		static std::time_t startMilliSeconds = std::chrono::duration_cast<std::chrono::milliseconds>(startTime.time_since_epoch()).count();
		static constexpr auto DayToMilliSeconds = (TimeUnit::DayToSec * TimeUnit::SecToMs);

		int dayDiff = static_cast<int>((_time - startMilliSeconds) / DayToMilliSeconds);
		return startLocalMidNightUtc + static_cast<Time_t>((dayDiff * DayToMilliSeconds) / TimeUnit::SecToMs);
	}
};

extern StartTime g_startTime;

// utc(ProfileTime_t, Time_t)
#define GET_PROFILE_TIME() (g_startTime.startTime + (std::chrono::steady_clock::now() - g_startTime.startTick)).time_since_epoch().count()	// nanosecond for utc 0
#define GET_TIME() std::chrono::duration_cast<std::chrono::milliseconds>((g_startTime.startTime + (std::chrono::steady_clock::now() - g_startTime.startTick)).time_since_epoch()).count()	// millisecond for utc 0

// tick(ProfileTickTime_t, TickTime_t)
#define GET_PROFILE_TICK() std::chrono::steady_clock::now().time_since_epoch().count()	// nanosecond for tick
#define GET_TICK() std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()	// millisecond for tick
