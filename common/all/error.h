#pragma once

enum class EError : uint32_t
{
	Success = 0,
	LackReceiveBuffer,
	InvalidParam,
	FailedFileOpen,
	NotExistNetworkListener,
	NotExistNetworkImn,
	NeedAcceptedHandler,
	AlreadySettedHandler,
	FailedNetworkAccept,
	OverMaxPacketSize,
	FailedSend,
	InvalidSocket,
	ClosedSocket,
	FailedReceive,
	FailedNetworkConnect,
	NotExistConnectInfo,
	NeedConnectedHandler,
	FailedAddConnection,
	InvalidConfig,
	InvalidDll,
	InvalidServerId,
	InvalidServerGroupId,
	InvalidThreadCount,
	FailedCreateModule,
	EmptyModule,
	DuplicatedModule,
	NotExistSendBuffer,
	NotExistModule,
	NotExistModuleAccessor,
	RestfulException,
	NotExistRestfulListener,
	AlreadyUsingRestfulListener,
	AlreadyExistDbName,
	FailedDbConnectionContainer,
	FailedAllocMssqlEnv,
	FailedSetMssqlEnvAttr,
	FailedAllocMssqlDbc,
	FailedSetMssqlConnAttr,
	FailedConnectMssql,
	FailedAllocMssqlStmt,
	FailedBindParameter,
	FailedResetMssqlStmt,
	FailedBorrowDbConnection,
	FailedSetAutoCommit,
	FailedEndTran,
	FailedRollback,
	NotExistDbConnection,
	EmptyQuery,
	FailedExecuteQuery,
	FailedBindCol,
	FailedQueriedFetch,
	FailedParseWeb,
	Kick,
	FailedCreateRedis,
	RedisException,
	InvalidUniqueIdPrefix,
	InvalidUniqueIdTimeUnit,
	NotExistAuthKey,
	TimeoutAuthKey,
	AlreadyTimerAllocated,
	AlreadyExistSameServer,
	NotInGrid,
	FailedLoadNavMesh,
	UnknownCommand,
	InvalidCommandParams,
	ActorIsNull,
	Shutdown,
	NotSupportShutdownMode,
	EmptyPacket,
	FailedLogin,
	NotInWorld,
	NotInitializedRestful
};

template <>
struct std::formatter<EError, char> : std::formatter<int, char>
{
	template <class T_FORMAT_CONTEXT>
	auto format(const EError& _Val, T_FORMAT_CONTEXT& _formatCtx) const
	{
		return std::format_to(_formatCtx.out(), "{}", magic_enum::enum_name(_Val));
	}
};

// -----------------------------------------------------------------------

struct Result
{
	EError error{ EError::Success };
	std::string message;

	Result()
	{
	}
	Result(const EError& _error) // not use explicit for convenience
	{
		error = _error;
	}
	explicit Result(const EError& _error, const std::string& _message)
	{
		error = _error;
		message = _message;
	}
	Result(const Result& _other) // not use explicit for convenience
	{
		error = _other.error;
		message = _other.message;
	}
	explicit Result(Result&& _other) noexcept
	{
		error = _other.error;
		message = std::move(_other.message);
	}

	const Result& operator = (const EError& _error)
	{
		error = _error;
		message.clear();
		return *this;
	}
	const Result& operator = (const Result& _other)
	{
		error = _other.error;
		message = _other.message;
		return *this;
	}
	const Result& operator = (Result&& _other) noexcept
	{
		error = _other.error;
		message = std::move(_other.message);
		return *this;
	}

	bool operator != (const Result& _other) const
	{
		return (error != _other.error);
	}
	bool operator == (const Result& _other) const
	{
		return (error == _other.error);
	}
	bool operator != (const EError& _other) const
	{
		return (error != _other);
	}
	bool operator == (const EError& _other) const
	{
		return (error == _other);
	}
	bool operator ! () const
	{
		return (error != EError::Success);
	}
};

template <>
struct std::formatter<Result, char> : std::formatter<int, char>
{
	template <class T_FORMAT_CONTEXT>
	auto format(const Result& _val, T_FORMAT_CONTEXT& _formatCtx) const
	{
		if (_val.message.empty())
		{
			return std::format_to(_formatCtx.out(), "{}", _val.error);
		}
		return std::format_to(_formatCtx.out(), "{}({})", _val.message.c_str(), _val.error);
	}
};