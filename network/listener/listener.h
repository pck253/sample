#pragma once
class Network;

struct AcceptorInfo
{
    ip::tcp::endpoint address;
    ip::tcp::acceptor acceptor;
    bool isPublic = false;
    AcceptedHandler_t acceptedHandler = nullptr;
};

class Listener : public ConnectionManager
{
public:
    Listener(Network* _networkMoudle);
    virtual ~Listener();

    Result Init(our::vector<std::tuple<std::string, ip::tcp::endpoint, bool>>& _addresses, const uint16_t& _threadCount);
    void Shutdown();

    Result SetAcceptedHandler(const std::string& _listenerName, AcceptedHandler_t&& _handler);

    void StopPublicListen(const std::string& _listenerName);
    void ClosePublicConnection(const std::string& _listenerName);

private:
    void OnAccepted(const error_code& _error, ip::tcp::socket* _socket, const AcceptorIndex& _acceptorIndex, ConnectionShared_t _conn);

private:
    std::shared_mutex m_accetorSettingMutex;
    our::vector<AcceptorInfo> m_acceptorInfos;
    our::unordered_map<std::string, AcceptorIndex> m_acceptorNames;
};