#pragma once
constexpr size_t PACKET_SIZE_BYTE = sizeof(PacketSize_t);
constexpr size_t MAX_PACKET_SIZE = 65535;	// not include header size

struct Packet
{
	const uint8_t* serializedData = nullptr;
	PacketSize_t size = 0;
	PacketDeallocatorShared_t deallocator;

	Packet() {};
	Packet(const uint8_t* _serializedData, const PacketSize_t& _size, PacketDeallocatorShared_t& _deallocator)
	{
		serializedData = _serializedData;
		size = _size;
		deallocator = _deallocator;
	}

	Packet(const Packet& _other)
	{
		serializedData = _other.serializedData;
		size = _other.size;
		deallocator = _other.deallocator;
	}

	Packet(Packet&& _other)
	{
		serializedData = _other.serializedData;
		size = _other.size;
		deallocator = std::move(_other.deallocator);

		_other.serializedData = nullptr;
		_other.size = 0;
	}

	~Packet()
	{
	}

	Packet& operator=(const Packet& _other)
	{
		serializedData = _other.serializedData;
		size = _other.size;
		deallocator = _other.deallocator;

		return *this;
	}

	Packet& operator=(Packet&& _other)
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
class ConnectionImpl : public Connection
{
public:
    static ConnectionShared_t Create(ip::tcp::socket* _socket, io_context* _ioContext, const bool& _isPublic,
        const AcceptorIndex& _acceptorIndex, ConnectionManager* _connectionManager)
    {
        auto conn = ConnectionShared_t(new ConnectionImpl(_socket, _ioContext, _isPublic, _acceptorIndex, _connectionManager));
        return conn;
    }
    virtual ~ConnectionImpl();

    AcceptorIndex GetAcceptorIndex() { return m_acceptorIndex; }
    void SetRemoteAddress(const std::string& _remoteAddress) { m_remoteAddress = _remoteAddress; }
    void SetConnectionId(const ConnectionId_t& _connectionId) { m_connectionId = _connectionId; }

    virtual Result Send(const PacketSize_t& _size, const uint8_t* _serializedData, PacketDeallocatorShared_t& _deallocator) final;
    virtual Result Close(const Result& _reason = Result(EError::Success)) final;
    void InitReceive()
    {
        std::call_once(m_initReceiveFlag, [this]() {
                Receive();
            });
    }

    virtual inline void SetSentHandler(SentHandler_t&& _sentHandler) final { m_sentHandler = std::move(_sentHandler); }
    virtual inline void SetReceivedHandler(ReceivedHandler_t&& _receivedHandler) final { m_receivedHandler = std::move(_receivedHandler); }
    virtual inline void SetClosedHandler(ClosedHandler_t&& _closedHandler) final { m_closedHandler = std::move(_closedHandler); }

private:
    ConnectionImpl(ip::tcp::socket* _socket, io_context* _ioContext, const bool& _isPublic, const AcceptorIndex& _acceptorIndex, ConnectionManager* _connectionManager);

    void Receive();

    void OnSend(ConnectionShared_t _self);
    void OnSent(const error_code& _error, const size_t& _bytesTransferred, ConnectionShared_t _self);
    void OnReceived(const error_code& _error, const size_t& _bytesTransferred, ConnectionShared_t _self);
    void OnClosed(const Result& _reason, ConnectionShared_t _self);
    void RelaseSentHandler(ConnectionShared_t _self) { m_sentHandler = SentHandler_t(); }

    io_context* m_ioContext = nullptr;
    ip::tcp::socket* m_socket = nullptr;
    AcceptorIndex m_acceptorIndex = NOT_FROM_ACCEPTOR;

    ConnectionManager& m_connectionManager;

    io_context::strand m_strand;

	std::atomic<size_t> m_packetBunchBytes = 0;    // this is used to call OnSend on successful send(=OnSent) or when not be sending.
    std::atomic_uint32_t m_packetBunchCount = 0;
    Concurrency::concurrent_queue<Packet*> m_packetBunch;

    ReservedSendData_t m_reservedSendDatas;
    SendBuffer m_sendBuffer;

    ReceiveBuffer m_receiveBuffer;
    std::once_flag m_initReceiveFlag;

    std::atomic_bool m_isClosed = false;

    SentHandler_t m_sentHandler;
    ReceivedHandler_t m_receivedHandler;
    ClosedHandler_t m_closedHandler;

	std::atomic_uint32_t m_onSendCallCount = 0;
};

