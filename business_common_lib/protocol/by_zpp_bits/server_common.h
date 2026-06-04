#pragma once

namespace ServerCommon
{
    enum class EProtocol : Protocol_t
    {
        Invalid = 0,
        Activation,
        Shutdown,
        Ping,
        Pong,
        Max
    };

    struct Activation : public ServerPacketBase
    {
        EModule moduleType{};
        ServerId_t serverId{};
        ServerGroupId_t serverGroupId{};

        Activation() : ServerPacketBase(EServerPacketType::ServerCommon, static_cast<Protocol_t>(EProtocol::Activation)) {}
        
        explicit Activation(
            const EModule _moduleType,
            const ServerId_t _serverId,
            const ServerGroupId_t _serverGroupId)
            : ServerPacketBase(EServerPacketType::ServerCommon, static_cast<Protocol_t>(EProtocol::Activation)),
            moduleType(_moduleType),
            serverId(_serverId),
            serverGroupId(_serverGroupId)
        {}


        template <typename Archive, typename Self>
        static zpp::bits::errc serialize(Archive & ar, Self & self)
        {
            if constexpr (Archive::kind() == zpp::bits::kind::in)
            {
                return ar(self.moduleType, self.serverId, self.serverGroupId);
            }
            ADD_BASE_ARCHIVE()
            return ar(self.moduleType, self.serverId, self.serverGroupId);
        }
    };

    struct Shutdown : public ServerPacketBase
    {


        Shutdown() : ServerPacketBase(EServerPacketType::ServerCommon, static_cast<Protocol_t>(EProtocol::Shutdown)) {}
        


        template <typename Archive, typename Self>
        static zpp::bits::errc serialize(Archive & ar, Self & self)
        {
            if constexpr (Archive::kind() == zpp::bits::kind::in)
            {
                return ar();
            }
            ADD_BASE_ARCHIVE()
            return ar();
        }
    };

    struct Ping : public ServerPacketBase
    {
        Time_t timestamp{};

        Ping() : ServerPacketBase(EServerPacketType::ServerCommon, static_cast<Protocol_t>(EProtocol::Ping)) {}
        
        explicit Ping(
            const Time_t _timestamp)
            : ServerPacketBase(EServerPacketType::ServerCommon, static_cast<Protocol_t>(EProtocol::Ping)),
            timestamp(_timestamp)
        {}


        template <typename Archive, typename Self>
        static zpp::bits::errc serialize(Archive & ar, Self & self)
        {
            if constexpr (Archive::kind() == zpp::bits::kind::in)
            {
                return ar(self.timestamp);
            }
            ADD_BASE_ARCHIVE()
            return ar(self.timestamp);
        }
    };

    struct Pong : public ServerPacketBase
    {
        Time_t timestamp{};
        Time_t relayTimestamp{};

        Pong() : ServerPacketBase(EServerPacketType::ServerCommon, static_cast<Protocol_t>(EProtocol::Pong)) {}
        
        explicit Pong(
            const Time_t _timestamp,
            const Time_t _relayTimestamp)
            : ServerPacketBase(EServerPacketType::ServerCommon, static_cast<Protocol_t>(EProtocol::Pong)),
            timestamp(_timestamp),
            relayTimestamp(_relayTimestamp)
        {}


        template <typename Archive, typename Self>
        static zpp::bits::errc serialize(Archive & ar, Self & self)
        {
            if constexpr (Archive::kind() == zpp::bits::kind::in)
            {
                return ar(self.timestamp, self.relayTimestamp);
            }
            ADD_BASE_ARCHIVE()
            return ar(self.timestamp, self.relayTimestamp);
        }
    };

}