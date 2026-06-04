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
	else
	{
		auto jobInst = ThreadPool::MakeJobInst([serverSession]()
			{
				const auto serverSerial = serverSession->GetServerSerial();
				Log("send Ping : to serverId {}, serverGroupId {}", serverSerial.id, serverSerial.groupId);

				ServerCommon::Ping res(GET_TIME());

				auto serializedInfo{ ZppBits::Serialize(res) };

				serverSession->Send(serializedInfo.serializedSize, serializedInfo.serializedBuffer, serializedInfo.deallocator);
			});
		Module::As<Server>()->GetTimerJobManager()->PushTimerJob(std::move(jobInst), 3000);
	}

	return true;
}

bool Handler(ServerSession& _server, ServerCommon::Shutdown&& _packet)
{
	Log("received shutdown.");
	Module::As<Server>()->ShutdownApplicationByRemote();
	return true;
}

bool Handler(ServerSession& _server, ServerCommon::Ping&& _packet)
{
	_server.PushJob([_packet](ServerSession& _session)
		{
			const auto serverSerial = _session.GetServerSerial();
			Log("received Ping : from serverId {}, serverGroupId {}", serverSerial.id, serverSerial.groupId);

			ServerCommon::Pong res(GET_TIME(), _packet.timestamp);

			auto serializedInfo{ ZppBits::Serialize(res) };

			_session.Send(serializedInfo.serializedSize, serializedInfo.serializedBuffer, serializedInfo.deallocator);
		});
	return true;
}

bool Handler(ServerSession& _server, ServerCommon::Pong&& _packet)
{
	_server.PushJob([_packet](ServerSession& _session)
		{
			const auto serverSerial = _session.GetServerSerial();
			Log("received Pong : from serverId {}, serverGroupId {}, rtt {}", serverSerial.id, serverSerial.groupId, _packet.timestamp - _packet.relayTimestamp);

			auto jobInst = ThreadPool::MakeJobInst([session = _session.Get<ServerSession>()]()
				{
					const auto serverSerial = session->GetServerSerial();
					Log("send Ping : to serverId {}, serverGroupId {}", serverSerial.id, serverSerial.groupId);

					ServerCommon::Ping res(GET_TIME());

					auto serializedInfo{ ZppBits::Serialize(res) };

					session->Send(serializedInfo.serializedSize, serializedInfo.serializedBuffer, serializedInfo.deallocator);
				});
			Module::As<Server>()->GetTimerJobManager()->PushTimerJob(std::move(jobInst), 3000);
		});
	return true;
}

// --------------- server to server ----------------------------
bool Handler(ServerSession& _server, ServerTest::Test&& _packet)
{

	return true;
}