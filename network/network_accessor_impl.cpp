#include "pch.h"


Result NetworkAccessorImpl::RegistAcceptedHandler(const std::string& _listenerName, AcceptedHandler_t&& _handler)
{
	return m_networkModule.GetListener().SetAcceptedHandler(_listenerName, std::move(_handler));
}

Result NetworkAccessorImpl::RequestConnect(const std::string& _address, const uint16_t& _port, ConnectedHandler_t&& _handler)
{
	return m_networkModule.GetConnecter().RequestConnect(_address, _port, std::move(_handler));
}

Result NetworkAccessorImpl::RequestConnect(const std::string& _connecterName, ConnectedHandler_t&& _handler, const uint16_t& _tryReconnectCount)
{
	return m_networkModule.GetConnecter().RequestConnect(_connecterName, std::move(_handler), _tryReconnectCount);
}

void NetworkAccessorImpl::StopPublicListen(const std::string& _listenerName)
{
	m_networkModule.GetListener().StopPublicListen(_listenerName);
}

void NetworkAccessorImpl::ClosePublicConnection(const std::string& _listenerName)
{
	m_networkModule.GetListener().ClosePublicConnection(_listenerName);
}