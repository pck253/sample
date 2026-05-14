#include "pch.h"

MODULE_STATIC_IMPL(Server);

Server::Server(const std::string& _configFilePath)
	: Module(_configFilePath), m_threadPool("Server ThreadPool"), m_serverSessionManager(m_threadPool)
{
}

Server::~Server()
{
}

static bool OnAcceptedConnection(const Result& _result, ConnectionShared_t _conn)
{
	if (!_result)
	{
		LogWarning("failed to accept");
		return false;
	}

	if (!_conn->IsPublic())
	{
		Module::As<Server>()->ServerConnected(_conn);
	}
	else
	{
		Module::As<Server>()->AddUserSession(_conn);
	}

	return true;
}

static bool OnConnected(const Result& _result, const std::string& _connecterName, ConnectionShared_t _conn)
{
	if (!_result)
	{
		LogWarning("AcceptedHandler : {}", _result.message);
		return false;
	}

	Module::As<Server>()->ServerConnected(_conn);

	return true;
}

static void OnReceived(std::vector<uint8_t>&& _rawData, const ConnectionShared_t& _conn)
{
	if (_conn->IsPublic())
	{
		auto const user = Module::As<Server>()->FindUserSession(_conn->GetConnectionId());
		if (not user)
		{
			LogError("not exist user");
			return;
		}

		user->PushJob([_rawData = std::move(_rawData)](UserSession& _user) mutable
			{
				_user.CallPacketHandler(std::move(_rawData));
			});
	}
	else
	{
		auto const server = Module::As<Server>()->GetServerSessionManager().FindSession(_conn->GetConnectionId());
		if (not server)
		{
			LogError("not exist server");
			return;
		}

		server->PushJob([_rawData = std::move(_rawData)](ServerSession& _server) mutable
			{
				_server.CallPacketHandler(std::move(_rawData));
			});
	}
}

static void OnClosed(const Result& _result, const ConnectionId_t _connectionId, const bool _isPublic)
{
	if (_isPublic)
	{
		if (!_result.message.empty())
		{
			LogWarning("Closed User Session : {}", _result.message);
		}
		Module::As<Server>()->ClosedUserSession(_result, _connectionId);
	}
	else
	{
		if (!_result.message.empty())
		{
			LogWarning("Closed Server Session : {}", _result.message);
		}

		auto session = Module::As<Server>()->GetServerSessionManager().RemoveSession(_connectionId);
		if (session)
		{
			session->Closed();
		}
	}
}

static bool OnRestfulRequest(const RestufulRequestId_t& _requestId, const std::wstring& _path, const std::wstring& _query, WebAccessor* _accessor)
{
	Module::As<Server>()->CallRestfulHandler(_requestId, _path, _query, _accessor);
	return true;
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
	if (!id.is_number_unsigned() || (std::numeric_limits<ServerId_t::IdType>::max)() < id.get<ServerId_t::IdType>())
	{
		return EError::InvalidServerId;
	}
	m_id = id.get<ServerId_t::IdType>();

	auto groupId = m_config["group id"];
	if (!groupId.is_number_unsigned() || (std::numeric_limits<ServerGroupId_t::IdType>::max)() < groupId.get<ServerGroupId_t::IdType>())
	{
		return EError::InvalidServerGroupId;
	}
	m_groupId = groupId.get<ServerGroupId_t::IdType>();

	// network
	AcceptedConfig acceptedConfig;
	acceptedConfig.Set(OnAcceptedConnection, OnReceived, OnClosed);

	ConnectedConfig connectedConfig{ OnConnected, OnReceived, OnClosed };

	NetworkHelper networkHelper;
	auto result = networkHelper.SettingByConfig(m_config, GetApplication(), acceptedConfig, connectedConfig);
	if (!result)
	{
		return result;
	}

	// web
	WebHelper webHelper;
	ModuleAccessor* webAccessor = nullptr;
	result = webHelper.SettingByConfig(m_config, GetApplication(), webAccessor, OnRestfulRequest);
	if (!result)
	{
		return result;
	}

	// timer
	auto timerModule = GetApplication()->GetModule(EModule::Timer);
	if (!timerModule)
	{
		return EError::NotExistModule;
	}
	m_timerJobManager = static_cast<TimerAccessor*>(timerModule->GetAccessor())->AllocateTimerJobManager(m_threadPool);

	return EError::Success;
}

void Server::InitRestfulHandlers()
{
	m_restfulHandlers.emplace(L"/shutdown", [this](const WebParams&)
		{
			m_serverSessionManager.Travel([](ServerSession& _otherServer)
				{
					auto serializedInfo{ ZppBits::Serialize(ServerCommon::Shutdown()) };
					_otherServer.Send(serializedInfo.serializedSize, serializedInfo.serializedBuffer, serializedInfo.deallocator);
				});

			GetApplication()->Shutdown();
			return nlohmann::json({ {"message", "done"} });
		});
}

void Server::CallRestfulHandler(const RestufulRequestId_t& _requestId, const std::wstring& _path, const std::wstring& _query, WebAccessor* _accessor)
{
	m_threadPool.PushJob([this, _requestId, _path, _query, _accessor]()
		{
			auto found = m_restfulHandlers.find(_path);
			if (m_restfulHandlers.end() == found)
			{
				return;
			}

			WebParams params;
			params.Parse(_query);

			if (!params.m_result)
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
		m_timerJobManager->Shutdown(EShutdownMode::RightNow, "timer job manager counter.");
	}
	m_threadPool.Shutdown(EShutdownMode::EmptyJob, "thread pool shutdown.");

	UserSession::UninitPacketHandlers();
	ServerSession::UninitPacketHandlers();

	m_timerJobManager.reset();
}

UserSessionShared_t Server::AddUserSession(ConnectionShared_t _conn)
{
	UserSessionShared_t user = UserSession::Create<UserSession>(m_threadPool, _conn);

	{
		SCOPED_WRITE_LOCK(m_userSessionsMutex);
		m_userSessions.emplace(_conn->GetConnectionId(), user);
	}

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
		user->PushJob([_result](UserSession& _user)
			{
				_user.Closed(_result);
			});
	}
}

ServerSessionShared_t Server::ServerConnected(ConnectionShared_t _conn)
{
	auto server = Module::As<Server>()->GetServerSessionManager().CreateSession(_conn);

	server->SendActivation(GetModuleType(), GetServerId(), GetServerGroupId());

	return server;
}