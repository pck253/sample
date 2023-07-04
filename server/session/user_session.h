#pragma once

class UserSession : public SerializedJobQueue
{   
    using Super_t = SerializedJobQueue;
public:
    using PacketHandlerCallers_t = std::unordered_map<Client::Protocol, FbPacketHandleCallerBase<UserSessionShared_t, Client::Body>*>;

    static UserSessionShared_t Create(ConnectionShared_t _conn, ThreadPool& _threadPool)
    {
        return UserSessionShared_t(new UserSession(_conn, _threadPool));
    }

    ~UserSession();

    Result Send(const PacketSize_t& _size, const uint8_t* _serializedData, PacketDeallocatorShared_t& _deallocator);
    void Close(const Result& _reason);
    void Closed(const Result& _result);

    static void InitPacketHandlers();
    static void UninitPacketHandlers();
    bool CallPacketHandler(our::vector<uint8_t>&& _rawData);

private:
    UserSession(ConnectionShared_t _conn, ThreadPool& _threadPool);

    ConnectionShared_t m_connection;

    static PacketHandlerCallers_t m_packetHandlerCallers;
};