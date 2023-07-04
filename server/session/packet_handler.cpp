#include "pch.h"

// --------------- client to server ----------------------------
bool Handler(UserSessionShared_t& _user, const Client::TestMessage* _packetBody, our::vector<uint8_t>&& _rawData)
{
	std::string title = _packetBody->title()->c_str();
	std::string body = _packetBody->body()->c_str();

	_user->PushJob([_user, title, body]() mutable
		{
			FLAT_BUFFER_BUILDER(builder);
			auto pktTitle = builder.CreateString(title);
			auto pktBody = builder.CreateString(body);
			auto remessage = Client::CreateTestMessage(builder, pktTitle, pktBody);
			auto repacket = Client::CreateBody(builder, Client::Protocol::Protocol_TestMessage, remessage.Union());
			builder.Finish(repacket);

			Fb::SerializedInfo serializedInfo = Fb::MakeSerializedInfo(builder);

			_user->Send(serializedInfo.serializedSize, serializedInfo.serializedBuffer, serializedInfo.deallocator);
		});

	return true;
}

// --------------- server common ----------------------------
bool Handler(ServerSessionShared_t& _server, const ServerCommon::Activation* _packetBody, our::vector<uint8_t>&& _rawData)
{
	EModule moduleType = static_cast<EModule>(_packetBody->moduleType());

	ServerSerial serial(_packetBody->serverId(), moduleType, _packetBody->serverGroupId());

	if (!Module::Get<Server>()->GetServerSessionManager().AddAuthorizedSession(serial, _server))
	{
		Module::Get<Server>()->GetTimerJobManager()->PushJob([_server]()
			{
				_server->Close(EError::AlreadyExistSameServer);
			}, 1000);
	}

	return true;
}

bool Handler(ServerSessionShared_t& _server, const ServerCommon::Shutdown* _packetBody, our::vector<uint8_t>&& _rawData)
{
	Log("received shutdown.");
	Module::Get<Server>()->ShutdownApplicationByRemote();
	return true;
}

// --------------- server to server ----------------------------
bool Handler(ServerSessionShared_t& _server, const ServerTest::Test* _packetBody, our::vector<uint8_t>&& _rawData)
{

	return true;
}