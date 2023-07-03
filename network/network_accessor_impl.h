#pragma once

class Network;
class NetworkAccessorImpl : public NetworkAccessor
{
public:
    static NetworkAccessorImpl* Create(Network* _networkModule)
    {
        return new NetworkAccessorImpl(_networkModule);
    }
	~NetworkAccessorImpl() = default;

	virtual Result RegistAcceptedHandler(const std::string& _listenerName, AcceptedHandler_t&& _handler) final;
	virtual Result RequestConnect(const std::string& _address, const uint16_t& _port, ConnectedHandler_t&& _handler) final;
	virtual Result RequestConnect(const std::string& _connecterName, ConnectedHandler_t&& _handler, const uint16_t& _tryReconnectCount) final;

	virtual void StopPublicListen(const std::string& _listenerName) final;
	virtual void ClosePublicConnection(const std::string& _listenerName) final;

private:
	NetworkAccessorImpl(Network* _networkModule) : m_networkModule(*_networkModule) {};

	Network& m_networkModule;
};