#pragma once

enum class ELogLevel : uint8_t
{
	Normal = 1,
	Error = 2,
	Warning = 3,
	Debug = 4,
	Max
};

enum class EDebugLogCategory : uint16_t
{
	Network = 1,
	Redis = 1,
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
	inline LogHandler_t& GetLogHandler() { return m_logHandler; }

	void SetConfiguration(const ELogLevel& _logLevel, std::unordered_set<EDebugLogCategory>&& _useDebugLogCategory)
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

	inline bool IsUsingDebugLogCategory(const EDebugLogCategory& _debugCategory)
	{
		return (m_useDebugLogCategory.find(_debugCategory) != m_useDebugLogCategory.end());
	}

private:

	std::string m_prefix;
	LogHandler_t m_logHandler;

	ELogLevel m_logLevel = ELogLevel::Debug;
	std::unordered_set<EDebugLogCategory> m_useDebugLogCategory;
};

extern Logger g_logger;

#define Log(f, ...)				g_logger.Write(ELogLevel::Normal, std::format(f, __VA_ARGS__).c_str());
#define LogError(f, ...)		g_logger.Write(ELogLevel::Error, std::format(f, __VA_ARGS__).c_str());
#define LogWarning(f, ...)		g_logger.Write(ELogLevel::Warning, std::format(f, __VA_ARGS__).c_str());
#define LogDebug(cate, f, ...)	if (g_logger.IsUsingDebugLogCategory(cate)) g_logger.Write(ELogLevel::Debug, std::format(f, __VA_ARGS__).c_str());