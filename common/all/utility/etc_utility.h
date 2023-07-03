#pragma once

#define SAFE_DELETE(p) { if (p) delete p; p = nullptr; }
#define SAFE_ARRAY_DELETE(p) { if (p) delete [] p; p = nullptr; }

// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

#define SCOPED_READ_LOCK(_sharedMutex) std::shared_lock<std::shared_mutex> readLock(_sharedMutex);
#define SCOPED_WRITE_LOCK(_sharedMutex) std::unique_lock<std::shared_mutex> writeLock(_sharedMutex);

// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

struct StartTime
{
	std::chrono::system_clock::time_point startTime = std::chrono::system_clock::now();	// 1 is 100 nanosecond
	std::chrono::steady_clock::time_point startTick = std::chrono::steady_clock::now();	// 1 is 1 nanosecond
};

namespace TimeUnit
{
	constexpr float UsToNs = 1000.0f;	// microsecond -> nanosecond
	constexpr float MsToUs = 1000.0f;   // millisecond -> microsecond
	constexpr float SecToMs = 1000.0f;	// second -> millisecond
	constexpr float MinuteToSec = 60.0f;
	constexpr float HourToSec = 60.0f * MinuteToSec;
	constexpr float DayToSec = 24.0f * MinuteToSec;
}

extern StartTime g_startTime;

// utc
using ProfileTime_t = int64_t;	// nanosecond for utc 0
#define GET_PROFILE_TIME() (g_startTime.startTime + (std::chrono::steady_clock::now() - g_startTime.startTick)).time_since_epoch().count()	// nanosecond for utc 0

using Time_t = int64_t;	// millisecond for utc 0
#define GET_TIME() std::chrono::duration_cast<std::chrono::milliseconds>((g_startTime.startTime + (std::chrono::steady_clock::now() - g_startTime.startTick)).time_since_epoch()).count()	// millisecond for utc 0

// tick
using ProfileTickTime_t = int64_t;	// nanosecond for tick
#define GET_PROFILE_TICK() std::chrono::steady_clock::now().time_since_epoch().count()	// nanosecond for tick

using TickTime_t = int64_t;	// millisecond for tick
#define GET_TICK() std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()	// millisecond for tick

// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------