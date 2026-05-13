#pragma once

static_assert(NETWORK_MODULE == 1);

class Network;

struct ImnInfo
{
    AcceptedConfig acceptedConfig{};
};

// Internal Module Network Manager
class ImnManager : public ConnectionManager
{
public:
    ImnManager(Network* _networkMoudle);
    virtual ~ImnManager();

    Result Init(const std::set<std::string>& _imns, const uint16_t& _threadCount);

    Result SetAcceptedConfig(const std::string& _imnName, const AcceptedConfig& _handler);
    Result RequestConnect(const std::string& _imnName, const ConnectedConfig& _connectedConfig);

private:
    std::unordered_map<std::string, ImnInfo> m_imnInfos;
};