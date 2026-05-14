#pragma once

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
	ServerSerial(const ServerSerial& _other)
	{
		all = _other.all;
	}
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
	const ServerSerial& operator = (const ServerSerial& _rhs)
	{
		all = _rhs.all;
		return *this;
	}

	bool operator()() const { return (all != INVALID_SERVER_SERIAL); }
};

namespace std
{
	template <>
	struct hash<ServerSerial>
	{
		size_t operator()(const ServerSerial& _serial) const noexcept
		{
			return std::hash<uint64_t>{}(_serial.all);
		}
	};
}

// -------------------------------------------------------------------------------------------

template<class T>
class ServerSessionBase : public SerializedJobQueue
{
	friend SerializedJobQueue;
	using Super_t = SerializedJobQueue;
public:
	using CommonPacketHandlerCallers_t = std::unordered_map<ServerCommon::EProtocol, ZppBitsPacketHandleCallerBase<T>*>;

	virtual ~ServerSessionBase()
	{
		m_connection.reset();
	}

	Result Send(const PacketSize_t& _size, const uint8_t* _serializedData, const PacketDeallocatorShared_t& _deallocator)
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
	inline ServerSerial GetServerSerial() const { return m_serial; }

	void SendActivation(const EModule& _moduleType, const ServerId_t& _serverId, const ServerGroupId_t& _serverGroupId)
	{
		const ServerCommon::Activation req(_moduleType, _serverId, _serverGroupId);

		auto serializedInfo{ ZppBits::Serialize(req) };
		m_connection->Send(serializedInfo.serializedSize, serializedInfo.serializedBuffer, serializedInfo.deallocator);
	}

protected:
	explicit ServerSessionBase(ThreadPool& _threadPool, ConnectionShared_t _conn)
		: Super_t(_threadPool), m_connection(_conn)
	{
	}

	static void InitCommonPacketHandlers()
	{
		m_commonPacketHandlerCallers.emplace(ServerCommon::EProtocol::Activation, new ZppBitsPacketHandleCaller<T, ServerCommon::Activation>([](const T&) { return true; }));
		m_commonPacketHandlerCallers.emplace(ServerCommon::EProtocol::Shutdown, new ZppBitsPacketHandleCaller<T, ServerCommon::Shutdown>([](const T&) { return true; }));
	}

	static void UninitCommonPacketHandlers()
	{
		for (auto& [protocol, handler] : m_commonPacketHandlerCallers)
		{
			SAFE_DELETE(handler);
		}
		m_commonPacketHandlerCallers.clear();
	}

	bool CallCommonPacketHandler(const ServerCommon::EProtocol protocol, zpp::bits::in<std::vector<uint8_t>>& _in)
	{
		auto found = m_commonPacketHandlerCallers.find(protocol);
		if (m_commonPacketHandlerCallers.end() == found)
		{
			return false;
		}

		return found->second->CallHandler(As<T>(), _in);
	}

	ServerSerial m_serial;
	ConnectionShared_t m_connection;

	static inline CommonPacketHandlerCallers_t m_commonPacketHandlerCallers;
};

// -------------------------------------------------------------------------------------

template<class T> requires std::derived_from<T, ServerSessionBase<T>>
class ServerSessionManager
{
public:
	using SessoinShared_t = std::shared_ptr<T>;

	ServerSessionManager(ThreadPool& _threadPool)
		: m_threadPoolRef(_threadPool)
	{
	}

	virtual ~ServerSessionManager()
	{
	}

	SessoinShared_t CreateSession(ConnectionShared_t _conn)
	{
		auto newSession = SerializedJobQueue::Create<T>(m_threadPoolRef, _conn);
		{
			SCOPED_WRITE_LOCK(m_mutex);
			m_sessions.try_emplace(_conn->GetConnectionId(), newSession);
		}
		return std::move(newSession);
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
		const auto session = FindSession(_connectionId);
		if (nullptr == session)
		{
			return session;
		}
		
		SCOPED_WRITE_LOCK(m_mutex);
		m_sessions.erase(_connectionId);

		auto serial = session->GetServerSerial();
		if (!serial())
		{
			return session;
		}

		if (m_sessionsById.erase(serial.id) > 0)
		{
			Log("Deactivated Server : {}, {}, {}", serial.moduleType, serial.groupId, serial.id);

			ServerSerial copiedSerial(ServerId_t::GetInvalidValue(), serial.moduleType, serial.groupId);
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

			copiedSerial.groupId = ServerGroupId_t::GetInvalidValue();
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

	bool AddAuthorizedSession(const ServerSerial& _serial, std::shared_ptr<T> _session)
	{
		_session->SetServerSerial(_serial);

		SCOPED_WRITE_LOCK(m_mutex);

		m_isAuthorized = true;
		{
			auto ret = m_sessionsById.emplace(_serial.id, _session);
			if (!ret.second)
			{
				Log("Duplicated Server : {}, {}, {}, {}", Module::GetModuleName(_serial.moduleType), _serial.groupId, _serial.id, _session->GetRemoteAddress());
				return false;
			}
			Log("Activated Server : {}, {}, {}, {}", Module::GetModuleName(_serial.moduleType), _serial.groupId, _serial.id, _session->GetRemoteAddress());
		}

		ServerSerial copiedSerial(ServerId_t::GetInvalidValue(), _serial.moduleType, _serial.groupId);
		{
			auto found = m_sessionsByDivision.find(copiedSerial);
			if (found == m_sessionsByDivision.end())
			{
				m_sessionsByDivision.emplace(copiedSerial, std::list{ _session });
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
				m_sessionsByDivision.emplace(copiedSerial, std::list{ _session });
			}
			else
			{
				found->second.push_back(_session);
			}
		}

		copiedSerial.groupId = ServerGroupId_t::GetInvalidValue();
		copiedSerial.moduleType = _serial.moduleType;
		{
			auto found = m_sessionsByDivision.find(copiedSerial);
			if (found == m_sessionsByDivision.end())
			{
				m_sessionsByDivision.emplace(copiedSerial, std::list{ _session });
			}
			else
			{
				found->second.push_back(_session);
			}
		}

		return true;
	}

	void Travel(std::function<void(T&)>&& _travelFunc)
	{
		SCOPED_READ_LOCK(m_mutex);
		for (auto& [connectionId, session] : m_sessions)
		{
			_travelFunc(*session.get());
		}
	}

private:
	// -------------------------------------------------------------------------
	// server session connection or disconnection is not frequent.
	// so, mutex is used almost exclusively for read lock.
	// -------------------------------------------------------------------------
	std::shared_mutex m_mutex;

	ThreadPool& m_threadPoolRef;

	bool m_isAuthorized{};
	std::unordered_map<ConnectionId_t, SessoinShared_t> m_sessions;
	std::unordered_map<ServerId_t, SessoinShared_t> m_sessionsById;
	std::unordered_map<ServerSerial, std::list<SessoinShared_t>> m_sessionsByDivision;
};