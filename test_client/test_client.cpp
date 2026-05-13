#include "pch.h"

MODULE_STATIC_IMPL(TestClient);

TestClient::TestClient(const std::string& _configFilePath)
	: Module(_configFilePath), m_threadPool("TestClient ThreadPool")
{
}

TestClient::~TestClient()
{
}

static bool OnConnected(const Result& _result, const std::string& _connecterName, ConnectionShared_t _conn)
{
	if (!_result)
	{
		LogWarning("ConnectedHandler : {}", _result.message);
		return false;
	}

	Module::Get<TestClient>()->Connected(_conn);

	return true;
}

static void OnReceived(std::vector<uint8_t>&& _rawData, const ConnectionShared_t& _conn)
{
	auto const session = Module::Get<TestClient>()->GetSession();
	if (not session)
	{
		LogError("not exist session");
		return;
	}
	session->PushJob([_rawData = std::move(_rawData)](const SessionShared_t& _session) mutable
		{
			_session->CallPacketHandler(std::move(_rawData));
		});
}

static void OnClosed(const Result& _result, const ConnectionId_t _connectionId, const bool _isPublic)
{
	if (!_result.message.empty())
	{
		LogWarning("Closed Session : {}", _result.message);
	}
	Module::Get<TestClient>()->ApplicationShutdown(_result);
}

Result TestClient::InitImpl()
{
	Session::InitPacketHandlers();

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

	// timer
	auto timerModule = GetApplication()->GetModule(EModule::Timer);
	if (!timerModule)
	{
		return EError::NotExistModule;
	}
	m_timerJobManager = static_cast<TimerAccessor*>(timerModule->GetAccessor())->AllocateTimerJobManager(m_threadPool);

	// network
	ConnectedConfig connectedConfig{ OnConnected, OnReceived, OnClosed };

	NetworkHelper networkHelper;
	auto result = networkHelper.SettingByConfig(m_config, GetApplication(), AcceptedConfig(), connectedConfig);
	if (!result)
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
		m_timerJobManager->Shutdown(EShutdownMode::RightNow, "test_client timer job manager shutdown.");
	}
	m_threadPool.Shutdown(EShutdownMode::EmptyJob, "client thread pool shutdown.");

	Session::UninitPacketHandlers();

	m_timerJobManager.reset();
}

void TestClient::ApplicationShutdown(const Result& _result)
{
	if (m_session)
	{
		m_session->Closed(_result);
		m_session.reset();
	}

	GetApplication()->Shutdown();
}

// ------------------------------------------------------------------------------------------
static const char* data[] = {
			"He who spares the rod hates his son, but he who loves him is careful to discipline him.",
			"Miracles happen to only those who believe in them.",
			"Think like a man of action and act like man of thought.",
			"Courage is very important. Like a muscle, it is strengthened by use.",
			"Life is the art of drawing sufficient conclusions from insufficient premises.",
			"By doubting we come at the truth.",
			"A man that has no virtue in himself, ever envies virtue in others.",
			"When money speaks, the truth keeps silent.",
			"Better the last smile than the first laughter.",
			"In the morning of life, work; in the midday, give counsel; in the evening, pray.",
			"Painless poverty is better than embittered wealth.",
			"A poet is the painter of the soul.",
			"Error is the discipline through which we advance.",
			"Faith without deeds is useless.",
			"Weak things united become strong.",
			"We give advice, but we cannot give conduct.",
			"Nature never deceives us; it is always we who deceive ourselves.",
			"Forgiveness is better than revenge.",
			"We never know the worth of water till the well is dry.",
			"Pain past is pleasure.",
			"Books are ships which pass through the vast seas of time.",
			"Who begins too much accomplishes little.",
			"Better the last smile than the first laughter.",
			"Faith is a higher faculty than reason.",
			"Until the day of his death, no man can be sure of his courage.",
			"Great art is an instant arrested in eternity.",
			"Faith without deeds is useless.",
			"The world is a beautiful book, but of little use to him who cannot read it.",
			"Heaven gives its favorites-early death.",
			"I never think of the future. It comes soon enough.",
			"Suspicion follows close on mistrust.",
			"All good things which exist are the fruits of originality.",
			"The will of a man is his happiness.",
			"He that has no shame has no conscience.",
			"Weak things united become strong.",
			"A minute's success pays the failure of years.",
			"United we stand, divided we fall.",
			"To doubt is safer than to be secure.",
			"Time is but the stream I go a-fishing in.",
			"A full belly is the mother of all evil.",
			"Love your neighbor as yourself.",
			"It is a wise father that knows his own child.",
			"By doubting we come at the truth.",
			"Absence makes the heart grow fonder.",
			"Habit is second nature.",
			"Who knows much believes the less.",
			"Only the just man enjoys peace of mind.",
			"Waste not fresh tears over old griefs.",
			"Life itself is a quotation.",
			"He is greatest who is most often in men's good thoughts.",
			"Envy and wrath shorten the life.",
			"Where there is no desire, there will be no industry.",
			"To be trusted is a greater compliment than to be loved.",
			"Education is the best provision for old age.",
			"To jaw-jaw is better than to war-war.",
			"Music is a beautiful opiate, if you don't take it too seriously.",
			"Appearances are deceptive.",
			"Let thy speech be short, comprehending much in few words.",
			"Things are always at their best in the beginning.",
			"A gift in season is a double favor to the needy.",
			"In giving advice, seek to help, not to please, your friend.",
			"The difficulty in life is the choice.",
			"The most beautiful thing in the world is, of course, the world itself.",
			"All fortune is to be conquered by bearing it.",
			"Better is to bow than break.",
			"Good fences makes good neighbors.",
			"Give me liberty, or give me death.",
};

void TestSendFunc(uint64_t sendIndex, SessionShared_t _session)
{
	++sendIndex;

	auto title = std::format("test message {}", sendIndex);
	Client::TestMessage packet(title, ::data[0]);

	auto serializedInfo{ ZppBits::Serialize(packet) };
	_session->Send(serializedInfo.serializedSize, serializedInfo.serializedBuffer, serializedInfo.deallocator);

	if (sendIndex == 66)
	{
		sendIndex = 0;
	}

	auto jobInst = ThreadPool::MakeJobInst([sendIndex, _session]()
		{
			TestSendFunc(sendIndex, _session);
		});

	Module::Get<TestClient>()->GetTimerJobManager()->PushTimerJob(std::move(jobInst), 16);
}
// ------------------------------------------------------------------------------------------

SessionShared_t TestClient::Connected(ConnectionShared_t _conn)
{
	m_session = Session::Create<Session>(m_threadPool, _conn);

	auto jobInst = ThreadPool::MakeJobInst([session = m_session]()
		{
			TestSendFunc(0, session);
		});

	m_timerJobManager->PushTimerJob(std::move(jobInst), 16);

	return m_session;
}