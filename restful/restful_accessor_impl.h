#pragma once

class Restful;
class RestfulAccessorImpl : public RestfulAccessor
{
public:
	static RestfulAccessorImpl* Create(Restful* _networkModule)
	{
		return new RestfulAccessorImpl(_networkModule);
	}
	~RestfulAccessorImpl() = default;

	virtual Result SetRequestHandler(const std::string& _listenerName, RequestHandler_t&& _handler) final;
	virtual void Response(const RestfulRequestId_t& _requestId, nlohmann::json&& _reply) final;

private:
	RestfulAccessorImpl(Restful* _restfulModule) : m_restfulModule(*_restfulModule) {};

	Restful& m_restfulModule;
};