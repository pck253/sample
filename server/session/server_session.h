#pragma once

class ServerSession : public ServerSessionBase<ServerSession>
{
public:
    using Super_t = ServerSessionBase<ServerSession>;
    using Shared_t = std::shared_ptr<ServerSession>;
    using ServerPacketHandlerCallers_t = std::unordered_map<ServerTest::Protocol, FbPacketHandleCallerBase<ServerSessionShared_t, ServerTest::Body>*>;

    static Shared_t Create(ConnectionShared_t _conn, ThreadPool& _threadPool)
    {
        return Shared_t(new ServerSession(_conn, _threadPool));
    }

    ~ServerSession();

    inline void Closed() { Shutdown(); }

    static void InitPacketHandlers();
    static void UninitPacketHandlers();
    bool CallPacketHandler(our::vector<uint8_t>&& _rawData);

private:
    ServerSession(ConnectionShared_t _conn, ThreadPool& _threadPool);

    static ServerPacketHandlerCallers_t m_serverPacketHandlerCallers;
};