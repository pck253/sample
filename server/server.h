#pragma once

class Server : public Module
{
    friend Module;
public:
    ~Server();

    virtual bool IsBusinessModule() final { return true; }
    virtual EModule GetModuleType() final { return EModule::Server; }
    virtual const char* GetModuleName() final { return "Server"; }

    virtual void Shutdown() final;

    void ShutdownApplicationByRemote()
    {
        GetApplication()->TryShutdown();
    }

    ServerId_t GetServerId() { return m_id; }
    ServerGroupId_t GetServerGroupId() { return m_groupId; }

    TimerJobManagerShared_t GetTimerJobManager() { return m_timerJobManager; }
    ServerSessionManager<ServerSession>& GetServerSessionManager() { return m_serverSessionManager; }

    UserSessionShared_t AddUserSession(ConnectionShared_t _conn);
    UserSessionShared_t FindUserSession(const ConnectionId_t& _connId);
    void ClosedUserSession(const Result& _result, const ConnectionId_t& _connId);

    void PushJobToAllUserForDev(std::function<void(UserSessionShared_t&)>&& _job);

    ServerSessionShared_t ServerConnected(ConnectionShared_t _conn);

    void CallRestfulHandler(const RestfulRequestId_t& _requestId, const std::wstring& _path, const std::wstring& _query, RestfulAccessor* _accessor);

private:
    Server(const std::string& _configFilePath);

    virtual Result InitImpl() final;
    void InitRestfulHandlers();

private:
    ServerId_t m_id = INVALID_SERVER_ID;
    ServerGroupId_t m_groupId = INVALID_SERVER_GROUP_ID;

    ThreadPool m_threadPool;

    TimerJobManagerShared_t m_timerJobManager;

    std::shared_mutex m_userSessionsMutex;
    our::unordered_map<ConnectionId_t, UserSessionShared_t> m_userSessions;

    ServerSessionManager<ServerSession> m_serverSessionManager;

    our::unordered_map<std::wstring, std::function<nlohmann::json(const RestfulParams&)>> m_restfulHandlers;
};