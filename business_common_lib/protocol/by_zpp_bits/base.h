using Protocol_t = uint16_t;

enum class EServerPacketType : uint8_t
{
    ServerCommon = 0,
    ServerTest = 1,
    Max
};

#define ADD_BASE_ARCHIVE()                     \
    auto ret = Base_t::serialize(ar, self);    \
    if (zpp::bits::failure(ret))               \
    {                                          \
        return ret;                            \
    }                                          \

struct ServerPacketBase
{
    using Base_t = ServerPacketBase;

    EServerPacketType type{};
    Protocol_t rawProtocol{};
    ServerPacketBase() = default;
    explicit ServerPacketBase(const EServerPacketType _type, Protocol_t _rawProtocol)
        : type(_type), rawProtocol(_rawProtocol) {
    }
    
    template <typename Archive, typename Self>
    static zpp::bits::errc serialize(Archive& ar, Self& self)
    {
        return ar(self.type, self.rawProtocol);
    }
};

struct ClientPacketBase
{
    using Base_t = ClientPacketBase;

    Protocol_t rawProtocol{};
    ClientPacketBase() = default;
    explicit ClientPacketBase(Protocol_t _rawProtocol)
        : rawProtocol(_rawProtocol) {
    }

    template <typename Archive, typename Self>
    static zpp::bits::errc serialize(Archive&ar, Self& self)
    {
        return ar(self.rawProtocol);
    }
};