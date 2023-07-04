#include "pch.h"

Session::PacketHandlerCallers_t Session::m_packetHandlerCallers;

Session::Session(ConnectionShared_t _conn, ThreadPool& _threadPool)
	: Super_t(_threadPool), m_connection(_conn)
{
}

Session::~Session()
{
	m_connection.reset();
	Log("deleted Session");
}

Result Session::Send(const PacketSize_t& _size, const uint8_t* _serializedData, PacketDeallocatorShared_t& _deallocator)
{
	return m_connection->Send(_size, _serializedData, _deallocator);
}

void Session::Close(const Result& _reason)
{
	m_connection->Close(_reason);
}

void Session::Closed(const Result& _result)
{
	Shutdown();
}

void Session::InitPacketHandlers()
{
	m_packetHandlerCallers.emplace(Client::Protocol::Protocol_TestMessage,
		new FbPacketHandleCaller<SessionShared_t, Client::Body, Client::TestMessage>([](SessionShared_t&) { return true; }));
}

void Session::UninitPacketHandlers()
{
	for (auto& [protocol, handler] : m_packetHandlerCallers)
	{
		SAFE_DELETE(handler);
	}
	m_packetHandlerCallers.clear();
}

bool Session::CallPacketHandler(our::vector<uint8_t>&& _rawData)
{
	const Client::Body* _packetBody = Client::GetBody(&_rawData[0]);

	Client::Protocol protocol = _packetBody->body_type();
	auto found = m_packetHandlerCallers.find(protocol);
	if (m_packetHandlerCallers.end() == found)
	{
		return false;
	}

	auto self = Get<Session>();
	return found->second->CallHandler(self, _packetBody, std::move(_rawData));
}

void Session::SendTestMessage()
{
    if (IsShutdown())
    {
        return;
    }

    auto sendIndex = std::rand() % 66;

    const char* data[] = {
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

    FLAT_BUFFER_BUILDER(builder);
    auto title = builder.CreateString(std::format("test message {}", sendIndex));
    auto message = builder.CreateString(data[sendIndex]);
    auto remessage = Client::CreateTestMessage(builder, title, message);
    auto packet = Client::CreateBody(builder, Client::Protocol::Protocol_TestMessage, remessage.Union());
    builder.Finish(packet);

    Fb::SerializedInfo serializedInfo = Fb::MakeSerializedInfo(builder);
    Send(serializedInfo.serializedSize, serializedInfo.serializedBuffer, serializedInfo.deallocator);

    std::weak_ptr<Session> weakSession = Get<Session>();
    Module::Get<TestClient>()->GetTimerJobManager()->PushJob([weakSession]() mutable
        {
            auto session = weakSession.lock();
            if (session != nullptr)
            {
                session->SendTestMessage();
            }
        }, 20);
}