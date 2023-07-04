#pragma once

class TestClient : public Module
{
    friend Module;
public:
    ~TestClient();

    virtual bool IsBusinessModule() final { return true; }
    virtual EModule GetModuleType() final { return EModule::TestClient; }
    virtual const char* GetModuleName() final { return "TestClient"; }

    virtual void Shutdown() final;

    TimerJobManagerShared_t GetTimerJobManager() { return m_timerJobManager; }

    void SetSession(ConnectionShared_t& _conn);
    SessionShared_t GetSession() { return m_session; }
    void ClosedSession(const Result& _result, const ConnectionId_t& _connId);

private:
    TestClient(const std::string& _configFilePath);

    virtual Result InitImpl() final;

private:
    ThreadPool m_threadPool;

    TimerJobManagerShared_t m_timerJobManager;

    SessionShared_t m_session;
};