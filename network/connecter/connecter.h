#pragma once

// -------------------------------------------------------------

class Network;
class Connecter : public ConnectionManager
{
public:
	Connecter(Network* _networkMoudle);
	virtual ~Connecter();

	Result Init(const uint16_t& _threadCount, const nlohmann::json& _config);
	void Shutdown();

	Result RequestConnect(const std::string& _address, const uint16_t& _port, ConnectedHandler_t&& _handler);
	Result RequestConnect(const std::string& _connecterName, ConnectedHandler_t&& _handler, const uint16_t& _tryReconnectCount);

private:
	void OnConnected(const error_code& _error, ip::tcp::socket* _socket, ConnectedHandler_t& _handler, ConnectionShared_t _conn,
		const std::string& _connecterName, const uint16_t& _tryReconnectCount);

private:
	nlohmann::json m_config;
};

