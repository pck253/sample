#pragma once

#define MODULE_STATIC_IMPL(cls)         \
    MemoryPool* g_memoryPool = nullptr; \
	Logger g_logger;					\
    StartTime g_startTime;              \
    thread_local SerializedJobQueueShared_t g_currentSerializedJobQueue;                                        \
    extern "C" _declspec(dllexport) Module * CreateModule(const char* _configFilePath, MemoryPool* _memoryPool) \
    {                                                   \
        g_memoryPool = _memoryPool;                     \
        return Module::Create<cls>(_configFilePath);    \
    }                                                   \
    \
    Module* Module::m_self = nullptr;   \
    std::once_flag Module::m_onceFlag;  \