#include "pch.h"

static_assert(NETWORK_MODULE == 1);

ImnManager::ImnManager(Network* _networkMoudle)
	: ConnectionManager(_networkMoudle, EThreadPool::CV)
{
	m_afterShutdown = [this]()
		{
			CloseAllAndWait(false);

			ShutdownThreadPool();

			Log("Shutdown ImnManager");
		};
}

ImnManager::~ImnManager()
{
}

Result ImnManager::Init(const std::set<std::string>& _imns, const uint16_t& _threadCount)
{
	InitThreadPool(_threadCount);

	std::ranges::for_each(_imns, [this](const auto& _name)
		{
			m_imnInfos.try_emplace(_name);
		});

	return EError::Success;
}

Result ImnManager::SetAcceptedConfig(const std::string& _imnName, const AcceptedConfig& _acceptedConfig)
{
	auto found = m_imnInfos.find(_imnName);
	if (found == m_imnInfos.end())
	{
		return EError::NotExistNetworkImn;
	}

	if (not _acceptedConfig.IsValid())
	{
		return EError::NeedAcceptedHandler;
	}

	if (not found->second.acceptedConfig.Set(_acceptedConfig.acceptedHandler, _acceptedConfig.receivedHandler, _acceptedConfig.closedHandler))
	{
		return EError::AlreadySettedHandler;
	}

	return EError::Success;
}

Result ImnManager::RequestConnect(const std::string& _imnName, const ConnectedConfig& _connectedConfig)
{
	auto found = m_imnInfos.find(_imnName);
	if (found == m_imnInfos.end())
	{
		return EError::NotExistNetworkImn;
	}

	if (not _connectedConfig.IsValid())
	{
		return EError::NeedConnectedHandler;
	}

	ConnectionId_t connectionId = m_networkMoudle.MakeConnectionId();
	ConnectionShared_t conn = ImnConnectionImpl::Create(connectionId, *this, m_cvThreadPool, _connectedConfig.receivedHandler, _connectedConfig.closedHandler);

	if (not AddConnection(conn))
	{
		return EError::FailedAddConnection;
	}

	auto const& acceptedConfig{ found->second.acceptedConfig };
	m_cvThreadPool.PushJob([_imnName, conn, &acceptedConfig, connectedConfig = _connectedConfig, this]() mutable
		{
			ConnectionId_t connectionId = m_networkMoudle.MakeConnectionId();
			ConnectionShared_t targetConn = ImnConnectionImpl::Create(connectionId, *this, m_cvThreadPool, acceptedConfig.receivedHandler, acceptedConfig.closedHandler);

			if (not AddConnection(conn))
			{
				(*connectedConfig.connectedHandler)(EError::FailedAddConnection, _imnName, conn);
				return;
			}

			ImnConnectionImpl* imnTargetConn = static_cast<ImnConnectionImpl*>(targetConn.get());
			imnTargetConn->SetTargetConnection(conn);
			(*acceptedConfig.acceptedHandler)(EError::Success, targetConn);

			ImnConnectionImpl* imnConn = static_cast<ImnConnectionImpl*>(conn.get());
			imnConn->SetTargetConnection(targetConn);
			(*connectedConfig.connectedHandler)(EError::Success, _imnName, conn);
		});

	return EError::Success;
}
