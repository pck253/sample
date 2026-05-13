#pragma once

namespace ServerTest
{
    enum class EProtocol : Protocol_t
    {
        Invalid = 0,
        Test,
        Max
    };

    struct Test : public ClientPacketBase
    {
        std::string message;

        Test() : ClientPacketBase(static_cast<Protocol_t>(EProtocol::Test)) {}
        
        explicit Test(
            const std::string& _message)
            : ClientPacketBase(static_cast<Protocol_t>(EProtocol::Test)),
            message(_message)
        {}


        template <typename Archive, typename Self>
        static zpp::bits::errc serialize(Archive & ar, Self & self)
        {
            if constexpr (Archive::kind() == zpp::bits::kind::in)
            {
                return ar(self.message);
            }
            ADD_BASE_ARCHIVE()
            return ar(self.message);
        }
    };

}