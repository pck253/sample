#include "pch.h"

Listener::Listener(Network* _networkMoudle)
	: ConnectionManager(_networkMoudle)
{
}

Listener::~Listener()
{
}

Result Listener::Init(our::vector<std::tuple<std::string, ip::tcp::endpoint, bool>>& _addresses, const uint16_t& _threadCount)
{
	m_threadCount = _threadCount;

	size_t concurrentAcceptCount = (std::max)(m_threadCount / _addresses.size(), (size_t)1);

	// ------------------------------------------------------------------------
	// acceptor needs AcceptorInfo::acceptedHandler to listen : see SetAcceptedHandler
	// ------------------------------------------------------------------------
	AcceptorIndex index = 0;
	for (auto& [name, addr, isPublic] : _addresses)
	{
		AcceptorInfo& acceptorInfo = m_acceptorInfos.emplace_back(AcceptorInfo{ addr, ip::tcp::acceptor(m_ioContext, addr), isPublic, nullptr });
		m_acceptorNames.emplace(name, index);

		++index;
	}

	m_workGuard = new WorkGuard_t(m_ioContext.get_executor());
	for (uint16_t i = 0; i < m_threadCount; ++i)
	{
		m_threads.emplace_back([this]()
			{
				Log("Listener m_ioContext run.");
				m_ioContext.run();
				Log("Listener thread stop.");
			});
	}

	return EError::Success;
}

void Listener::Shutdown()
{
	m_shutdown = true;

	// ----------------------------------------------------------------------------------
	// no more listen
	// ----------------------------------------------------------------------------------
	for (auto& acceptorInfo : m_acceptorInfos)
	{
		if (!acceptorInfo.acceptor.is_open())
		{
			continue;
		}

		acceptorInfo.acceptor.cancel();
		acceptorInfo.acceptor.close();
		acceptorInfo.acceptor.release();
	}

	CloseAll(true);
	CloseAll(false);

	do
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	} while (!IsEmptyConnection(false) || !IsEmptyConnection(true));

	if (m_workGuard)
	{
		m_workGuard->reset();
	}

	for (auto& thread : m_threads)
	{
		thread.join();
	}

	m_ioContext.stop();
	while (!m_ioContext.stopped());

	if (!IsEmptyConnection(true))
	{
		LogWarning("Remain Public Connection");
	}

	if (!IsEmptyConnection(false))
	{
		LogWarning("Remain private Connection");
	}

	m_acceptorInfos.clear();

	Log("shutdown Listener");
}

Result Listener::SetAcceptedHandler(const std::string& _listenerName, AcceptedHandler_t&& _handler)
{
	auto found = m_acceptorNames.find(_listenerName);
	if (found == m_acceptorNames.end())
	{
		return EError::NotExistNetworkListener;
	}

	if (!_handler)
	{
		return EError::NeedAcceptedHandler;
	}

	{
		SCOPED_WRITE_LOCK(m_accetorSettingMutex);

		if (m_acceptorInfos[found->second].acceptedHandler)
		{
			return EError::AlreadySettedAcceptHandler;
		}

		m_acceptorInfos[found->second].acceptedHandler = _handler;
	}

	size_t concurrentAcceptCount = (std::max)(m_threadCount / m_acceptorNames.size(), (size_t)1);

	for (size_t i = 0; i < concurrentAcceptCount; ++i)
	{
		ip::tcp::socket* socket = new ip::tcp::socket(m_ioContext);
		ConnectionShared_t conn = ConnectionImpl::Create(socket, &m_ioContext, m_acceptorInfos[found->second].isPublic, found->second, this);

		m_acceptorInfos[found->second].acceptor.async_accept(*socket, boost::bind(&Listener::OnAccepted, this, placeholders::error, socket, found->second, conn));
	}

	return EError::Success;
}

void Listener::StopPublicListen(const std::string& _listenerName)
{
	auto found = m_acceptorNames.find(_listenerName);
	if (found == m_acceptorNames.end())
	{
		return;
	}

	if (!m_acceptorInfos[found->second].isPublic || !m_acceptorInfos[found->second].acceptor.is_open())
	{
		return;
	}
	m_acceptorInfos[found->second].acceptor.cancel();
	m_acceptorInfos[found->second].acceptor.close();
	m_acceptorInfos[found->second].acceptor.release();
}

void Listener::ClosePublicConnection(const std::string& _listenerName)
{
	auto found = m_acceptorNames.find(_listenerName);
	if (found == m_acceptorNames.end())
	{
		return;
	}

	CloseAll(true, found->second);
}

void Listener::OnAccepted(const error_code& _error, ip::tcp::socket* _socket, const AcceptorIndex& _acceptorIndex, ConnectionShared_t _conn)
{
	auto& acceptorInfo = m_acceptorInfos[_acceptorIndex];

	Result result;
	if (!_error)
	{
		ConnectionId_t connectionId = m_networkMoudle.MakeConnectionId();

		std::string remoteAddr = _socket->remote_endpoint().address().to_string();

		ConnectionImpl* impl = static_cast<ConnectionImpl*>(_conn.get());
		impl->SetRemoteAddress(remoteAddr);
		impl->SetConnectionId(connectionId);

		bool handlerResult = acceptorInfo.acceptedHandler(result, _conn);

		AddConnection(connectionId, _conn, acceptorInfo.isPublic);

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
		result = EError::FailedNetworkAccept;

		acceptorInfo.acceptedHandler(result, nullptr);

		_conn->Close(result);
		_conn.reset();
	}

	if (!m_shutdown && acceptorInfo.acceptor.is_open())
	{
		ip::tcp::socket* newSocket = new ip::tcp::socket(m_ioContext);
		ConnectionShared_t newConn = ConnectionImpl::Create(newSocket, &m_ioContext, acceptorInfo.isPublic, _acceptorIndex, this);

		acceptorInfo.acceptor.async_accept(*newSocket, boost::bind(&Listener::OnAccepted, this, placeholders::error, newSocket, _acceptorIndex, newConn));
	}
}
