#pragma once

class Network;
class ConnectionManager
{
public:
	ConnectionManager(Network* _networkMoudle) : m_networkMoudle(*_networkMoudle) {}
	virtual ~ConnectionManager() = default;

	void OnClosed(const ConnectionId_t& _connectionId, const bool& _isPublic)
	{
		ConnectionShared_t conn;
		if (_isPublic)
		{
			SCOPED_WRITE_LOCK(m_publicConnectionMutex);
			auto found = m_publicConnections.find(_connectionId);
			if (found != m_publicConnections.end())
			{
				conn = found->second;
				m_publicConnections.erase(found);
			}
		}
		else
		{
			SCOPED_WRITE_LOCK(m_privateConnectionMutex);
			auto found = m_privateConnections.find(_connectionId);
			if (found != m_privateConnections.end())
			{
				conn = found->second;
				m_privateConnections.erase(found);
			}
		}
	}

protected:
	bool IsEmptyConnection(const bool& _isPublic)
	{
		if (_isPublic)
		{
			SCOPED_READ_LOCK(m_publicConnectionMutex);
			return m_publicConnections.empty();
		}

		SCOPED_READ_LOCK(m_privateConnectionMutex);
		return m_privateConnections.empty();
	}
	void AddConnection(const ConnectionId_t& _connectionId, ConnectionShared_t _conn, const bool& _isPublic)
	{
		if (_isPublic)
		{
			SCOPED_WRITE_LOCK(m_publicConnectionMutex);
			m_publicConnections.emplace(_connectionId, _conn);
		}
		else
		{
			SCOPED_WRITE_LOCK(m_privateConnectionMutex);
			m_privateConnections.emplace(_connectionId, _conn);
		}
	}
	void CloseAll(const bool& _isPublic, const std::optional<AcceptorIndex>& _acceptorIndex = {})
	{
		// ------------------------------------------------------------------------------
		// if insert to m_publicConnections or m_privateConnections after this function, it must be closed because checking m_stop.
		// ------------------------------------------------------------------------------
		if (_isPublic)
		{
			SCOPED_WRITE_LOCK(m_publicConnectionMutex);
			for (auto& [connectionId, connection] : m_publicConnections)
			{
				ConnectionImpl* impl = static_cast<ConnectionImpl*>(connection.get());
				if (_acceptorIndex.has_value() && *_acceptorIndex != impl->GetAcceptorIndex())
				{
					continue;
				}
				connection->Close(EError::Shutdown);
			}
		}
		else
		{
			SCOPED_WRITE_LOCK(m_privateConnectionMutex);
			for (auto& [connectionId, connection] : m_privateConnections)
			{
				connection->Close(EError::Shutdown);
			}
		}
	}

protected:
	Network& m_networkMoudle;

	std::atomic_bool m_shutdown = false;

	io_context m_ioContext;
	using WorkGuard_t = executor_work_guard<io_context::executor_type>;
	WorkGuard_t* m_workGuard = nullptr;

	uint16_t m_threadCount = 0;
	our::vector<std::thread> m_threads;

	std::shared_mutex m_publicConnectionMutex;
	our::unordered_map<ConnectionId_t, ConnectionShared_t> m_publicConnections;

	std::shared_mutex m_privateConnectionMutex;
	our::unordered_map<ConnectionId_t, ConnectionShared_t> m_privateConnections;
};