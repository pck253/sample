#pragma once

static_assert(NETWORK_MODULE == 1);

class Network;
class Connecter : public ConnectionManager
{
public:
	using ConnectInfo = std::unordered_map<std::string, asio::ip::tcp::endpoint>;

	explicit Connecter(Network* _networkMoudle);
	virtual ~Connecter();

	Result Init(const uint16_t& _threadCount, ConnectInfo&& _connectInfos);

	Result RequestConnect(const std::string& _address, const uint16_t& _port, const ConnectedConfig& _connectedConfig);
	Result RequestConnect(const std::string& _connecterName, const ConnectedConfig& _connectedConfig, const uint16_t& _tryReconnectCount);

private:
	void OnConnected(const asio::error_code& _error, asio::ip::tcp::socket* _socket, const ConnectedConfig& _connectedConfig, const ConnectionShared_t& _conn,
		const std::string& _connecterName, const uint16_t _tryReconnectCount);

private:
	ConnectInfo m_connectInfos;
};

