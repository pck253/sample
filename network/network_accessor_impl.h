#pragma once

static_assert(NETWORK_MODULE == 1);

class Network;
class NetworkAccessorImpl : public NetworkAccessor
{
public:
    static NetworkAccessorImpl* Create(Network* _networkModule)
    {
        return new NetworkAccessorImpl(_networkModule);
    }
	~NetworkAccessorImpl() = default;

	virtual Result RegistAcceptedConfig(const std::string& _listenerName, const AcceptedConfig& _acceptedConfig) final;
	virtual Result RequestConnect(const std::string& _address, const uint16_t& _port, const ConnectedConfig& _connectedConfig) final;
	virtual Result RequestConnect(const std::string& _connecterName, const ConnectedConfig& _connectedConfig, const uint16_t& _tryReconnectCount) final;

	virtual void StopPublicListen(const std::string& _listenerName) final;
	virtual void ClosePublicConnection(const std::string& _listenerName) final;

private:
	NetworkAccessorImpl(Network* _networkModule) : m_networkModule(*_networkModule) {};

	Network& m_networkModule;
};