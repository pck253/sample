#include "pch.h"

Session::Session(ThreadPool& _threadPool, ConnectionShared_t _conn)
	: Super_t(_threadPool), m_connection(_conn)
{
	m_latestReceived = GET_TICK();
}

Session::~Session()
{
	m_connection.reset();
	Log("deleted Session");
}

Result Session::Send(const PacketSize_t& _size, const uint8_t* _serializedData, const PacketDeallocatorShared_t& _deallocator)
{
	return m_connection->Send(_size, _serializedData, _deallocator);
}

void Session::Close(const Result& _reason)
{
	m_connection->Close(_reason);
}

void Session::Closed(const Result& _result)
{
	Shutdown("test_client session shutdown.");
}

void Session::InitPacketHandlers()
{
	m_packetHandlerCallers.emplace(Client::EProtocol::TestMessage,
		new ZppBitsPacketHandleCaller<Session, Client::TestMessage>([](const Session&) { return true; }));
}

void Session::UninitPacketHandlers()
{
	for (auto& [protocol, handler] : m_packetHandlerCallers)
	{
		SAFE_DELETE(handler);
	}
	m_packetHandlerCallers.clear();
}

bool Session::CallPacketHandler(std::vector<uint8_t>&& _rawData)
{
	ClientPacketBase base;
	zpp::bits::in in(_rawData);
	auto result = in(base);

	auto found = m_packetHandlerCallers.find(static_cast<Client::EProtocol>(base.rawProtocol));
	if (m_packetHandlerCallers.end() == found)
	{
		return false;
	}

	return found->second->CallHandler(As<Session>(), in);
}

void Session::ReceivedStatics(const PacketSize_t& _receivedSize)
{
	++m_receivedCount;
	m_receivedByte += _receivedSize;
	auto now = GET_TICK();
	if (now - m_latestReceived >= 5000)
	{
		printf_s("%llu received.\n", m_receivedByte);
		m_receivedByte = 0;
		m_latestReceived = now;
	}
}