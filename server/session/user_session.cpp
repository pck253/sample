#include "pch.h"

UserSession::PacketHandlerCallers_t UserSession::m_packetHandlerCallers;

UserSession::UserSession(ConnectionShared_t _conn, ThreadPool& _threadPool)
	: Super_t(_threadPool), m_connection(_conn)
{
}

UserSession::~UserSession()
{
	m_connection.reset();
	Log("deleted UserSession");
}

Result UserSession::Send(const PacketSize_t& _size, const uint8_t* _serializedData, PacketDeallocatorShared_t& _deallocator)
{
	return m_connection->Send(_size, _serializedData, _deallocator);
}

void UserSession::Close(const Result& _reason)
{
	m_connection->Close(_reason);
}

void UserSession::Closed(const Result& _result)
{
	Shutdown();
}

void UserSession::InitPacketHandlers()
{
	m_packetHandlerCallers.emplace(Client::Protocol::Protocol_TestMessage,
		new FbPacketHandleCaller<UserSessionShared_t, Client::Body, Client::TestMessage>([](UserSessionShared_t&) { return true; }));
}

void UserSession::UninitPacketHandlers()
{
	for (auto& [protocol, handler] : m_packetHandlerCallers)
	{
		SAFE_DELETE(handler);
	}
	m_packetHandlerCallers.clear();
}

bool UserSession::CallPacketHandler(our::vector<uint8_t>&& _rawData)
{
	const Client::Body* _packetBody = Client::GetBody(&_rawData[0]);

	Client::Protocol protocol = _packetBody->body_type();
	auto found = m_packetHandlerCallers.find(protocol);
	if (m_packetHandlerCallers.end() == found)
	{
		return false;
	}

	auto self = Get<UserSession>();
	return found->second->CallHandler(self, _packetBody, std::move(_rawData));
}