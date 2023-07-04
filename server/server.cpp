#include "pch.h"

MODULE_STATIC_IMPL(Server);

Server::Server(const std::string& _configFilePath)
	: Module(_configFilePath), m_serverSessionManager(m_threadPool)
{
}

Server::~Server()
{
}

Result Server::InitImpl()
{
	UserSession::InitPacketHandlers();
	ServerSession::InitPacketHandlers();
	InitRestfulHandlers();

	int16_t threadCount = 0;
	auto threadConfig = m_config["thread"];
	if (threadConfig.is_number())
	{
		threadCount = threadConfig.get<int16_t>();
	}

	if (threadCount == 0)
	{
		threadCount = std::thread::hardware_concurrency();
	}
	m_threadPool.Init(threadCount);

	auto id = m_config["id"];
	if (!id.is_number_unsigned() || (std::numeric_limits<ServerId_t>::max)() < id.get<ServerId_t>())
	{
		return EError::InvalidServerId;
	}
	m_id = id.get<ServerId_t>();

	auto groupId = m_config["group id"];
	if (!groupId.is_number_unsigned() || (std::numeric_limits<ServerGroupId_t>::max)() < groupId.get<ServerGroupId_t>())
	{
		return EError::InvalidServerGroupId;
	}
	m_groupId = groupId.get<ServerGroupId_t>();

	// network
	NetworkHelper networkHelper;
	ModuleAccessor* networkAccessor = nullptr;
	auto result = networkHelper.SettingByConfig(m_config, GetApplication(), (ModuleAccessor*&)networkAccessor,
		[](const Result& _result, ConnectionShared_t _conn) -> bool
		{
			if (_result != EError::Success)
			{
				LogWarning("failed to accept");
				return false;
			}

			if (!_conn->IsPublic())
			{
				Module::Get<Server>()->ServerConnected(_conn);
			}
			else
			{
				Module::Get<Server>()->AddUserSession(_conn);
			}

			return true;
		},
		[](const Result & _result, const std::string & _connecterName, ConnectionShared_t _conn) -> bool
		{
			if (_result != EError::Success)
			{
				LogWarning("failed to connect");
				return false;
			}

			Module::Get<Server>()->ServerConnected(_conn);

			return true;
		});
	if (result != EError::Success)
	{
		return result;
	}

	// restful
	RestfulHelper restfulHelper;
	ModuleAccessor* restfulAccessor = nullptr;
	result = restfulHelper.SettingByConfig(m_config, GetApplication(), restfulAccessor,
		[](const RestfulRequestId_t& _requestId, const std::wstring& _path, const std::wstring& _query, RestfulAccessor* _accessor) -> bool
		{
			Module::Get<Server>()->CallRestfulHandler(_requestId, _path, _query, _accessor);
			return true;
		});
	if (result != EError::Success)
	{
		return result;
	}

	// timer
	auto timerModule = GetApplication()->GetModule(EModule::Timer);
	if (!timerModule)
	{
		return EError::NotExistModule;
	}
	m_timerJobManager = TimerJobManager::Create<TimerJobManager>(m_threadPool);
	m_timerJobManager->Init(static_cast<TimerAccessor*>(timerModule->GetAccessor()));

	return EError::Success;
}

void Server::InitRestfulHandlers()
{
	m_restfulHandlers.emplace(L"/shutdown", [this](const RestfulParams&)
		{
			GetServerSessionManager().Travel([](ServerSessionShared_t& _session)
				{
					FLAT_BUFFER_BUILDER(builder);
					auto shutdown = ServerCommon::CreateShutdown(builder);
					auto serverCommon = ServerCommon::CreateBody(builder, ServerCommon::Protocol::Protocol_Shutdown, shutdown.Union());
					auto packet = CreateServerPacketBody(builder, ServerPacket::ServerPacket_ServerCommon_Body, serverCommon.Union());
					builder.Finish(packet);

					Fb::SerializedInfo serializedInfo = Fb::MakeSerializedInfo(builder);
					_session->Send(serializedInfo.serializedSize, serializedInfo.serializedBuffer, serializedInfo.deallocator);
				});
			GetApplication()->TryShutdown();
			return nlohmann::json({ {"message", "done"} });
		});
	m_restfulHandlers.emplace(L"/kick", [this](const RestfulParams& _params)
		{
			auto found = _params.m_params.find("target");
			if (_params.m_params.end() == found)
			{
				return nlohmann::json({ {"error", "invalid params"} });
			}
			ConnectionId_t connectionId = std::stoull(found->second);
			auto user = FindUserSession(connectionId);
			if (!user)
			{
				return nlohmann::json({ {"error", "not exist target."} });
			}

			user->Close(EError::Kick);

			return nlohmann::json({ {"message", "done"} });
		});
	m_restfulHandlers.emplace(L"/kickall", [this](const RestfulParams& _params)
		{
			NetworkHelper networkHelper;
			networkHelper.ShutdownPublic(m_config, GetApplication());

			return nlohmann::json({ {"message", "done"} });
		});
}

void Server::CallRestfulHandler(const RestfulRequestId_t& _requestId, const std::wstring& _path, const std::wstring& _query, RestfulAccessor* _accessor)
{
	m_threadPool.PushJob([this, _requestId, _path, _query, _accessor]()
		{
			auto found = m_restfulHandlers.find(_path);
			if (m_restfulHandlers.end() == found)
			{
				return;
			}

			RestfulParams params;
			params.Parse(_query);

			if (params.m_result != EError::Success)
			{
				_accessor->Response(_requestId, nlohmann::json({ {"error", "invalid params"} }));
				return;
			}

			auto reply = found->second(params);
			_accessor->Response(_requestId, std::move(reply));
		});
}

void Server::Shutdown()
{
	NetworkHelper networkHelper;
	networkHelper.ShutdownPublic(m_config, GetApplication());

	if (m_timerJobManager)
	{
		m_timerJobManager->Shutdown();
	}

	while (!m_threadPool.IsEmpty())
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(5000));
		Log("Server Shutdown : waiting until thread pool is empty.");
	}
	m_threadPool.Shutdown();

	UserSession::UninitPacketHandlers();
	ServerSession::UninitPacketHandlers();

	m_timerJobManager.reset();
}

UserSessionShared_t Server::AddUserSession(ConnectionShared_t _conn)
{
	UserSessionShared_t user = UserSession::Create(_conn, m_threadPool);

	{
		SCOPED_WRITE_LOCK(m_userSessionsMutex);
		m_userSessions.emplace(_conn->GetConnectionId(), user);
	}

	_conn->SetReceivedHandler([user](our::vector<uint8_t>&& _rawData, ConnectionShared_t _conn)
		{
			user->PushJob([user, _rawData = std::move(_rawData)]() mutable
				{
					user->Get<UserSession>()->CallPacketHandler(std::move(_rawData));
				});
		});

	//_conn->SetSentHandler([](const size_t& _size){});

	_conn->SetClosedHandler([](const Result& _result, const ConnectionId_t & _connectionId, const bool& _isPublic)
		{
			if (!_result.message.empty())
			{
				LogWarning("Closed User Session : {}", _result.message);
			}
			Module::Get<Server>()->ClosedUserSession(_result, _connectionId);
		});

	return user;
}

UserSessionShared_t Server::FindUserSession(const ConnectionId_t& _connId)
{
	SCOPED_READ_LOCK(m_userSessionsMutex);
	auto found = m_userSessions.find(_connId);
	return (found == m_userSessions.end()) ? nullptr : found->second;
}

void Server::ClosedUserSession(const Result& _result, const ConnectionId_t& _connId)
{
	auto user = FindUserSession(_connId);
	{
		SCOPED_WRITE_LOCK(m_userSessionsMutex);
		m_userSessions.erase(_connId);
	}

	if (user)
	{
		user->PushJob([user, _result]()
			{
				user->Closed(_result);
			});
	}
}

void Server::PushJobToAllUserForDev(std::function<void(UserSessionShared_t&)>&& _job)
{
	SCOPED_READ_LOCK(m_userSessionsMutex);
	for (auto& [connectionId, user] : m_userSessions)
	{
		user->PushJob([user, _job]() mutable
			{
				_job(user);
			});
	}
}

ServerSessionShared_t Server::ServerConnected(ConnectionShared_t _conn)
{
	auto server = Module::Get<Server>()->GetServerSessionManager().CreateSession(_conn);

	_conn->SetReceivedHandler([server](our::vector<uint8_t>&& _rawData, ConnectionShared_t _conn)
		{
			server->PushJob([server, _rawData = std::move(_rawData)]() mutable
				{
					server->CallPacketHandler(std::move(_rawData));
				});
		});

	//_conn->SetSentHandler([](const size_t& _size){});

	_conn->SetClosedHandler([](const Result& _result, const ConnectionId_t& _connectionId, const bool& _isPublic)
		{
			if (!_result.message.empty())
			{
				LogWarning("Closed Server Session : {}", _result.message);
			}

			auto session = Module::Get<Server>()->GetServerSessionManager().RemoveSession(_connectionId);
			if (session)
			{
				session->Closed();
			}
		});

	server->SendActivation(GetModuleType(), GetServerId(), GetServerGroupId());

	return server;
}