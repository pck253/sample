#pragma once

static_assert(NETWORK_MODULE == 1);

class ImnManager;
class ConnectionManager;
class ImnConnectionImpl : public Connection
{
    friend ImnManager;
public:
    static ConnectionShared_t Create(const ConnectionId_t _connectionId, ConnectionManager& _connectionManager, ThreadPool& _threadPool,
        const ReceivedHandler_t _receivedHandler, const ClosedHandler_t _closedHandler)
    {
        auto conn = ConnectionShared_t(new ImnConnectionImpl(_connectionId, _connectionManager, _threadPool, _receivedHandler, _closedHandler));
        return conn;
    }
    virtual ~ImnConnectionImpl();

    virtual Result Send(const PacketSize_t& _size, const uint8_t* _serializedData, const PacketDeallocatorShared_t& _deallocator) final;
    virtual Result Close(const Result& _reason = Result(EError::Success)) final;

    Result Receive(std::vector<uint8_t>&& _rawData);

private:
    explicit ImnConnectionImpl(const ConnectionId_t _connectionId, ConnectionManager& _connectionManager, ThreadPool& _threadPool,
        const ReceivedHandler_t _receivedHandler, const ClosedHandler_t _closedHandler);

    void SetTargetConnection(ConnectionShared_t& _targetConnection) { m_targetConnection = _targetConnection; }
    ConnectionShared_t& GetTargetConnection() { return m_targetConnection; }

    void OnSent(const Result& _error, const size_t& _bytesTransferred);
    void OnReceived(std::vector<uint8_t>&& _rawData);

private:
    ConnectionManager& m_connectionManager;

    SerializedJobQueueShared_t m_jobQueue;          // job from current connection
    SerializedJobQueueShared_t m_receivedJobQueue;  // OnReceived
    ConnectionShared_t m_targetConnection;

    ReceivedHandler_t m_receivedHandler;
    ClosedHandler_t m_closedHandler;
};

