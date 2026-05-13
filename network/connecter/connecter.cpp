#include "pch.h"

static_assert(NETWORK_MODULE == 1);

Connecter::Connecter(Network* _networkMoudle)
	: ConnectionManager(_networkMoudle, EThreadPool::ASIO)
{
	m_afterShutdown = [this]()
		{
			m_connectInfos.clear();

			CloseAllAndWait(false);

			ShutdownThreadPool();

			Log("Shutdown Connecter");
		};
}

Connecter::~Connecter()
{
}

Result Connecter::Init(const uint16_t& _threadCount, ConnectInfo&& _connectInfos)
{
	m_connectInfos = std::move(_connectInfos);

	InitThreadPool(_threadCount);

	return EError::Success;
}

Result Connecter::RequestConnect(const std::string& _address, const uint16_t& _port, const ConnectedConfig& _connectedConfig)
{
	asio::ip::tcp::socket* socket = new asio::ip::tcp::socket(*GetAsioContext());
	ConnectionId_t connectionId = m_networkMoudle.MakeConnectionId();
	ConnectionShared_t conn = SocketConnectionImpl::Create(socket, connectionId, GetAsioContext(), false, NOT_FROM_ACCEPTOR, *this, _connectedConfig.receivedHandler, _connectedConfig.closedHandler);

	if (not AddConnection(conn))
	{
		return EError::FailedAddConnection;
	}

	asio::ip::tcp::endpoint endpoint(asio::ip::make_address(_address), _port);
	socket->async_connect(endpoint, std::bind(&Connecter::OnConnected, this, std::placeholders::_1, socket, _connectedConfig, conn, "", 0));

	return EError::Success;
}

Result Connecter::RequestConnect(const std::string& _connecterName, const ConnectedConfig& _connectedConfig, const uint16_t& _tryReconnectCount)
{
	if (m_connectInfos.empty())
	{
		return EError::NotExistConnectInfo;
	}

	if (not _connectedConfig.IsValid())
	{
		return EError::NeedConnectedHandler;
	}

	auto found = m_connectInfos.find(_connecterName);
	if (m_connectInfos.end() == found)
	{
		return EError::NotExistConnectInfo;
	}

	asio::ip::tcp::socket* socket = new asio::ip::tcp::socket(*GetAsioContext());
	ConnectionId_t connectionId = m_networkMoudle.MakeConnectionId();
	ConnectionShared_t conn = SocketConnectionImpl::Create(socket, connectionId, GetAsioContext(), false, NOT_FROM_ACCEPTOR, *this, _connectedConfig.receivedHandler, _connectedConfig.closedHandler);

	if (not AddConnection(conn))
	{
		return EError::FailedAddConnection;
	}

	socket->async_connect(found->second, std::bind(&Connecter::OnConnected, this, std::placeholders::_1, socket, _connectedConfig, conn, _connecterName, _tryReconnectCount));

	return EError::Success;
}

void Connecter::OnConnected(const asio::error_code& _error, asio::ip::tcp::socket* _socket, const ConnectedConfig& _connectedConfig, const ConnectionShared_t& _conn,
	const std::string& _connecterName, const uint16_t _tryReconnectCount)
{
	Result result;
	if (!_error)
	{
		std::string remoteAddr = _socket->remote_endpoint().address().to_string();

		SocketConnectionImpl* impl = static_cast<SocketConnectionImpl*>(_conn.get());
		impl->SetRemoteAddress(remoteAddr);
		bool handlerResult = (*_connectedConfig.connectedHandler)(result, _connecterName, _conn);

		if (handlerResult)
		{
			impl->InitReceive();

			Log("connected. connection id={}", _conn->GetConnectionId());
		}
		else
		{
			_conn->Close(EError::Shutdown);
		}
	}
	else
	{
		if (_tryReconnectCount > 0)
		{
			Log("try reconnect");
			if (RequestConnect(_connecterName, _connectedConfig, _tryReconnectCount - 1) == EError::Success)
			{
				return;
			}
			LogError("falied to try reconnect");
		}

		result = EError::FailedNetworkConnect;
		_conn->Close(result);

		(*_connectedConfig.connectedHandler)(result, _connecterName, nullptr);
	}
}