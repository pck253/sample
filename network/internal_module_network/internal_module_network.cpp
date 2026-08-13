#include "pch.h"

static_assert(NETWORK_MODULE == 1);

ImnManager::ImnManager(Network* _networkMoudle)
	: ConnectionManager(_networkMoudle, EThreadPool::CV)
{
}

void ImnManager::RegisterShutdownSteps(ShutdownCoordinator& _coordinator)
{
	PushCloseConnectionSteps(_coordinator, "imn", false);

	PushShutdownThreadPoolSteps(_coordinator, "imn");
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

	auto const& acceptedConfig{ found->second.acceptedConfig };
	m_cvThreadPool.PushJob([_imnName, &acceptedConfig, connectedConfig = _connectedConfig, this]() mutable
		{
			ConnectionId_t connectionId = m_networkMoudle.MakeConnectionId();
			const ConnectionShared_t conn = ImnConnectionImpl::Create(connectionId, *this, connectedConfig.receivedHandler);
			ImnConnectionImpl* imnConn = static_cast<ImnConnectionImpl*>(conn.get());

			if (not AddConnection(conn, connectedConfig.closedHandler, [imnConn, this]()
				{
					imnConn->InitJobQueue(m_cvThreadPool);
				}))
			{
				(*connectedConfig.connectedHandler)(EError::FailedAddConnection, _imnName, nullptr);
				return;
			}

			ConnectionId_t targetConnectionId = m_networkMoudle.MakeConnectionId();
			ConnectionShared_t targetConn = ImnConnectionImpl::Create(targetConnectionId, *this, acceptedConfig.receivedHandler);
			ImnConnectionImpl* imnTargetConn = static_cast<ImnConnectionImpl*>(targetConn.get());

			if (not AddConnection(targetConn, acceptedConfig.closedHandler, [imnTargetConn, this]()
				{
					imnTargetConn->InitJobQueue(m_cvThreadPool);
				}))
			{
				(*acceptedConfig.acceptedHandler)(EError::FailedAddConnection, nullptr);

				(*connectedConfig.connectedHandler)(EError::FailedAddConnection, _imnName, nullptr);
				conn->Close(EError::FailedAddConnection);
				return;
			}

			imnConn->SetTargetConnection(targetConn);
			imnTargetConn->SetTargetConnection(conn);

			// dose not change the order of the following two lines.
			(*acceptedConfig.acceptedHandler)(EError::Success, targetConn);
			(*connectedConfig.connectedHandler)(EError::Success, _imnName, conn);
		});

	return EError::Success;
}
