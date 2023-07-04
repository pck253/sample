#include "pch.h"

ServerSession::ServerPacketHandlerCallers_t ServerSession::m_serverPacketHandlerCallers;
ServerSession::Super_t::CommonPacketHandlerCallers_t ServerSession::Super_t::m_commonPacketHandlerCallers;

ServerSession::ServerSession(ConnectionShared_t _conn, ThreadPool& _threadPool)
	: Super_t(_conn, _threadPool)
{
}

ServerSession::~ServerSession()
{
	Log("Server : deleted ServerSession");
}

void ServerSession::InitPacketHandlers()
{
	m_serverPacketHandlerCallers.emplace(ServerTest::Protocol::Protocol_Test, new FbPacketHandleCaller<Shared_t, ServerTest::Body, ServerTest::Test>([](ServerSessionShared_t&) { return true; }));

	Super_t::InitCommonPacketHandlers();
}

void ServerSession::UninitPacketHandlers()
{
	for (auto& [protocol, handler] : m_serverPacketHandlerCallers)
	{
		SAFE_DELETE(handler);
	}
	m_serverPacketHandlerCallers.clear();

	Super_t::UninitCommonPacketHandlers();
}

bool ServerSession::CallPacketHandler(our::vector<uint8_t>&& _rawData)
{
	const ServerPacketBody* packetBody = GetServerPacketBody(&_rawData[0]);

	auto self = Get<ServerSession>();

	ServerPacket packet = packetBody->body_type();
	switch (packet)
	{
	case ServerPacket_ServerCommon_Body:
		{
			auto body = packetBody->body_as_ServerCommon_Body();
			return Super_t::CallCommonPacketHandler(body, std::move(_rawData));
		}
		break;
	case ServerPacket_ServerTest_Body:
		{
			auto body = packetBody->body_as_ServerTest_Body();
			auto protocol = body->body_type();
			auto found = m_serverPacketHandlerCallers.find(protocol);
			if (m_serverPacketHandlerCallers.end() == found)
			{
				return false;
			}

			auto self = Get<ServerSession>();
			return found->second->CallHandler(self, body, std::move(_rawData));
		}
	}
	return false;
}