#pragma once
class Network : public Module
{
    friend Module;
public:
    ~Network();

    virtual bool IsBusinessModule() final { return false; }
    virtual EModule GetModuleType() final { return EModule::Network; }
    virtual const char* GetModuleName() final { return "Network"; }

    virtual void Shutdown() final;

    inline Listener& GetListener() { return m_listener; }
    inline Connecter& GetConnecter() { return m_connecter; }

    inline ConnectionId_t MakeConnectionId() { return (++m_ConnectionIdSequence); }

private:
    Network(const std::string& _configFilePath);

    virtual Result InitImpl() final;

private:
    std::atomic<ConnectionId_t> m_ConnectionIdSequence = INVALID_CONNECTION_ID;

    Listener m_listener;
    Connecter m_connecter;
};