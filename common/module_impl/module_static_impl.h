#pragma once

#define MODULE_STATIC_IMPL(cls)         \
    MemoryPool* g_memoryPool = nullptr; \
    Logger g_logger;					\
    StartTime g_startTime;              \
    extern "C" _declspec(dllexport) Module * CreateModule(const char* _configFilePath, MemoryPool* _memoryPool) \
    {                                                   \
        g_memoryPool = _memoryPool;                     \
        return new cls(_configFilePath);                \
    }                                                   \