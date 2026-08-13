#include "pch.h"

static_assert(NETWORK_MODULE == 1);

#ifdef _DEBUG
#define CHECK_ACCEPT_STRAND
#endif

Listener::Listener(Network* _networkMoudle)
	: ConnectionManager(_networkMoudle, EThreadPool::ASIO),
	m_acceptStrand(asio::make_strand(*GetAsioContext()))
{
}

void Listener::RegisterShutdownSteps(ShutdownCoordinator& _coordinator)
{
	_coordinator.Push("listener close acceptors", [this]()
		{
			if (m_acceptorInfos.empty())
			{
				m_acceptorsClosed.store(true);
				return EStepResult::Done;
			}

			asio::post(m_acceptStrand, [this]()
				{
					for (AcceptorIndex index = 0; index < static_cast<AcceptorIndex>(m_acceptorInfos.size()); ++index)
					{
						CloseAcceptor(index);
					}
					m_acceptorsClosed.store(true);
				});

			return EStepResult::Done;
		});

	_coordinator.Push("listener acceptors closed", [this]()
		{
			return m_acceptorsClosed.load() ? EStepResult::Done : EStepResult::Wait;
		});

	PushCloseConnectionSteps(_coordinator, "listener", true);

	PushShutdownThreadPoolSteps(_coordinator, "listener");
}

Listener::~Listener()
{
}

Result Listener::Init(const std::vector<std::tuple<std::string, asio::ip::tcp::endpoint, bool>>& _addresses, const uint16_t& _threadCount)
{
	if (_addresses.empty())
	{
		return EError::Success;
	}

	// ------------------------------------------------------------------------
	// acceptor needs AcceptorInfo::acceptedHandler to listen : see SetAcceptedConfig
	// ------------------------------------------------------------------------
	AcceptorIndex index = 0;
	for (auto& [name, addr, isPublic] : _addresses)
	{
		m_acceptorInfos.emplace_back(addr, *GetAsioContext(), isPublic);
		m_acceptorNames.emplace(name, index);

		++index;
	}

	InitThreadPool(_threadCount);

	return EError::Success;
}

Result Listener::SetAcceptedConfig(const std::string& _listenerName, const AcceptedConfig& _acceptedConfig)
{
	auto found = m_acceptorNames.find(_listenerName);
	if (found == m_acceptorNames.end())
	{
		return EError::NotExistNetworkListener;
	}

	if (not _acceptedConfig.IsValid())
	{
		return EError::NeedAcceptedHandler;
	}

	auto const index{ found->second };
	auto& acceptorInfo{ m_acceptorInfos[index] };

	if (not acceptorInfo.acceptedConfig.Set(_acceptedConfig.acceptedHandler, _acceptedConfig.receivedHandler, _acceptedConfig.closedHandler))
	{
		return EError::AlreadySettedHandler;
	}

	//for(/* if want*/)
	//{
		asio::post(m_acceptStrand, [this, index]() { StartAccept(index); });
	//}

	return EError::Success;
}

void Listener::StopPublicListen(const std::string& _listenerName)
{
	auto found = m_acceptorNames.find(_listenerName);
	if (found == m_acceptorNames.end())
	{
		return;
	}

	auto const index{ found->second };
	if (not m_acceptorInfos[index].isPublic)
	{
		return;
	}

	asio::post(m_acceptStrand, [this, index]()
		{
			CloseAcceptor(index);
			CloseAll(true, index);
		});
}

bool Listener::IsEmptyPublicConnection(const std::string& _listenerName) const
{
	auto found = m_acceptorNames.find(_listenerName);
	if (found == m_acceptorNames.end())
	{
		return true;
	}

	auto const index{ found->second };
	if (not m_acceptorInfos[index].isPublic)
	{
		return true;
	}

	return IsEmptyConnection(true, index);
}

void Listener::OnAccepted(const asio::error_code& _error, asio::ip::tcp::socket* _socket, const AcceptorIndex _acceptorIndex, const ConnectionShared_t& _conn)
{
	auto& acceptorInfo = m_acceptorInfos[_acceptorIndex];

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

		if (not AddConnection(_conn, acceptorInfo.acceptedConfig.closedHandler))
		{
			result = EError::FailedAddConnection;
		}

		bool handlerResult = (*acceptorInfo.acceptedConfig.acceptedHandler)(result, !result ? nullptr : _conn);
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
		result = EError::FailedNetworkAccept;

		(*acceptorInfo.acceptedConfig.acceptedHandler)(result, nullptr);
	}

	asio::post(m_acceptStrand, [this, _acceptorIndex]() { StartAccept(_acceptorIndex); });
}

void Listener::StartAccept(const AcceptorIndex _acceptorIndex)
{
#ifdef CHECK_ACCEPT_STRAND
	if (not m_acceptStrand.running_in_this_thread())
	{
		LogError("StartAccept is not on m_acceptStrand. must be called via asio::post(m_acceptStrand).");
	}
#endif

	auto& acceptorInfo = m_acceptorInfos[_acceptorIndex];
	if (IsShutdownStarted() || not acceptorInfo.acceptor.is_open())
	{
		return;
	}

	asio::ip::tcp::socket* socket = new asio::ip::tcp::socket(*GetAsioContext());
	const ConnectionId_t connId{ m_networkMoudle.MakeConnectionId() };
	ConnectionShared_t newConn =
		SocketConnectionImpl::Create(socket, connId, GetAsioContext(), acceptorInfo.isPublic, _acceptorIndex, *this, acceptorInfo.acceptedConfig.receivedHandler);

	acceptorInfo.acceptor.async_accept(*socket,
		[this, socket, _acceptorIndex, conn = std::move(newConn)](const asio::error_code& _error)
		{
			OnAccepted(_error, socket, _acceptorIndex, conn);
		});
}

void Listener::CloseAcceptor(const AcceptorIndex _acceptorIndex)
{
#ifdef CHECK_ACCEPT_STRAND
	if (not m_acceptStrand.running_in_this_thread())
	{
		LogError("CloseAcceptor is not on m_acceptStrand. must be called via asio::post(m_acceptStrand).");
	}
#endif

	auto& acceptorInfo = m_acceptorInfos[_acceptorIndex];
	if (not acceptorInfo.acceptor.is_open())
	{
		return;
	}

	asio::error_code ec;
	acceptorInfo.acceptor.cancel(ec);
	acceptorInfo.acceptor.close(ec);
}
