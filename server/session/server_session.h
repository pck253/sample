#pragma once

class ServerSession : public ServerSessionBase<ServerSession>
{
    friend SerializedJobQueue;
public:
    using Super_t = ServerSessionBase<ServerSession>;
    using Shared_t = std::shared_ptr<ServerSession>;
    using ServerPacketHandlerCallers_t = std::unordered_map<ServerTest::EProtocol, ZppBitsPacketHandleCallerBase<ServerSessionShared_t>*>;

    ~ServerSession();

    inline void Closed() { Shutdown(EShutdownMode::CurrentJob, "server session shutdown."); }

    static void InitPacketHandlers();
    static void UninitPacketHandlers();
    bool CallPacketHandler(std::vector<uint8_t>&& _rawData);

private:
    ServerSession(ThreadPool& _threadPool, ConnectionShared_t _conn);

    static inline ServerPacketHandlerCallers_t m_serverPacketHandlerCallers;
};