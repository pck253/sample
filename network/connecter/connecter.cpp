#include "pch.h"

static_assert(NETWORK_MODULE == 1);

Connecter::Connecter(Network* _networkMoudle)
	: ConnectionManager(_networkMoudle, EThreadPool::ASIO)
{
}

void Connecter::RegisterShutdownSteps(ShutdownCoordinator& _coordinator)
{
	_coordinator.Push("connecter clear connect infos", [this]()
		{
			m_connectInfos.clear();

			return EStepResult::Done;
		});

	PushCloseConnectionSteps(_coordinator, "connecter", false);

	PushShutdownThreadPoolSteps(_coordinator, "connecter");
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
	const ConnectionShared_t conn = SocketConnectionImpl::Create(socket, connectionId, GetAsioContext(), false, NOT_FROM_ACCEPTOR, *this, _connectedConfig.receivedHandler);

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
	const ConnectionShared_t conn = SocketConnectionImpl::Create(socket, connectionId, GetAsioContext(), false, NOT_FROM_ACCEPTOR, *this, _connectedConfig.receivedHandler);

	socket->async_connect(found->second, std::bind(&Connecter::OnConnected, this, std::placeholders::_1, socket, _connectedConfig, conn, _connecterName, _tryReconnectCount));

	return EError::Success;
}

void Connecter::OnConnected(const asio::error_code& _error, asio::ip::tcp::socket* _socket, const ConnectedConfig& _connectedConfig, const ConnectionShared_t& _conn,
	const std::string& _connecterName, const uint16_t _tryReconnectCount)
{
	// remote_endpoint can be failed when the peer has disconnected.
	asio::error_code error = _error;
	asio::ip::tcp::endpoint remoteEndpoint;
	if (!error)
	{
		remoteEndpoint = _socket->remote_endpoint(error);
		if (error)
		{
			LogWarning("failed to get remote endpoint : {}", error.message());
		}
	}

	Result result;
	if (!error)
	{
		std::string remoteAddr = remoteEndpoint.address().to_string();

		SocketConnectionImpl* impl = static_cast<SocketConnectionImpl*>(_conn.get());
		impl->SetRemoteAddress(remoteAddr);

		if (not AddConnection(_conn, _connectedConfig.closedHandler))
		{
			result = EError::FailedAddConnection;
		}

		bool handlerResult = (*_connectedConfig.connectedHandler)(result, _connecterName, !result ? nullptr : _conn);
		if (!result)
		{
			_conn->Close(result);
		}
		else if (handlerResult)
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

		(*_connectedConfig.connectedHandler)(result, _connecterName, nullptr);
	}
}