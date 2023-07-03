#include "pch.h"

Result RestfulAccessorImpl::SetRequestHandler(const std::string& _listenerName, RequestHandler_t&& _handler)
{
	return m_restfulModule.SetRequestHandler(_listenerName, std::move(_handler));
}

void RestfulAccessorImpl::Response(const RestfulRequestId_t& _requestId, nlohmann::json&& _reply)
{
	return m_restfulModule.Response(_requestId, std::move(_reply));
}