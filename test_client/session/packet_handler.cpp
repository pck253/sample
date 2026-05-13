#include "pch.h"

bool g_printTestMessage = false;

bool Handler(SessionShared_t& _session, Client::TestMessage&& _packet)
{
    _session->ReceivedStatics(static_cast<PacketSize_t>(_packet.title.size() + _packet.body.size()));

    std::string& title = _packet.title;
    std::string& body = _packet.body;
    if (g_printTestMessage)
    {
        Log("{}, {}", title.c_str(), body.c_str());
    }

    return true;
}