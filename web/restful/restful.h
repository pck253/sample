#pragma once

static_assert(WEB_MODULE == 1);

enum class EListenerState : uint8_t
{
    None = 0,
    Opening,	// SetRestfulHandler is running. the shutdown handler waits on this.
    Opened,
    Closed,
};

struct RestfulListenerInfo
{
    RestfulListenerInfo(std::unique_ptr<http_listener>&& _listener)
        : listener(std::move(_listener))
    {
    }

    std::unique_ptr<http_listener> listener;
    std::atomic<EListenerState> state = EListenerState::None;
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

protected:
    virtual void RegisterShutdownSteps(ShutdownCoordinator& _coordinator) override;

private:
    Web& m_webModule;

	std::atomic_bool m_initialized = false;
	std::unordered_map<std::string, RestfulListenerInfo> m_restfulListeners;

    std::atomic<RestufulRequestId_t> m_restfulReqIdSequence{ std::numeric_limits<RestufulRequestId_t>::min() };
	std::shared_mutex m_restfulRequestMutex;
	std::unordered_map<RestufulRequestId_t, http_request> m_waitRestfulRequests;

	std::atomic<int32_t> m_inFlightRequests{ 0 };

	std::vector<std::pair<std::string, pplx::task<void>>> m_closingListeners;
};