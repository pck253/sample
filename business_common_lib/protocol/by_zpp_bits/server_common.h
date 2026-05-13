#pragma once

namespace ServerCommon
{
    enum class EProtocol : Protocol_t
    {
        Invalid = 0,
        Activation,
        Shutdown,
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

}