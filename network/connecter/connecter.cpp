#include "pch.h"

// -------------------------------------------------------------

Connecter::Connecter(Network* _networkMoudle)
	: ConnectionManager(_networkMoudle)
{
}

Connecter::~Connecter()
{
	SAFE_DELETE(m_workGuard);
}

Result Connecter::Init(const uint16_t& _threadCount, const nlohmann::json& _config)
{
	m_config = _config;
	m_threadCount = _threadCount;

	m_workGuard = new WorkGuard_t(m_ioContext.get_executor());
	for (uint16_t i = 0; i < m_threadCount; ++i)
	{
		m_threads.emplace_back([this]()
			{
				Log("Connecter m_ioContext run.");
				m_ioContext.run();
				Log("Connecter thread stop.");
			});
	}

	return EError::Success;
}

void Connecter::Shutdown()
{
	m_shutdown = true;

	CloseAll(false);

	do
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	} while (!IsEmptyConnection(false));

	if (m_workGuard)
	{
		m_workGuard->reset();
	}

	for (auto& thread : m_threads)
	{
		thread.join();
	}

	if (!IsEmptyConnection(false))
	{
		LogWarning("Remain Connection");
	}

	m_ioContext.stop();
	while (!m_ioContext.stopped());

	Log("Shutdown Connecter");
}

Result Connecter::RequestConnect(const std::string& _address, const uint16_t& _port, ConnectedHandler_t&& _handler)
{
	ip::tcp::socket* socket = new ip::tcp::socket(m_ioContext);
	ConnectionShared_t conn = ConnectionImpl::Create(socket, &m_ioContext, false, NOT_FROM_ACCEPTOR, this);

	ip::tcp::endpoint endpoint(ip::address::from_string(_address.c_str()), _port);
	socket->async_connect(endpoint, boost::bind(&Connecter::OnConnected, this, placeholders::error, socket, std::move(_handler), conn, "", 0));


	return EError::Success;
}

Result Connecter::RequestConnect(const std::string& _connecterName, ConnectedHandler_t&& _handler, const uint16_t& _tryReconnectCount)
{
	if (!m_config.is_array())
	{
		return EError::NotExistConnectInfo;
	}

	if (!_handler)
	{
		return EError::NeedConnectedHandler;
	}

	auto connects = m_config.get<our::vector<nlohmann::json>>();
	for (auto& con : connects)
	{
		auto name = con["name"].get<std::string>();
		if (name != _connecterName)
		{
			continue;
		}
		auto ip = con["ip"].get<std::string>();
		auto port = con["port"].get<uint16_t>();

		ip::tcp::endpoint endpoint(ip::address::from_string(ip), port);

		ip::tcp::socket* socket = new ip::tcp::socket(m_ioContext);
		ConnectionShared_t conn = ConnectionImpl::Create(socket, &m_ioContext, false, NOT_FROM_ACCEPTOR, this);

		socket->async_connect(endpoint, boost::bind(&Connecter::OnConnected, this, placeholders::error, socket, std::move(_handler), conn, name, _tryReconnectCount));

		return EError::Success;
	}

	return EError::NotExistConnectInfo;
}

void Connecter::OnConnected(const error_code& _error, ip::tcp::socket* _socket, ConnectedHandler_t& _handler, ConnectionShared_t _conn,
	const std::string& _connecterName, const uint16_t& _tryReconnectCount)
{
	Result result;
	if (!_error)
	{
		ConnectionId_t connectionId = m_networkMoudle.MakeConnectionId();

		std::string remoteAddr = _socket->remote_endpoint().address().to_string();

		ConnectionImpl* impl = static_cast<ConnectionImpl*>(_conn.get());
		impl->SetRemoteAddress(remoteAddr);
		impl->SetConnectionId(connectionId);
		bool handlerResult = _handler(result, _connecterName, _conn);

		AddConnection(connectionId, _conn, _conn->IsPublic());

		if (!m_shutdown && handlerResult)
		{
			impl->InitReceive();

			Log("connected. connection id={}", connectionId);
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
			RequestConnect(_connecterName, std::move(_handler), _tryReconnectCount - 1);
			Log("try reconnect");
			return;
		}

		result = EError::FailedNetworkConnect;
		_conn->Close(result);
		_conn.reset();

		_handler(result, _connecterName, _conn);
	}
}