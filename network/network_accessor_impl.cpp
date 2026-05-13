#include "pch.h"

static_assert(NETWORK_MODULE == 1);

Result NetworkAccessorImpl::RegistAcceptedConfig(const std::string& _listenerName, const AcceptedConfig& _acceptedConfig)
{
	if (m_networkModule.IsRedirectToImn(_listenerName))
	{
		return m_networkModule.GetImnManager().SetAcceptedConfig(_listenerName, _acceptedConfig);
	}
	return m_networkModule.GetListener().SetAcceptedConfig(_listenerName, _acceptedConfig);
}

Result NetworkAccessorImpl::RequestConnect(const std::string& _address, const uint16_t& _port, const ConnectedConfig& _connectedConfig)
{
	return m_networkModule.GetConnecter().RequestConnect(_address, _port, _connectedConfig);
}

Result NetworkAccessorImpl::RequestConnect(const std::string& _connecterName, const ConnectedConfig& _connectedConfig, const uint16_t& _tryReconnectCount)
{
	if (m_networkModule.IsRedirectToImn(_connecterName))
	{
		return m_networkModule.GetImnManager().RequestConnect(_connecterName, _connectedConfig);
	}
	return m_networkModule.GetConnecter().RequestConnect(_connecterName, _connectedConfig, _tryReconnectCount);
}

void NetworkAccessorImpl::StopPublicListen(const std::string& _listenerName)
{
	m_networkModule.GetListener().StopPublicListen(_listenerName);
}

void NetworkAccessorImpl::ClosePublicConnection(const std::string& _listenerName)
{
	m_networkModule.GetListener().ClosePublicConnection(_listenerName);
}