#pragma once

static_assert(NETWORK_MODULE == 1);

class ImnJobQueue final : public SerializedJobQueue
{
    friend SerializedJobQueue;
    using Super_t = SerializedJobQueue;
public:
    ~ImnJobQueue() = default;

    const auto& GetConnection() const { return m_connection; }
    void ReleaseConnection() { m_connection.reset(); }

private:
    ImnJobQueue(ThreadPool& _threadPool, const ConnectionShared_t& _conn);

private:
    ConnectionShared_t m_connection;
};

//-----------------------------------------------------------------------------

class ImnManager;
class ConnectionManager;
class ImnConnectionImpl final : public Connection
{
    friend ImnManager;
public:
    static ConnectionShared_t Create(const ConnectionId_t _connectionId, ConnectionManager& _connectionManager, const ReceivedHandler_t _receivedHandler)
    {
        return ConnectionShared_t(new ImnConnectionImpl(_connectionId, _connectionManager, _receivedHandler));
    }
    ~ImnConnectionImpl() override;

    void InitJobQueue(ThreadPool& _threadPool);

    virtual Result Send(const PacketSize_t& _size, const uint8_t* _serializedData, const PacketDeallocatorShared_t& _deallocator) override;
    virtual Result Close(const Result& _reason = Result(EError::Success)) override;

    Result Receive(std::vector<uint8_t>&& _rawData);

private:
    explicit ImnConnectionImpl(const ConnectionId_t _connectionId, ConnectionManager& _connectionManager, const ReceivedHandler_t _receivedHandler);

    void SetTargetConnection(const ConnectionShared_t& _targetConnection) { m_targetConnection = _targetConnection; }
    ConnectionShared_t& GetTargetConnection()  { return m_targetConnection; }

    void OnSent(const Result& _error, const size_t _bytesTransferred);

private:
    ConnectionManager& m_connectionManager;

    std::shared_ptr<ImnJobQueue> m_jobQueue;          // job from current connection
    ConnectionShared_t m_targetConnection;

    ReceivedHandler_t m_receivedHandler;
};

