#pragma once

#define MODULE_STATIC_IMPL(cls)         \
    extern "C" _declspec(dllexport) Module * CreateModule(Application& _app, const char* _configFilePath, MemoryPool* _memoryPool) \
    {                                                   \
        g_memoryPool = _memoryPool;                     \
        return new cls(_app, _configFilePath);                \
    }                                                   \