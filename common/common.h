#pragma once

#define WIN32_LEAN_AND_MEAN

#include <crtdbg.h>
#define _CRTDBG_MAP_ALLOC

#define NOMINMAX
#include <windows.h>
#include <variant>
#include <string_view>
#include <functional>
#include <thread>
#include <condition_variable>
#include <shared_mutex>
#include <fstream>
#include <chrono>
#include <concurrent_queue.h>
#include <concurrent_priority_queue.h>
#include <memory>
#include <format>

#include <vector>
#include <array>
#include <queue>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>

#ifdef _DEBUG
#define FLATBUFFERS_MEMORY_LEAK_TRACKING
#endif

#include "json.hpp"

class SerializedJobQueue;
using SerializedJobQueueShared_t = std::shared_ptr<SerializedJobQueue>;

#include "all/logger.h"
#include "all/allocator/memory_pool.h"
#include "all/allocator/stl_allocator.h"
#include "all/allocator/instance_pool.h"
#include "all/error.h"
#include "all/math/math_common.h"
#include "all/utility/time_utility.h"
#include "all/utility/etc_utility.h"
#include "all/utility/string_utility.h"
#include "all/utility/shutdown_utility.h"
#include "all/utility/stl_utility.h"
#include "all/thread/thread_pool.h"
#include "all/thread/serialized_job_queue.h"
#include "all/module.h"
#include "all/application.h"
#include "all/cheat/cheat_command.h"
