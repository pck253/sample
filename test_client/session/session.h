#pragma once

class Session : public SerializedJobQueue
{
    friend SerializedJobQueue;
    using Super_t = SerializedJobQueue;
public:
    using PacketHandlerCallers_t = std::unordered_map<Client::EProtocol, ZppBitsPacketHandleCallerBase<SessionShared_t>*>;

    ~Session();

    Result Send(const PacketSize_t& _size, const uint8_t* _serializedData, const PacketDeallocatorShared_t& _deallocator);
    void Close(const Result& _reason);
    void Closed(const Result& _result);

    static void InitPacketHandlers();
    static void UninitPacketHandlers();
    bool CallPacketHandler(std::vector<uint8_t>&& _rawData);

    void ReceivedStatics(const PacketSize_t& _receivedSize);

private:
    explicit Session(ThreadPool& _threadPool, ConnectionShared_t _conn);

    ConnectionShared_t m_connection;

    static inline PacketHandlerCallers_t m_packetHandlerCallers;

    // --------------------
    uint64_t m_receivedCount = 0;
    uint64_t m_receivedByte = 0;
    TickTime_t m_latestReceived;
};