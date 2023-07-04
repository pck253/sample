#pragma once

class Session : public SerializedJobQueue
{   
    using Super_t = SerializedJobQueue;
public:
    using PacketHandlerCallers_t = std::unordered_map<Client::Protocol, FbPacketHandleCallerBase<SessionShared_t, Client::Body>*>;

    static SessionShared_t Create(ConnectionShared_t _conn, ThreadPool& _threadPool)
    {
        return SessionShared_t(new Session(_conn, _threadPool));
    }

    ~Session();

    Result Send(const PacketSize_t& _size, const uint8_t* _serializedData, PacketDeallocatorShared_t& _deallocator);
    void Close(const Result& _reason);
    void Closed(const Result& _result);

    static void InitPacketHandlers();
    static void UninitPacketHandlers();
    bool CallPacketHandler(our::vector<uint8_t>&& _rawData);

    void SetReceivedInfo(const size_t& _receivedByte)
    {
        m_receivedByte += _receivedByte;
        if (m_latestReceiveTime != 0)
        {
            auto now = GET_PROFILE_TICK();
            if (std::chrono::milliseconds(now - m_latestReceiveTime) >= std::chrono::milliseconds(5000))
            {
                Log("{} received.", m_receivedByte);
                m_receivedByte = 0;
            }
        }
        m_latestReceiveTime = GET_PROFILE_TICK();
    }

    void SendTestMessage();

private:
    Session(ConnectionShared_t _conn, ThreadPool& _threadPool);

    ConnectionShared_t m_connection;

    static PacketHandlerCallers_t m_packetHandlerCallers;

    ProfileTickTime_t m_latestReceiveTime = 0;
    size_t m_receivedByte = 0;

    std::optional<uint64_t> m_testMessageSeq = 0;
};