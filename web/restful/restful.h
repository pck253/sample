#pragma once

static_assert(WEB_MODULE == 1);

struct RestfulListenerInfo
{
    RestfulListenerInfo(http_listener* _listener)
        : listener(_listener)
    {
    }
    RestfulListenerInfo(RestfulListenerInfo&& _other)
    {
        listener = _other.listener;
        nowUsing = _other.nowUsing.load();

    }
    const RestfulListenerInfo& operator = (RestfulListenerInfo&& _other)
    {
        listener = _other.listener;
        nowUsing = _other.nowUsing.load();
        return *this;
    }
    http_listener* listener = nullptr;
    std::atomic_bool nowUsing = false;
};

class Web;
class Restful : public UseShutdown
{
public:
	Restful(Web& _webModule);
	~Restful();

    Result Init(const nlohmann::json& _config);

    Result SetRestfulHandler(const std::string& _listenerName, const RestfulHandler_t _handler);
    void Response(const RestufulRequestId_t& _requestId, nlohmann::json&& _reply);

private:
    Web& m_webModule;

	std::unordered_map<std::string, RestfulListenerInfo> m_restfulListeners;

    std::atomic<RestufulRequestId_t::IdType> m_restfulReqIdSequence{ RestufulRequestId_t::GetInvalidValue() };
	std::shared_mutex m_restfulRequestMutex;
	std::unordered_map<RestufulRequestId_t, http_request> m_waitRestfulRequests;
};