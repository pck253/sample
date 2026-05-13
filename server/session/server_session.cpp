#include "pch.h"

ServerSession::ServerSession(ThreadPool& _threadPool, ConnectionShared_t _conn)
	: Super_t(_threadPool, _conn)
{
}

ServerSession::~ServerSession()
{
	Log("Server : deleted ServerSession");
}

void ServerSession::InitPacketHandlers()
{
	m_serverPacketHandlerCallers.emplace(ServerTest::EProtocol::Test, new ZppBitsPacketHandleCaller<Shared_t, ServerTest::Test>([](ServerSessionShared_t&) { return true; }));

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

bool ServerSession::CallPacketHandler(std::vector<uint8_t>&& _rawData)
{
	ServerPacketBase base;
	zpp::bits::in in(_rawData);
	auto result = in(base);

	switch (base.type)
	{
	case EServerPacketType::ServerCommon:
		{
			const auto protocol{ static_cast<ServerCommon::EProtocol>(base.rawProtocol) };
			return Super_t::CallCommonPacketHandler(protocol, in);
		}
		break;
	case EServerPacketType::ServerTest:
		{
			const auto protocol{ static_cast<ServerTest::EProtocol>(base.rawProtocol) };
			const auto found = m_serverPacketHandlerCallers.find(protocol);
			if (m_serverPacketHandlerCallers.end() != found)
			{
				auto self = Get<ServerSession>();
				return found->second->CallHandler(self, in);
			}
		}
		break;
	}

	return false;
}