#pragma once

class TestClient : public Module
{
    friend Module;
public:
    TestClient(const std::string& _configFilePath);
    ~TestClient();

    virtual bool IsBusinessModule() final { return true; }
    virtual EModule GetModuleType() final { return EModule::TestClient; }
    
    virtual void Shutdown() final;

    SessionShared_t Connected(ConnectionShared_t _conn);
    SessionShared_t GetSession() const { return m_session; }

    TimerJobManagerShared_t GetTimerJobManager() { return m_timerJobManager; }

    void ApplicationShutdown(const Result& _result);

private:
    virtual Result InitImpl() final;
    void InitRestfulHandlers();

private:
    ThreadPool m_threadPool;

    TimerJobManagerShared_t m_timerJobManager;

    SessionShared_t m_session;
};