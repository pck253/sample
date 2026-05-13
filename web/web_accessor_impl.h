#pragma once

static_assert(WEB_MODULE == 1);

class Web;
class WebAccessorImpl : public WebAccessor
{
public:
	static WebAccessorImpl* Create(Web* _networkModule)
	{
		return new WebAccessorImpl(_networkModule);
	}
	~WebAccessorImpl() = default;

	virtual Result SetRestfulHandler(const std::string& _listenerName, const RestfulHandler_t _handler) final;
	virtual void Response(const RestufulRequestId_t& _requestId, nlohmann::json&& _reply) final;

private:
	explicit WebAccessorImpl(Web* _webModule) : m_webModule(*_webModule) {};

	Web& m_webModule;
};