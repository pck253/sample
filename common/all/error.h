#pragma once

enum class EError : uint32_t
{
	Success = 0,
	LackReceiveBuffer,
	InvalidParam,
	NotExistNetworkListener,
	NeedAcceptedHandler,
	AlreadySettedAcceptHandler,
	FailedNetworkAccept,
	OverMaxPacketSize,
	FailedSend,
	InvalidSocket,
	ClosedSocket,
	FailedReceive,
	FailedNetworkConnect,
	NotExistConnectInfo,
	NeedConnectedHandler,
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
	RestfulException,
	NotExistRestfulListener,
	AlreadyUsingRestfulListener,
	FailedAllocMssqlEnv,
	FailedSetMssqlEnvAttr,
	FailedAllocMssqlDbc,
	FailedSetMssqlConnAttr,
	FailedConnectMssql,
	FailedAllocMssqlStmt,
	FailedBindParameter,
	FailedParseRestful,
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
	UnknownCommand,
	InvalidCommandParams,
	ActorIsNull,
	Shutdown
};

template <>
struct std::formatter<EError, char> : std::_Formatter_base<EError, char, std::_Basic_format_arg_type::_Custom_type>
{
	template <class _FormatContext>
	auto format(const EError& _Val, _FormatContext& _FormatCtx) const {
		return std::format_to(_FormatCtx.out(), "{}", std::underlying_type_t<EError>(_Val));
	}
};

// -----------------------------------------------------------------------

struct Result
{
	EError error = EError::Success;
	std::string message;

	Result()
	{
	}
	Result(const EError& _error)
	{
		error = _error;
	}
	Result(const EError& _error, const std::string& _message)
	{
		error = _error;
		message = _message;
	}
	Result(const Result& _other)
	{
		error = _other.error;
		message = _other.message;
	}
	Result(Result&& _other)
	{
		error = _other.error;
		message = std::move(_other.message);
	}

	Result& operator = (const Result& _other)
	{
		error = _other.error;
		message = _other.message;
		return *this;
	}
	Result& operator = (Result&& _other)
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
};

template <>
struct std::formatter<Result, char> : std::_Formatter_base<EError, char, std::_Basic_format_arg_type::_Custom_type>
{
	template <class _FormatContext>
	auto format(const Result& _Val, _FormatContext& _FormatCtx) const {
		if (_Val.message.empty())
		{
			return std::format_to(_FormatCtx.out(), "{}", _Val.error);
		}
		return std::format_to(_FormatCtx.out(), "{}({})", _Val.message.c_str(), _Val.error);
	}
};