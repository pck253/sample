#pragma once

using ConnectionId_t = uint64_t;
#define INVALID_CONNECTION_ID 0

using PacketSize_t = uint16_t;

constexpr size_t PACKET_SIZE_BYTE = sizeof(PacketSize_t);
constexpr size_t MAX_PACKET_SIZE = std::numeric_limits<PacketSize_t>::max();	// include header size

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
    explicit PacketDeallocator(std::function<void()>&& _deallocator) noexcept
        : m_deallocator(std::move(_deallocator)) {}

    std::function<void()> m_deallocator;
};
using PacketDeallocatorShared_t = std::shared_ptr<PacketDeallocator>;

class Connection;
using ConnectionShared_t = std::shared_ptr<Connection>;

using ReceivedHandler_t = void(*)(std::vector<uint8_t>&&, const ConnectionShared_t&);
using ClosedHandler_t = void(*)(const Result&, const ConnectionId_t, const bool);

class Connection : public std::enable_shared_from_this<Connection>
{
public:
    ConnectionShared_t Get() {
        return shared_from_this();
    }
    virtual ~Connection() = default;

public:
    inline const std::string& GetRemoteAddress() const { return m_remoteAddress; }
    inline ConnectionId_t GetConnectionId() const { return m_connectionId; }
    inline bool IsPublic() const { return m_isPublic; }

    // _deallocator : for _serializedData
    virtual Result Send(const PacketSize_t& _size, const uint8_t* _serializedData, const PacketDeallocatorShared_t& _deallocator) = 0;
    virtual Result Close(const Result& _reason = Result(EError::Success)) = 0;

protected:
    explicit Connection(const ConnectionId_t& _connectionId, const bool& _isPublic)
        : m_connectionId(_connectionId), m_isPublic(_isPublic)
    {
    }

protected:
    std::atomic_bool m_isClosed = false;
    std::string m_remoteAddress;
    const ConnectionId_t m_connectionId;
    const bool m_isPublic;
};

