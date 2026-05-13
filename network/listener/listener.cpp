#include "pch.h"

static_assert(NETWORK_MODULE == 1);

Listener::Listener(Network* _networkMoudle)
	: ConnectionManager(_networkMoudle, EThreadPool::ASIO)
{
	m_afterShutdown = [this]()
		{
			// ----------------------------------------------------------------------------------
			// no more listen
			// ----------------------------------------------------------------------------------
			for (auto& acceptorInfo : m_acceptorInfos)
			{
				if (not acceptorInfo.acceptor.is_open())
				{
					continue;
				}

				acceptorInfo.acceptor.cancel();	// refer to AcceptorInfo declare
				acceptorInfo.acceptor.close();	// refer to AcceptorInfo declare
			}

			CloseAllAndWait(true);
			CloseAllAndWait(false);

			ShutdownThreadPool();

			Log("Shutdown Listener");
		};
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
		asio::ip::tcp::socket* socket = new asio::ip::tcp::socket(*GetAsioContext());
		const ConnectionId_t connId{ m_networkMoudle.MakeConnectionId() };
		const ConnectionShared_t conn =
			SocketConnectionImpl::Create(socket, connId, GetAsioContext(), acceptorInfo.isPublic, index, *this,
				acceptorInfo.acceptedConfig.receivedHandler, acceptorInfo.acceptedConfig.closedHandler);

		if (not AddConnection(conn))
		{
			return EError::FailedAddConnection;
		}

		if (not IsShutdown())
		{
			m_acceptorInfos[found->second].acceptor.async_accept(*socket, std::bind(&Listener::OnAccepted, this, std::placeholders::_1, socket, found->second, conn));
		}
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

	if (not m_acceptorInfos[found->second].isPublic || not m_acceptorInfos[found->second].acceptor.is_open())
	{
		return;
	}
	m_acceptorInfos[found->second].acceptor.cancel();	// refer to AcceptorInfo declare
	m_acceptorInfos[found->second].acceptor.close();	// refer to AcceptorInfo declare
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

void Listener::OnAccepted(const asio::error_code& _error, asio::ip::tcp::socket* _socket, const AcceptorIndex& _acceptorIndex, ConnectionShared_t _conn)
{
	auto& acceptorInfo = m_acceptorInfos[_acceptorIndex];

	Result result;
	if (!_error)
	{
		std::string remoteAddr = _socket->remote_endpoint().address().to_string();

		SocketConnectionImpl* impl = static_cast<SocketConnectionImpl*>(_conn.get());
		impl->SetRemoteAddress(remoteAddr);

		bool handlerResult = (*acceptorInfo.acceptedConfig.acceptedHandler)(result, _conn);

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
		result = EError::FailedNetworkAccept;

		(*acceptorInfo.acceptedConfig.acceptedHandler)(result, nullptr);

		_conn->Close(result);
	}

	if (acceptorInfo.acceptor.is_open())
	{
		asio::ip::tcp::socket* newSocket = new asio::ip::tcp::socket(*GetAsioContext());

		const ConnectionId_t connId = m_networkMoudle.MakeConnectionId();
		const ConnectionShared_t newConn =
			SocketConnectionImpl::Create(newSocket, connId, GetAsioContext(), acceptorInfo.isPublic, _acceptorIndex, *this,
				acceptorInfo.acceptedConfig.receivedHandler, acceptorInfo.acceptedConfig.closedHandler);

		if (not AddConnection(newConn))
		{
			(*acceptorInfo.acceptedConfig.acceptedHandler)(Result(EError::FailedAddConnection, "failed to restart listen"), nullptr);
			return;
		}

		acceptorInfo.acceptor.async_accept(*newSocket, std::bind(&Listener::OnAccepted, this, std::placeholders::_1, newSocket, _acceptorIndex, newConn));
	}
}
