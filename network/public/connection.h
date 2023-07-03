#pragma once

using ConnectionId_t = uint64_t;
using PacketSize_t = uint16_t;

class PacketDeallocator : public std::enable_shared_from_this<PacketDeallocator>
{
public:
    static std::shared_ptr<PacketDeallocator> Create(std::function<void()>&& _deallocator)
    {
        return std::shared_ptr<PacketDeallocator>(new PacketDeallocator(std::move(_deallocator)));
    }

    ~PacketDeallocator()
    {
        m_deallocator();
    }

private:
    PacketDeallocator(std::function<void()>&& _deallocator)
        : m_deallocator(std::move(_deallocator)) {}

    std::function<void()> m_deallocator;
};
using PacketDeallocatorShared_t = std::shared_ptr<PacketDeallocator>;

#define INVALID_CONNECTION_ID 0

class Connection;
using ConnectionShared_t = std::shared_ptr<Connection>;

using SentHandler_t = std::function<void(const size_t&)>;
using ReceivedHandler_t = std::function<void(our::vector<uint8_t>&&, ConnectionShared_t)>;
using ClosedHandler_t = std::function<void(const Result&, const ConnectionId_t&, const bool&)>;

class Connection : public std::enable_shared_from_this<Connection>
{
public:
    ConnectionShared_t Get() {
        return shared_from_this();
    }
    virtual ~Connection() = default;

public:
    inline const std::string& GetRemoteAddress() { return m_remoteAddress; }
    inline ConnectionId_t GetConnectionId() { return m_connectionId; }
    inline bool IsPublic() { return m_isPublic; }

    // _deallocator : for _serializedData
    virtual Result Send(const PacketSize_t& _size, const uint8_t* _serializedData, PacketDeallocatorShared_t& _deallocator) = 0;
    virtual Result Close(const Result& _reason = Result(EError::Success)) = 0;

public:
    virtual inline void SetSentHandler(SentHandler_t&& _sentHandler) = 0;
    virtual inline void SetReceivedHandler(ReceivedHandler_t&& _receivedHandler) = 0;
    virtual inline void SetClosedHandler(ClosedHandler_t&& _closedHandler) = 0;

protected:
    Connection() = default;

    std::string m_remoteAddress;
    ConnectionId_t m_connectionId = INVALID_CONNECTION_ID;
    bool m_isPublic = false;
};

