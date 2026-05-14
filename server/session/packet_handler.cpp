#include "pch.h"

// --------------- client to server ----------------------------
bool Handler(UserSession& _user, Client::TestMessage&& _packet)
{
	std::string& title = _packet.title;
	std::string& body = _packet.body;

	Client::TestMessage res(title, body);

	auto serializedInfo{ ZppBits::Serialize(res) };

	_user.PushJob([serializedInfo](UserSession& _user) mutable
		{
			_user.Send(serializedInfo.serializedSize, serializedInfo.serializedBuffer, serializedInfo.deallocator);
		}
	);

	return true;
}

// --------------- server common ----------------------------
bool Handler(ServerSession& _server, ServerCommon::Activation&& _packet)
{
	ServerSerial serial(_packet.serverId, _packet.moduleType, _packet.serverGroupId);

	const auto serverSession = _server.Get<ServerSession>();
	if (!Module::As<Server>()->GetServerSessionManager().AddAuthorizedSession(serial, serverSession))
	{
		auto jobInst = ThreadPool::MakeJobInst([serverSession]()
			{
				serverSession->Close(EError::AlreadyExistSameServer);
			});
		Module::As<Server>()->GetTimerJobManager()->PushTimerJob(std::move(jobInst), 1000);
	}

	return true;
}

bool Handler(ServerSession& _server, ServerCommon::Shutdown&& _packet)
{
	Log("received shutdown.");
	Module::As<Server>()->ShutdownApplicationByRemote();
	return true;
}

// --------------- server to server ----------------------------
bool Handler(ServerSession& _server, ServerTest::Test&& _packet)
{

	return true;
}