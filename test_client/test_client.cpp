#include "pch.h"

MODULE_STATIC_IMPL(TestClient);

TestClient::TestClient(const std::string& _configFilePath)
	: Module(_configFilePath)
{
}

TestClient::~TestClient()
{
}

Result TestClient::InitImpl()
{
	Session::InitPacketHandlers();

	m_threadPool.Init(1);

	// timer
	auto timerModule = GetApplication()->GetModule(EModule::Timer);
	if (!timerModule)
	{
		return EError::NotExistModule;
	}
	m_timerJobManager = TimerJobManager::Create<TimerJobManager>(m_threadPool);
	m_timerJobManager->Init(static_cast<TimerAccessor*>(timerModule->GetAccessor()));

	// network
	NetworkHelper networkHelper;
	ModuleAccessor* networkAccessor = nullptr;
	auto result = networkHelper.SettingByConfig(m_config, GetApplication(), (ModuleAccessor*&)networkAccessor,
		[](const Result& _result, ConnectionShared_t _conn) -> bool
		{
			// not use.
			return true;
		},
		[](const Result & _result, const std::string & _connecterName, ConnectionShared_t _conn) -> bool
		{
			if (_result != EError::Success)
			{
				LogWarning("failed to connect");
				return false;
			}

			Module::Get<TestClient>()->SetSession(_conn);
			auto session = Module::Get<TestClient>()->GetSession();

			_conn->SetReceivedHandler([session](our::vector<uint8_t>&& _rawData, ConnectionShared_t _conn)
				{
					session->PushJob([session, _rawData = std::move(_rawData)]() mutable
						{
							session->Get<Session>()->CallPacketHandler(std::move(_rawData));
						});
				});

			//_conn->SetSentHandler([](const size_t& _size){});

			_conn->SetClosedHandler([](const Result& _result, const ConnectionId_t& _connectionId, const bool& _isPublic)
				{
					if (!_result.message.empty())
					{
						LogWarning("Closed Session : {}", _result.message);
					}
					Module::Get<TestClient>()->ClosedSession(_result, _connectionId);
				});

			std::weak_ptr<Session> weakSession = session;
            Module::Get<TestClient>()->GetTimerJobManager()->PushJob([weakSession]() mutable
                {
					auto session = weakSession.lock();
					if (session != nullptr)
					{
						session->SendTestMessage();
					}
                }, 20);

			return true;
		});

	if (result != EError::Success)
	{
		return result;
	}

	return EError::Success;
}

void TestClient::Shutdown()
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
		Log("TestClient Shutdown : waiting until thread pool is empty.");
	}
	m_threadPool.Shutdown();

	Session::UninitPacketHandlers();
}

void TestClient::SetSession(ConnectionShared_t& _conn)
{
	m_session = Session::Create(_conn, m_threadPool);
}

void TestClient::ClosedSession(const Result& _result, const ConnectionId_t& _connId)
{
	if (m_session)
	{
		m_session->PushJob([_session = m_session, _result]()
			{
				_session->Closed(_result);
			});
        m_session.reset();
	}
}