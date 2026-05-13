#pragma once

namespace Client
{
    enum class EProtocol : Protocol_t
    {
        Invalid = 0,
        TestMessage,
        Max
    };

    struct TestMessage : public ClientPacketBase
    {
        std::string title;
        std::string body;

        TestMessage() : ClientPacketBase(static_cast<Protocol_t>(EProtocol::TestMessage)) {}
        
        explicit TestMessage(
            const std::string& _title,
            const std::string& _body)
            : ClientPacketBase(static_cast<Protocol_t>(EProtocol::TestMessage)),
            title(_title),
            body(_body)
        {}


        template <typename Archive, typename Self>
        static zpp::bits::errc serialize(Archive & ar, Self & self)
        {
            if constexpr (Archive::kind() == zpp::bits::kind::in)
            {
                return ar(self.title, self.body);
            }
            ADD_BASE_ARCHIVE()
            return ar(self.title, self.body);
        }
    };

}