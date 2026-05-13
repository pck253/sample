#include "pch.h"

// --------------- client to server ----------------------------
bool Handler(UserSessionShared_t& _user, Client::TestMessage&& _packet)
{
	std::string& title = _packet.title;
	std::string& body = _packet.body;

	Client::TestMessage res(title, body);

	auto serializedInfo{ ZppBits::Serialize(res) };

	_user->PushJob([serializedInfo](const UserSessionShared_t& _user) mutable
		{
			_user->Send(serializedInfo.serializedSize, serializedInfo.serializedBuffer, serializedInfo.deallocator);
		}
	);

	return true;
}

// --------------- server common ----------------------------
bool Handler(ServerSessionShared_t& _server, ServerCommon::Activation&& _packet)
{
	ServerSerial serial(_packet.serverId, _packet.moduleType, _packet.serverGroupId);

	if (!Module::Get<Server>()->GetServerSessionManager().AddAuthorizedSession(serial, _server))
	{
		auto jobInst = ThreadPool::MakeJobInst([_server]()
			{
				_server->Close(EError::AlreadyExistSameServer);
			});
		Module::Get<Server>()->GetTimerJobManager()->PushTimerJob(std::move(jobInst), 1000);
	}

	return true;
}

bool Handler(ServerSessionShared_t& _server, ServerCommon::Shutdown&& _packet)
{
	Log("received shutdown.");
	Module::Get<Server>()->ShutdownApplicationByRemote();
	return true;
}

// --------------- server to server ----------------------------
bool Handler(ServerSessionShared_t& _server, ServerTest::Test&& _packet)
{

	return true;
}