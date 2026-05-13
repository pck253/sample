#include "pch.h"

static_assert(WEB_MODULE == 1);

Result WebAccessorImpl::SetRestfulHandler(const std::string& _listenerName, const RestfulHandler_t _handler)
{
	return m_webModule.GetRestful().SetRestfulHandler(_listenerName, _handler);
}

void WebAccessorImpl::Response(const RestufulRequestId_t& _requestId, nlohmann::json&& _reply)
{
	return m_webModule.GetRestful().Response(_requestId, std::move(_reply));
}