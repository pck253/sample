#pragma once

enum class ELogLevel : uint8_t
{
	Normal = 1,
	Error = 2,
	Warning = 3,
	Debug = 4,
	Max
};

enum class ELogCategory : uint16_t
{
	Network = 1,
	Redis,
	MsSql,
	Nav,
	Max
};

using LogHandler_t = std::function<void(const char*, const ELogLevel&, const char*)>;

class Logger
{
public:
	Logger() = default;
	virtual ~Logger() = default;

	inline void SetPrefix(const std::string& _prefix) { m_prefix = _prefix; }

	inline void SetLogHandler(LogHandler_t&& _logHandler) { m_logHandler = std::move(_logHandler); }
	inline const LogHandler_t& GetLogHandler() { return m_logHandler; }

	void SetConfiguration(const ELogLevel& _logLevel, std::unordered_set<ELogCategory>&& _useDebugLogCategory)
	{
		m_logLevel = _logLevel;
		m_useDebugLogCategory = std::move(_useDebugLogCategory);
	}

	void Write(const ELogLevel& _logLevel, const char* _log)
	{
		if (m_logLevel < _logLevel)
		{
			return;
		}
		m_logHandler(m_prefix.c_str(), _logLevel, _log);
	}

	inline bool IsUsingDebugLogCategory(const ELogCategory& _debugCategory)
	{
		return m_useDebugLogCategory.contains(_debugCategory);
	}

private:

	std::string m_prefix;
	LogHandler_t m_logHandler;

	ELogLevel m_logLevel = ELogLevel::Debug;
	std::unordered_set<ELogCategory> m_useDebugLogCategory;
};

inline Logger g_logger;

template <typename... Args>
inline void LogImpl(const ELogLevel _logLevel, const char* _function, const int _line, std::format_string<Args...> _fmt, Args&&... _args)
{
	g_logger.Write(_logLevel, std::format("[{}:{}] {}", _function, _line, std::format(_fmt, std::forward<Args>(_args)...)).c_str());
}

template <typename... Args>
inline void LogDebugImpl(const ELogCategory _category, const char* _function, const int _line, std::format_string<Args...> _fmt, Args&&... _args)
{
	if (g_logger.IsUsingDebugLogCategory(_category))
	{
		LogImpl(ELogLevel::Debug, _function, _line, _fmt, std::forward<Args>(_args)...);
	}
}

// macros, not functions: __FUNCTION__ / __LINE__ have to be expanded at the call site.
#define Log(...)					LogImpl(ELogLevel::Normal, __FUNCTION__, __LINE__, __VA_ARGS__)
#define LogError(...)				LogImpl(ELogLevel::Error, __FUNCTION__, __LINE__, __VA_ARGS__)
#define LogWarning(...)				LogImpl(ELogLevel::Warning, __FUNCTION__, __LINE__, __VA_ARGS__)
#define LogDebug(_category, ...)	LogDebugImpl(_category, __FUNCTION__, __LINE__, __VA_ARGS__)