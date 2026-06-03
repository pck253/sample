#pragma once

static_assert(NETWORK_MODULE == 1);

struct PacketInfo : public MemPoolInstance
{
	const uint8_t* serializedData = nullptr;
	PacketSize_t size = 0;
	PacketDeallocatorShared_t deallocator;

	PacketInfo() {};
	explicit PacketInfo(const uint8_t* _serializedData, const PacketSize_t& _size, const PacketDeallocatorShared_t& _deallocator)
	{
		serializedData = _serializedData;
		size = _size;
		deallocator = _deallocator;
	}

	explicit PacketInfo(const PacketInfo& _other)
	{
		serializedData = _other.serializedData;
		size = _other.size;
		deallocator = _other.deallocator;
	}

	explicit PacketInfo(PacketInfo&& _other) noexcept
	{
		serializedData = _other.serializedData;
		size = _other.size;
		deallocator = std::move(_other.deallocator);

		_other.serializedData = nullptr;
		_other.size = 0;
	}

	~PacketInfo()
	{
	}

	PacketInfo& operator=(const PacketInfo& _other)
	{
		serializedData = _other.serializedData;
		size = _other.size;
		deallocator = _other.deallocator;

		return *this;
	}

	PacketInfo& operator=(PacketInfo&& _other) noexcept
	{
		serializedData = _other.serializedData;
		size = _other.size;
		deallocator = std::move(_other.deallocator);

		_other.serializedData = nullptr;
		_other.size = 0;
		return *this;
	}
};

class ConnectionManager;
class SocketConnectionImpl : public Connection
{
public:
    static ConnectionShared_t Create(asio::ip::tcp::socket* _socket, const ConnectionId_t _connectionId, asio::io_context* _ioContext, const bool& _isPublic,
        const AcceptorIndex& _acceptorIndex, ConnectionManager& _connectionManager, const ReceivedHandler_t _receivedHandler, const ClosedHandler_t _closedHandler)
    {
        auto conn = ConnectionShared_t(new SocketConnectionImpl(_socket, _connectionId, _ioContext, _isPublic, _acceptorIndex, _connectionManager, _receivedHandler, _closedHandler));
        return conn;
    }
    virtual ~SocketConnectionImpl();

    AcceptorIndex GetAcceptorIndex() { return m_acceptorIndex; }
    void SetRemoteAddress(const std::string& _remoteAddress) { m_remoteAddress = _remoteAddress; }

    virtual Result Send(const PacketSize_t& _size, const uint8_t* _serializedData, const PacketDeallocatorShared_t& _deallocator) final;
    virtual Result Close(const Result& _reason = Result(EError::Success)) final;
    void InitReceive()
    {
        std::call_once(m_initReceiveFlag, [this]() {
				auto self = Get();
                Receive(self);
            });
    }

private:
    SocketConnectionImpl(asio::ip::tcp::socket* _socket, const ConnectionId_t _connectionId, asio::io_context* _ioContext,
		const bool& _isPublic, const AcceptorIndex& _acceptorIndex, ConnectionManager& _connectionManager, const ReceivedHandler_t _receivedHandler, const ClosedHandler_t _closedHandler);

    void Receive(ConnectionShared_t& _self);

    void OnSend(ConnectionShared_t& _self);
    void OnSent(const asio::error_code& _error, const size_t _bytesTransferred, ConnectionShared_t& _self);
    void OnReceived(const asio::error_code& _error, const size_t _bytesTransferred, ConnectionShared_t& _self);

private:
    asio::io_context* m_ioContext = nullptr;
	asio::ip::tcp::socket* m_socket = nullptr;
	const AcceptorIndex m_acceptorIndex;
	std::atomic_bool m_isClosed = false;

    ConnectionManager& m_connectionManager;

	std::atomic<size_t> m_packetBunchBytes = 0;		// this is used to call OnSend on successful send(=OnSent) or when not be sending.
    std::atomic_uint32_t m_packetBunchCount = 0;	// this is used to move m_packetBunch to m_reservedSendDatas.
    Concurrency::concurrent_queue<PacketInfo*> m_packetBunch;

    ReservedSendData_t m_reservedSendDatas;
    SendBuffer m_sendBuffer;

    ReceiveBuffer m_receiveBuffer;
    std::once_flag m_initReceiveFlag;

	ReceivedHandler_t m_receivedHandler;
    ClosedHandler_t m_closedHandler;

#ifdef _DEBUG
	std::atomic_uint32_t m_onSendCallCount = 0;
#endif
};

