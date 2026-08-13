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

	void OnClosed(const Result& _reason, const ConnectionId_t _connectionId, const bool _isPublic)
	{
		Log("OnClosed");

		ClosedHandler_t closedHandler{};
		if (_isPublic)
		{
			SCOPED_WRITE_LOCK(m_publicConnectionMutex);
			const auto itor = m_publicConnections.find(_connectionId);
			if (m_publicConnections.end() == itor)
			{
				return;
			}
			closedHandler = std::get<ClosedHandler_t>(itor->second);
			m_publicConnections.erase(itor);
		}
		else
		{
			SCOPED_WRITE_LOCK(m_privateConnectionMutex);
			const auto itor = m_privateConnections.find(_connectionId);
			if (m_privateConnections.end() == itor)
			{
				return;
			}
			closedHandler = std::get<ClosedHandler_t>(itor->second);
			m_privateConnections.erase(itor);
		}

		if (closedHandler)
		{
			closedHandler(_reason, _connectionId, _isPublic);
		}
		else
		{
			LogError("closed handler is null");
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
	void PushShutdownThreadPoolSteps(ShutdownCoordinator& _coordinator, const std::string& _name)
	{
		m_asioThreadPool.PushShutdownSteps(_coordinator, _name);
		m_cvThreadPool.Shutdown(_coordinator, "ConnectionManager's thread pool shutdown.");
	}

	void PushCloseConnectionSteps(ShutdownCoordinator& _coordinator, const std::string& _name, const bool _closePublic)
	{
		_coordinator.Push(_name + " close connections", [this, _closePublic]()
			{
				if (_closePublic)
				{
					CloseAll(true);
				}
				CloseAll(false);

				return EStepResult::Done;
			});

		if (_closePublic)
		{
			_coordinator.Push(_name + " public connections closed", [this]()
				{
					return IsEmptyConnection(true) ? EStepResult::Done : EStepResult::Wait;
				});
		}

		_coordinator.Push(_name + " private connections closed", [this]()
			{
				return IsEmptyConnection(false) ? EStepResult::Done : EStepResult::Wait;
			});
	}

	asio::io_context* GetAsioContext()
	{
		return (EThreadPool::ASIO == m_threadPoolType) ? m_asioThreadPool.GetIoContext() : nullptr;
	}

	bool IsEmptyConnection(const bool& _isPublic, const std::optional<AcceptorIndex>& _acceptorIndex = {}) const
	{
		if (_isPublic)
		{
			SCOPED_READ_LOCK(m_publicConnectionMutex);
			if (_acceptorIndex.has_value())
			{
				for (const auto& [connection, _] : m_publicConnections | std::views::values)
				{
					SocketConnectionImpl* impl = static_cast<SocketConnectionImpl*>(connection.get());
					if (*_acceptorIndex == impl->GetAcceptorIndex())
					{
						return false;
					}
				}

				return true;
			}

			return m_publicConnections.empty();
		}

		SCOPED_READ_LOCK(m_privateConnectionMutex);
		return m_privateConnections.empty();
	}

	bool AddConnection(const ConnectionShared_t& _conn, const ClosedHandler_t _closedHandler)
	{
		return AddConnection(_conn, _closedHandler, []() {});
	}

	template<typename T_ON_ADDED>
	bool AddConnection(const ConnectionShared_t& _conn, const ClosedHandler_t _closedHandler, T_ON_ADDED&& _onAdded)
	{
		if (_conn->IsPublic())
		{
			SCOPED_WRITE_LOCK(m_publicConnectionMutex);
			if (IsShutdownStarted())
			{
				return false;
			}
			auto [_, inserted] = m_publicConnections.emplace(_conn->GetConnectionId(), std::make_tuple(_conn, _closedHandler));
			if (inserted)
			{
				std::forward<T_ON_ADDED>(_onAdded)();
			}
			return inserted;
		}
		else
		{
			SCOPED_WRITE_LOCK(m_privateConnectionMutex);
			if (IsShutdownStarted())
			{
				return false;
			}
			auto [_, inserted] = m_privateConnections.emplace(_conn->GetConnectionId(), std::make_tuple(_conn, _closedHandler));
			if (inserted)
			{
				std::forward<T_ON_ADDED>(_onAdded)();
			}
			return inserted;
		}
	}

	void CloseAll(const bool& _isPublic, const std::optional<AcceptorIndex>& _acceptorIndex = {})
	{
		if (_isPublic)
		{
			SCOPED_READ_LOCK(m_publicConnectionMutex);
			for (const auto& [connection, _] : m_publicConnections | std::views::values)
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
			for (const auto& [connection, _] : m_privateConnections | std::views::values)
			{
				connection->Close(EError::Shutdown);
			}
		}
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

		void PushShutdownSteps(ShutdownCoordinator& _coordinator, const std::string& _name)
		{
			_coordinator.Push(_name + " asio work guard reset", [this]()
				{
					if (m_workGuard)
					{
						m_workGuard->reset();
					}

					return ShutdownCoordinator::EStepResult::Done;
				});

			_coordinator.Push(_name + " asio join threads", [this]()
				{
					for (auto& thread : m_threads)
					{
						if (thread.joinable())
						{
							thread.join();
						}
					}
					SafeDelete(m_workGuard);

					m_ioContext.stop();

					return ShutdownCoordinator::EStepResult::Done;
				});
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

	mutable std::shared_mutex m_publicConnectionMutex;
	std::unordered_map<ConnectionId_t, std::tuple<ConnectionShared_t, ClosedHandler_t>> m_publicConnections;

	mutable std::shared_mutex m_privateConnectionMutex;
	std::unordered_map<ConnectionId_t, std::tuple<ConnectionShared_t, ClosedHandler_t>> m_privateConnections;
};
