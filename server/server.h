#pragma once

class Server : public Module
{
    friend Module;
public:
    explicit Server(const std::string& _configFilePath);
    ~Server();

    virtual bool IsBusinessModule() final { return true; }
    virtual EModule GetModuleType() final { return EModule::Server; }

    virtual void Shutdown() final;

    void ShutdownApplicationByRemote()
    {
        GetApplication()->Shutdown();
    }

    ServerId_t GetServerId() { return m_id; }
    ServerGroupId_t GetServerGroupId() { return m_groupId; }

    TimerJobManagerShared_t GetTimerJobManager() { return m_timerJobManager; }
    ServerSessionManager<ServerSession>& GetServerSessionManager() { return m_serverSessionManager; }

    UserSessionShared_t AddUserSession(ConnectionShared_t _conn);
    UserSessionShared_t FindUserSession(const ConnectionId_t& _connId);
    void ClosedUserSession(const Result& _result, const ConnectionId_t& _connId);

    ServerSessionShared_t ServerConnected(ConnectionShared_t _conn);

    void CallRestfulHandler(const RestufulRequestId_t& _requestId, const std::wstring& _path, const std::wstring& _query, WebAccessor* _accessor);

private:

    virtual Result InitImpl() final;
    void InitRestfulHandlers();

private:
    ServerId_t m_id = ServerId_t::GetInvalidValue();
    ServerGroupId_t m_groupId = ServerGroupId_t::GetInvalidValue();

    ThreadPool m_threadPool;

    TimerJobManagerShared_t m_timerJobManager;

    std::shared_mutex m_userSessionsMutex;
    our::unordered_map<ConnectionId_t, UserSessionShared_t> m_userSessions;

    ServerSessionManager<ServerSession> m_serverSessionManager;

    std::unordered_map<std::wstring, std::function<nlohmann::json(const WebParams&)>> m_restfulHandlers;
};