#pragma once

struct ListenerInfo
{
    ListenerInfo(http_listener* _listener)
        : listener(_listener)
    {
    }
    ListenerInfo(ListenerInfo&& _other)
    {
        listener = _other.listener;
        nowUsing = _other.nowUsing.load();

    }
    ListenerInfo& operator = (ListenerInfo&& _other)
    {
        listener = _other.listener;
        nowUsing = _other.nowUsing.load();
        return *this;
    }
    http_listener* listener = nullptr;
    std::atomic_bool nowUsing = false;
};

class Restful : public Module
{
    friend Module;
public:
    ~Restful();

    virtual bool IsBusinessModule() final { return false; }
    virtual EModule GetModuleType() final { return EModule::Restful; }
    virtual const char* GetModuleName() final { return "Restful"; }

    virtual void Shutdown() final;

    Result SetRequestHandler(const std::string& _listenerName, RequestHandler_t&& _handler);
    void Response(const RestfulRequestId_t& _requestId, nlohmann::json&& _reply);

private:
    Restful(const std::string& _configFilePath);

    virtual Result InitImpl() final;

private:
    std::atomic_bool m_shutdown = false;

    our::unordered_map<std::string, ListenerInfo> m_listeners;

    std::atomic<RestfulRequestId_t> m_RequestIdSequence = INVALID_RESTFUL_ID;
    std::shared_mutex m_waitRequestMutex;
    our::unordered_map<RestfulRequestId_t, http_request> m_waitRequests;
};