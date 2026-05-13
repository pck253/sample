#pragma once

static_assert(NETWORK_MODULE == 1);

class Network : public Module
{
    friend Module;
public:
    Network(const std::string& _configFilePath);
    ~Network();

    virtual bool IsBusinessModule() final { return false; }
    virtual EModule GetModuleType() final { return EModule::Network; }

    virtual void Shutdown() final;

    inline Listener& GetListener() { return m_listener; }
    inline Connecter& GetConnecter() { return m_connecter; }
    inline ImnManager& GetImnManager() { return m_imnManager; }

    inline ConnectionId_t MakeConnectionId() { return (++m_ConnectionIdSequence); }

    inline bool IsRedirectToImn(const std::string& _imnName) { return (m_redirectToImnInfos.find(_imnName) != m_redirectToImnInfos.end()); }

private:
    virtual Result InitImpl() final;

private:
    std::atomic<ConnectionId_t> m_ConnectionIdSequence = INVALID_CONNECTION_ID;

    Listener m_listener;
    Connecter m_connecter;

    std::set<std::string> m_redirectToImnInfos;
    ImnManager m_imnManager;
};