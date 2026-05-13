#pragma once

static_assert(NETWORK_MODULE == 1);

class Network;

struct AcceptorInfo
{
    asio::ip::tcp::endpoint address;
    /////////////////////////////////////////////////////////////////////////////////////
    // acceptor
    //   cancel() : cancel async job.
    //   close() : disconnect && destroy socket.
    //   release() : cancel && relase ownership of socket, return value is socket handle.
    asio::ip::tcp::acceptor acceptor;
    /////////////////////////////////////////////////////////////////////////////////////
    bool isPublic = false;
    AcceptedConfig acceptedConfig;

    explicit AcceptorInfo(asio::ip::tcp::endpoint const& _address, asio::io_context& _ioContext, const bool& _isPublic)
        : address(_address),
        acceptor(_ioContext, _address),
        isPublic(_isPublic)
    {
    }
    explicit AcceptorInfo(AcceptorInfo&& _other) noexcept
        : address(std::move(_other.address)),
        acceptor(std::move(_other.acceptor)),
        isPublic(std::move(_other.isPublic))
    {
    }
};

class Listener : public ConnectionManager
{
public:
    explicit Listener(Network* _networkMoudle);
    virtual ~Listener();

    Result Init(const std::vector<std::tuple<std::string, asio::ip::tcp::endpoint, bool>>& _addresses, const uint16_t& _threadCount);

    Result SetAcceptedConfig(const std::string& _listenerName, const AcceptedConfig& _acceptedConfig);

    void StopPublicListen(const std::string& _listenerName);
    void ClosePublicConnection(const std::string& _listenerName);

private:
    void OnAccepted(const asio::error_code& _error, asio::ip::tcp::socket* _socket, const AcceptorIndex& _acceptorIndex, ConnectionShared_t _conn);

private:
    std::vector<AcceptorInfo> m_acceptorInfos;
    std::unordered_map<std::string, AcceptorIndex> m_acceptorNames;
};