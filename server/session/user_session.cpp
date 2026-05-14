#include "pch.h"

UserSession::UserSession(ThreadPool& _threadPool, ConnectionShared_t _conn)
	: Super_t(_threadPool), m_connection(_conn)
{
}

UserSession::~UserSession()
{
	m_connection.reset();
	Log("deleted UserSession");
}

Result UserSession::Send(const PacketSize_t& _size, const uint8_t* _serializedData, const PacketDeallocatorShared_t& _deallocator)
{
	return m_connection->Send(_size, _serializedData, _deallocator);
}

void UserSession::Close(const Result& _reason)
{
	m_connection->Close(_reason);
}

void UserSession::Closed(const Result& _result)
{
	Shutdown(EShutdownMode::RightNow, "user session shutdown.");
}

void UserSession::InitPacketHandlers()
{
	m_packetHandlerCallers.emplace(Client::EProtocol::TestMessage,
		new ZppBitsPacketHandleCaller<UserSession, Client::TestMessage>([](const UserSession&) { return true; }));
}

void UserSession::UninitPacketHandlers()
{
	for (auto& [protocol, handler] : m_packetHandlerCallers)
	{
		SAFE_DELETE(handler);
	}
	m_packetHandlerCallers.clear();
}

bool UserSession::CallPacketHandler(std::vector<uint8_t>&& _rawData)
{
	ClientPacketBase base;
	zpp::bits::in in(_rawData);
	auto result = in(base);

	const auto found = m_packetHandlerCallers.find(static_cast<Client::EProtocol>(base.rawProtocol));
	if (m_packetHandlerCallers.end() == found)
	{
		return false;
	}

	return found->second->CallHandler(*this, in);
}