#pragma once

static_assert(NETWORK_MODULE == 1);

class Network;
class ConnectionManager : public UseShutdown
{
public:
	enum class EThreadPool
	{
		ASIO,
		CV	// use std condition variable
	};
public:
	explicit ConnectionManager(Network* _networkMoudle, const EThreadPool& _threadPoolType)
		: m_networkMoudle(*_networkMoudle), m_threadPoolType(_threadPoolType), m_cvThreadPool("ConnectionMaager")
	{}
	virtual ~ConnectionManager()
	{
		if (!IsEmptyConnection(true))
		{
			LogWarning("Remain Public Connection");
		}

		if (!IsEmptyConnection(false))
		{
			LogWarning("Remain private Connection");
		}
	}

	void OnClosed(const ConnectionId_t& _connectionId, const bool& _isPublic)
	{
		Log("OnClosed");
		if (_isPublic)
		{
			SCOPED_WRITE_LOCK(m_publicConnectionMutex);
			m_publicConnections.erase(_connectionId);
		}
		else
		{
			SCOPED_WRITE_LOCK(m_privateConnectionMutex);
			m_privateConnections.erase(_connectionId);
		}
	}

protected:
	void InitThreadPool(const uint16_t& _threadCount)
	{
		m_threadCount = _threadCount;
		switch (m_threadPoolType)
		{
		case EThreadPool::ASIO:
			{
				m_asioThreadPool.Init(m_threadCount);
			}
			break;
		case EThreadPool::CV:
			{
				m_cvThreadPool.Init(m_threadCount);
			}
			break;
		}
	}
	void ShutdownThreadPool()
	{
		m_asioThreadPool.Shutdown();
		m_cvThreadPool.Shutdown("ConnectionManager's thread pool shutdown.");
	}

	asio::io_context* GetAsioContext()
	{
		return (EThreadPool::ASIO == m_threadPoolType) ? m_asioThreadPool.GetIoContext() : nullptr;
	}

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

	bool AddConnection(const ConnectionShared_t& _conn)
	{
		if (_conn->IsPublic())
		{
			SCOPED_WRITE_LOCK(m_publicConnectionMutex);
			if (IsShutdown())
			{
				return false;
			}
			m_publicConnections.emplace(_conn->GetConnectionId(), _conn);
		}
		else
		{
			SCOPED_WRITE_LOCK(m_privateConnectionMutex);
			if (IsShutdown())
			{
				return false;
			}
			m_privateConnections.emplace(_conn->GetConnectionId(), _conn);
		}
		return true;
	}

	void CloseAll(const bool& _isPublic, const std::optional<AcceptorIndex>& _acceptorIndex = {})
	{
		if (_isPublic)
		{
			SCOPED_READ_LOCK(m_publicConnectionMutex);
			for (auto& [connectionId, connection] : m_publicConnections)
			{
				SocketConnectionImpl* impl = static_cast<SocketConnectionImpl*>(connection.get());
				if (_acceptorIndex.has_value() && *_acceptorIndex != impl->GetAcceptorIndex())
				{
					continue;
				}
				connection->Close(EError::Shutdown);
			}
		}
		else
		{
			SCOPED_READ_LOCK(m_privateConnectionMutex);
			for (auto& [connectionId, connection] : m_privateConnections)
			{
				connection->Close(EError::Shutdown);
			}
		}
	}
	void CloseAllAndWait(const bool& _isPublic, const std::optional<AcceptorIndex>& _acceptorIndex = {})
	{
		CloseAll(_isPublic, _acceptorIndex);

		do
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		} while (!IsEmptyConnection(_isPublic));
	}

protected:
	class AsioThreadPool
	{
	public:
		AsioThreadPool() = default;
		~AsioThreadPool() = default;

		void Init(const uint16_t& _threadCount)
		{
			m_workGuard = new WorkGuard_t(m_ioContext.get_executor());
			for (uint16_t i = 0; i < _threadCount; ++i)
			{
				m_threads.emplace_back([this]()
					{
						Log("Connecter m_ioContext run.");
						m_ioContext.run();
						Log("Connecter thread stop.");
					});
			}
		}
		void Shutdown()
		{
			if (m_workGuard)
			{
				m_workGuard->reset();
			}

			for (auto& thread : m_threads)
			{
				thread.join();
			}
			SAFE_DELETE(m_workGuard);

			m_ioContext.stop();
		}

		inline asio::io_context* GetIoContext() { return &m_ioContext; }
	private:
		asio::io_context m_ioContext;
		using WorkGuard_t = asio::executor_work_guard<asio::io_context::executor_type>;
		WorkGuard_t* m_workGuard = nullptr;
		std::vector<std::thread> m_threads;
	};

protected:
	Network& m_networkMoudle;

	uint16_t m_threadCount = 0;
	EThreadPool m_threadPoolType;

	AsioThreadPool m_asioThreadPool;
	ThreadPool m_cvThreadPool;

	std::shared_mutex m_publicConnectionMutex;
	std::unordered_map<ConnectionId_t, ConnectionShared_t> m_publicConnections;

	std::shared_mutex m_privateConnectionMutex;
	std::unordered_map<ConnectionId_t, ConnectionShared_t> m_privateConnections;
};