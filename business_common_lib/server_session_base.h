#pragma once

using ServerId_t = uint16_t;
#define INVALID_SERVER_ID 0
using ServerGroupId_t = uint16_t;
#define INVALID_SERVER_GROUP_ID 0

#define INVALID_SERVER_SERIAL 0
union ServerSerial
{
	uint64_t all;
	struct {
		ServerId_t id;
		EModule moduleType;
		ServerGroupId_t groupId;
	};
	ServerSerial() : all(INVALID_SERVER_SERIAL) {}
	ServerSerial(const ServerId_t& _id, const EModule& _moduleType, const ServerGroupId_t& _groupId)
	{
		all = INVALID_SERVER_SERIAL;
		id = _id;
		moduleType = _moduleType;
		groupId = _groupId;
	}

	bool operator == (const ServerSerial& _rhs) const
	{
		return all == _rhs.all;
	}

	bool operator != (const ServerSerial& _rhs) const
	{
		return !(operator==(_rhs));
	}

	inline bool IsValid() { return (all != INVALID_SERVER_SERIAL); }
};

struct ServerSerialHash {
	std::size_t operator()(const ServerSerial& _serial) const
	{
		return std::hash<uint64_t>::_Do_hash(_serial.all);
	}
};

// -------------------------------------------------------------------------------------------

template<class CT>
class ServerSessionBase : public SerializedJobQueue
{
	using Super_t = SerializedJobQueue;
public:
	using CommonPacketHandlerCallers_t = std::unordered_map<ServerCommon::Protocol, FbPacketHandleCallerBase<std::shared_ptr<CT>, ServerCommon::Body>*>;

	virtual ~ServerSessionBase()
	{
		m_connection.reset();
	}

	Result Send(const PacketSize_t& _size, const uint8_t* _serializedData, PacketDeallocatorShared_t& _deallocator)
	{
		return m_connection->Send(_size, _serializedData, _deallocator);
	}

	void Close(const Result& _reason)
	{
		m_connection->Close(_reason);
	}

	inline ConnectionId_t GetConnectionId() { return m_connection->GetConnectionId(); }
	inline const std::string& GetRemoteAddress() { return m_connection->GetRemoteAddress(); }

	inline void SetServerSerial(const ServerSerial& _serial) { m_serial = _serial; }
	inline ServerSerial GetServerSerial() { return m_serial; }

	void SendActivation(const EModule& _moduleType, const ServerId_t& _serverId, const ServerGroupId_t& _serverGroupId)
	{
		FLAT_BUFFER_BUILDER(builder);
		auto activation = ServerCommon::CreateActivation(builder, std::underlying_type_t<EModule>(_moduleType), _serverId, _serverGroupId);
		auto serverCommon = ServerCommon::CreateBody(builder, ServerCommon::Protocol::Protocol_Activation, activation.Union());
		auto packet = CreateServerPacketBody(builder, ServerPacket::ServerPacket_ServerCommon_Body, serverCommon.Union());
		builder.Finish(packet);

		Fb::SerializedInfo serializedInfo = Fb::MakeSerializedInfo(builder);
		m_connection->Send(serializedInfo.serializedSize, serializedInfo.serializedBuffer, serializedInfo.deallocator);
	}

protected:
	ServerSessionBase(ConnectionShared_t _conn, ThreadPool& _threadPool)
		: Super_t(_threadPool), m_connection(_conn)
	{
	}

	static void InitCommonPacketHandlers()
	{
		m_commonPacketHandlerCallers.emplace(ServerCommon::Protocol::Protocol_Activation, new FbPacketHandleCaller<std::shared_ptr<CT>, ServerCommon::Body, ServerCommon::Activation>([](std::shared_ptr<CT>&) { return true; }));
		m_commonPacketHandlerCallers.emplace(ServerCommon::Protocol::Protocol_Shutdown, new FbPacketHandleCaller<std::shared_ptr<CT>, ServerCommon::Body, ServerCommon::Shutdown>([](std::shared_ptr<CT>&) { return true; }));
	}

	static void UninitCommonPacketHandlers()
	{
		for (auto& [protocol, handler] : m_commonPacketHandlerCallers)
		{
			SAFE_DELETE(handler);
		}
		m_commonPacketHandlerCallers.clear();
	}

	bool CallCommonPacketHandler(const ServerCommon::Body* _packetBody, our::vector<uint8_t>&& _rawData)
	{
		auto protocol = _packetBody->body_type();
		auto found = m_commonPacketHandlerCallers.find(protocol);
		if (m_commonPacketHandlerCallers.end() == found)
		{
			return false;
		}

		auto self = Get<CT>();
		return found->second->CallHandler(self, _packetBody, std::move(_rawData));
	}

	ServerSerial m_serial;
	ConnectionShared_t m_connection;

	static CommonPacketHandlerCallers_t m_commonPacketHandlerCallers;
};

// -------------------------------------------------------------------------------------

template<class CT> requires std::derived_from<CT, ServerSessionBase<CT>>
class ServerSessionManager
{
public:
	using SessoinShared_t = std::shared_ptr<CT>;

	ServerSessionManager(ThreadPool& _threadPool)
		: m_threadPool(_threadPool)
	{
	}

	virtual ~ServerSessionManager()
	{
	}

	SessoinShared_t CreateSession(ConnectionShared_t _conn)
	{
		return CT::Create(_conn, m_threadPool);
	}

	SessoinShared_t FindSession(const ConnectionId_t& _connectionId)
	{
		SCOPED_READ_LOCK(m_mutex);
		auto found = m_sessions.find(_connectionId);
		return (found == m_sessions.end()) ? nullptr : found->second;
	}

	SessoinShared_t FindSessionById(const ServerId_t& _serverId)
	{
		SCOPED_READ_LOCK(m_mutex);
		auto found = m_sessionsById.find(_serverId);
		return (found == m_sessionsById.end()) ? nullptr : found->second;
	}

	SessoinShared_t RemoveSession(const ConnectionId_t& _connectionId)
	{
		auto session = FindSession(_connectionId);
		if (session == nullptr)
		{
			return session;
		}
		
		SCOPED_WRITE_LOCK(m_mutex);
		m_sessions.erase(_connectionId);

		auto serial = session->GetServerSerial();
		if (!serial.IsValid())
		{
			return session;
		}

		if (m_sessionsById.erase(serial.id) > 0)
		{
			Log("Deactivated Server : {}, {}, {}", serial.moduleType, serial.groupId, serial.id);

			ServerSerial copiedSerial(INVALID_SERVER_ID, serial.moduleType, serial.groupId);
			{
				auto found = m_sessionsByDivision.find(copiedSerial);
				if (found != m_sessionsByDivision.end())
				{
					for (auto itr = found->second.begin(); itr != found->second.end(); ++itr)
					{
						if ((*itr)->GetServerSerial() == serial)
						{
							found->second.erase(itr);
							break;
						}
					}
				}
			}

			copiedSerial.moduleType = EModule::None;
			{
				auto found = m_sessionsByDivision.find(copiedSerial);
				if (found != m_sessionsByDivision.end())
				{
					for (auto itr = found->second.begin(); itr != found->second.end(); ++itr)
					{
						if ((*itr)->GetServerSerial() == serial)
						{
							found->second.erase(itr);
							break;
						}
					}
				}
			}

			copiedSerial.groupId = INVALID_SERVER_GROUP_ID;
			copiedSerial.moduleType = serial.moduleType;
			{
				auto found = m_sessionsByDivision.find(copiedSerial);
				if (found != m_sessionsByDivision.end())
				{
					for (auto itr = found->second.begin(); itr != found->second.end(); ++itr)
					{
						if ((*itr)->GetServerSerial() == serial)
						{
							found->second.erase(itr);
							break;
						}
					}
				}
			}
		}
		return session;
	}

	bool AddAuthorizedSession(const ServerSerial& _serial, std::shared_ptr<CT> _session)
	{
		_session->SetServerSerial(_serial);

		SCOPED_WRITE_LOCK(m_mutex);

		{
			auto ret = m_sessions.emplace(_session->GetConnectionId(), _session);
			if (!ret.second)
			{
				Log("Duplicated Server Connection Id : {}, {}, {}, {}", _serial.moduleType, _serial.groupId, _serial.id, _session->GetRemoteAddress());
				return false;
			}
		}

		{
			auto ret = m_sessionsById.emplace(_serial.id, _session);
			if (!ret.second)
			{
				Log("Duplicated Server : {}, {}, {}, {}", _serial.moduleType, _serial.groupId, _serial.id, _session->GetRemoteAddress());
				return false;
			}
			Log("Activated Server : {}, {}, {}, {}", _serial.moduleType, _serial.groupId, _serial.id, _session->GetRemoteAddress());
		}

		ServerSerial copiedSerial(INVALID_SERVER_ID, _serial.moduleType, _serial.groupId);
		{
			auto found = m_sessionsByDivision.find(copiedSerial);
			if (found == m_sessionsByDivision.end())
			{
				m_sessionsByDivision.emplace(copiedSerial, our::list{ _session });
			}
			else
			{
				found->second.push_back(_session);
			}
		}

		copiedSerial.moduleType = EModule::None;
		{
			auto found = m_sessionsByDivision.find(copiedSerial);
			if (found == m_sessionsByDivision.end())
			{
				m_sessionsByDivision.emplace(copiedSerial, our::list{ _session });
			}
			else
			{
				found->second.push_back(_session);
			}
		}

		copiedSerial.groupId = INVALID_SERVER_GROUP_ID;
		copiedSerial.moduleType = _serial.moduleType;
		{
			auto found = m_sessionsByDivision.find(copiedSerial);
			if (found == m_sessionsByDivision.end())
			{
				m_sessionsByDivision.emplace(copiedSerial, our::list{ _session });
			}
			else
			{
				found->second.push_back(_session);
			}
		}

		return true;
	}

	void Travel(std::function<void(SessoinShared_t&)>&& _travelFunc)
	{
		for (auto& [connectionId, session] : m_sessions)
		{
			_travelFunc(session);
		}
	}

private:
	// -------------------------------------------------------------------------
	// server session connection or disconnection is not frequent.
	// so, mutex is used almost exclusively for read lock.
	// -------------------------------------------------------------------------
	std::shared_mutex m_mutex;

	ThreadPool& m_threadPool;

	our::unordered_map<ConnectionId_t, SessoinShared_t> m_sessions;
	our::unordered_map<ServerId_t, SessoinShared_t> m_sessionsById;
	our::unordered_map<ServerSerial, our::list<SessoinShared_t>, ServerSerialHash> m_sessionsByDivision;
};