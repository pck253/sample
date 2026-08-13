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
class SocketConnectionImpl final : public Connection
{
public:
    static ConnectionShared_t Create(asio::ip::tcp::socket* _socket, const ConnectionId_t _connectionId, asio::io_context* _ioContext, const bool& _isPublic,
        const AcceptorIndex& _acceptorIndex, ConnectionManager& _connectionManager, const ReceivedHandler_t _receivedHandler)
    {
        auto conn = ConnectionShared_t(new SocketConnectionImpl(_socket, _connectionId, _ioContext, _isPublic, _acceptorIndex, _connectionManager, _receivedHandler));
        return conn;
    }
    ~SocketConnectionImpl() override;

    AcceptorIndex GetAcceptorIndex() { return m_acceptorIndex; }
    void SetRemoteAddress(const std::string& _remoteAddress) { m_remoteAddress = _remoteAddress; }

    Result Send(const PacketSize_t& _size, const uint8_t* _serializedData, const PacketDeallocatorShared_t& _deallocator) override;
    Result Close(const Result& _reason = Result(EError::Success)) override;
    void InitReceive()
    {
        asio::post(m_strand, [self = Get(), this]() mutable
            {
                if (m_receiveStarted)
                {
                    return;
                }
                m_receiveStarted = true;
                Receive(std::move(self));
            });
    }

private:
    SocketConnectionImpl(asio::ip::tcp::socket* _socket, const ConnectionId_t _connectionId, asio::io_context* _ioContext,
		const bool& _isPublic, const AcceptorIndex& _acceptorIndex, ConnectionManager& _connectionManager, const ReceivedHandler_t _receivedHandler);

    void Receive(ConnectionShared_t&& _self);

    void OnSend(ConnectionShared_t&& _self);
    void OnSent(const asio::error_code& _error, const size_t _bytesTransferred, ConnectionShared_t&& _self);
    void OnReceived(const asio::error_code& _error, const size_t _bytesTransferred, ConnectionShared_t&& _self);

private:
    asio::io_context* m_ioContext{};
    asio::ip::tcp::socket* m_socket{};
    asio::strand<asio::io_context::executor_type> m_strand;	// m_strand is used only for socket operation initiation and shutdown/close.
    const AcceptorIndex m_acceptorIndex;

    ConnectionManager& m_connectionManager;

    std::atomic<size_t> m_packetBunchBytes{};		// this is used to call OnSend on successful send(=OnSent) or when not be sending.
    std::atomic_uint32_t m_packetBunchCount{};	// this is used to move m_packetBunch to m_reservedSendDatas.
    Concurrency::concurrent_queue<PacketInfo*> m_packetBunch;

    ReservedSendData_t m_reservedSendDatas;
    SendBuffer m_sendBuffer;

    ReceiveBuffer m_receiveBuffer;
    bool m_receiveStarted{};				// strand-confined

    ReceivedHandler_t m_receivedHandler{};

#ifdef _DEBUG
	std::atomic_uint32_t m_onSendCallCount = 0;
#endif
};

