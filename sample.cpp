
#include "common.h"

MemoryPool* g_memoryPool{};
Logger g_logger;
StartTime g_startTime;

int main(int _argc, char* _argv[])
{
    if (_argc < 2)
    {
        printf_s("need config file path : server_binary [config file path]");
        return -1;
    }

    Application* app = new Application();
    Result result = app->Build(_argv[1]);
    if (result != EError::Success)
    {
        return -1;
    }
    result = app->InitCommon();
    if (result != EError::Success)
    {
        return -1;
    }
    result = app->InitBusiness();
    if (result != EError::Success)
    {
        return -1;
    }

    do
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } while (!app->IsShutdown());

    SAFE_DELETE(app);

    return 0;
}