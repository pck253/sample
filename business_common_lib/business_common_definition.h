#pragma once

enum class ServerState : uint8_t
{
    CanNotUse = 0,
    Normal,
    Busy
};

struct ServerIdentityInfo
{
    ServerId_t ServerId_t{};
    std::string name;
    ServerState state{};
};

// --------------------------------------------------------------------

enum class EActorType : uint8_t
{
    Character = 0,
    Monster,
    Npc,
    Max
};

using ActorSerialRaw_t = uint64_t;
union ActorSerial
{
    ActorSerialRaw_t serialId{};
    struct
    {
        uint8_t instanceId[7];
        EActorType type;
    };

    ActorSerial() {}
    ActorSerial(const ActorSerialRaw_t& _serialId) { serialId = _serialId; } // not use explicit for convenience
    explicit ActorSerial(const EActorType& _type, const uint64_t& _instanceId) : serialId(_instanceId) { type = _type; }
    ActorSerial(const ActorSerial& _other) : serialId(_other.serialId) {} // not use explicit for convenience
    explicit ActorSerial(ActorSerial&& _other) noexcept : serialId(_other.serialId) { _other.serialId = 0; }

    void Reset()
    {
        serialId = 0;
    }

    ActorSerial& operator=(const ActorSerial& _other)
    {
        serialId = _other.serialId;
        return *this;
    }
    ActorSerial& operator=(ActorSerial&& _other) noexcept
    {
        serialId = _other.serialId;
        _other.serialId = 0;
        return *this;
    }

    bool operator()() const { return (0 != serialId); }

    bool operator!=(const ActorSerial& _other) const
    {
        return (serialId != _other.serialId);
    }
    bool operator==(const ActorSerial& _other) const
    {
        return (serialId == _other.serialId);
    }

    operator ActorSerialRaw_t() const { return this->serialId; }
    operator EActorType() const { return this->type; }

    // for zpp::bits
    template <typename Archive, typename Self>
    static auto serialize(Archive& ar, Self& self)
    {
        return ar(self.serialId);
    }
};
extern const ActorSerial INVALID_ACTOR_SERIAL;

namespace std
{
    template <>
    struct hash<ActorSerial>
    {
        size_t operator()(const ActorSerial& _serial) const noexcept
        {
            return std::hash<ActorSerialRaw_t>{}(_serial.serialId);
        }
    };
}

using ActorMoveFlags_t = uint8_t;
enum class EActorMoveFlag : ActorMoveFlags_t
{
    None = 0,
    Move = 1 << 0,
    Stop = 1 << 1,
    Warp = 1 << 2,
};

struct ActorMove
{
    ActorSerial actorSerial{};
    float speed{};
    WorldPos_t pos;
    WorldPos_t dir;
    ActorMoveFlags_t flags{};
};

// --------------------------------------------------------------------

enum class ENeighborRect
{
    Top = 0,
    TopRight,
    Right,
    BottomRight,
    Bottom,
    BottomLeft,
    Left,
    TopLeft,
    Max
};

// --------------------------------------------------------------------

struct ZoneKey
{
    uint32_t zoneId{};
    uint64_t instanceId{};

    ZoneKey() {}
    explicit ZoneKey(const uint32_t& _zoneId, const uint64_t& _instanceId) : zoneId(_zoneId), instanceId(_instanceId) {}

    auto GetZoneId() const { return zoneId; }
    auto GetInstanced() const { return instanceId; }

    bool operator()() const { return (0 != zoneId); }

    bool operator==(const ZoneKey& _other) const
    {
        return (zoneId == _other.zoneId && instanceId == _other.instanceId);
    }
    bool operator!=(const ZoneKey& _other) const
    {
        return !operator==(_other);
    }
};

namespace std
{
    template <>
    struct hash<ZoneKey>
    {
        size_t operator()(const ZoneKey& _zoneKey) const noexcept
        {
            size_t seed = 0;
            HashCombine(seed, _zoneKey.zoneId);
            HashCombine(seed, _zoneKey.instanceId);
            return seed;
        }
    };
}