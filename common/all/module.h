#pragma once

enum class EModule : uint8_t
{
	None = 0,
	TestClient,
	Server,
	Network,
	Web,
	Timer,
	Max
};

template <>
struct std::formatter<EModule, char> : std::formatter<int, char>
{
	template <class T_FORMAT_CONTEXT>
	auto format(const EModule& _val, T_FORMAT_CONTEXT& _formatCtx) const
	{
		return std::format_to(_formatCtx.out(), "{}", std::underlying_type_t<EModule>(_val));
	}
};

class ModuleAccessor
{
public:
	virtual ~ModuleAccessor() {};

	template<class T> requires std::derived_from<T, ModuleAccessor>
	T& As()
	{
		return dynamic_cast<T&>(*this);
	}

protected:
	// -------------------------------------------------------------------
	// accessor must be created in module dll
	// -------------------------------------------------------------------
	ModuleAccessor() {};
};

// ----------------------------------------------------------------------------------

class Application;
class Module;

using CreateModuleFunc_t = Module* (*)(Application&, const char*, MemoryPool*);

class Module
{
public:
	static const char* GetModuleName(const EModule& _module)
	{
		switch (_module)
		{
		case EModule::TestClient:	return "TestClient";
		case EModule::Server:	return "Server";
		case EModule::Network:	return "Network";
		case EModule::Web:		return "Web";
		case EModule::Timer:	return "Timer";
		default:				return "Unknown";
		}
	}

public:
	virtual ~Module()
	{
		SafeDelete(m_accessor);
	}

	Module(Application& _application, const std::string& _configFilePath)
		: m_configFilePath(_configFilePath), m_application(_application), m_refLogger(g_logger)
	{
		// -------------------------------------------------------------------
		// module is singleton
		// -------------------------------------------------------------------
		bool expect = false;
		if (!m_created.compare_exchange_strong(expect, true))
		{
			// forcly kill
			int64_t* kill = 0;
			*kill = 1;
		}

		m_self = this;
	};

	virtual bool IsBusinessModule() = 0;
	virtual EModule GetModuleType() = 0;

	template<class T = Module> requires std::derived_from<T, Module>
	static T& As()
	{
		assert(nullptr != m_self);
		return dynamic_cast<T&>(*m_self);
	}

	virtual Result Init()
	{
		m_refLogger.SetPrefix(GetModuleName(GetModuleType()));

		std::ifstream f(m_configFilePath);

		FinalJob job([&f]()
			{
				if (f.is_open())
				{
					f.close();
				}
			});

		try
		{
			m_config = nlohmann::json::parse(f);
		}
		catch (std::exception& _ex)
		{
			LogError("module config : {}", _ex.what());
			return EError::InvalidConfig;
		}

		auto logConfig = m_config["log"];
		if (!logConfig.is_null())
		{
			ELogLevel logLevel = ELogLevel::Error;
			std::unordered_set<ELogCategory> useDebugLogCategory;

			auto levelConfig = logConfig["level"];
			if (levelConfig.is_string())
			{
				auto value = levelConfig.get<std::string>();
				if (_stricmp(value.c_str(), "normal") == 0)
				{
					logLevel = ELogLevel::Normal;
				}
				else if (_stricmp(value.c_str(), "error") == 0)
				{
					logLevel = ELogLevel::Error;
				}
				else if (_stricmp(value.c_str(), "warning") == 0)
				{
					logLevel = ELogLevel::Warning;
				}
				else if (_stricmp(value.c_str(), "debug") == 0)
				{
					logLevel = ELogLevel::Debug;
				}
			}
			auto debugCategory = logConfig["debug categories"];

			m_refLogger.SetConfiguration(logLevel, std::move(useDebugLogCategory));
		}

		return InitImpl();
	}

	ModuleAccessor* GetAccessor() { return m_accessor; }

	inline void SetLogHandler(LogHandler_t&& _logHandler) { m_refLogger.SetLogHandler(std::move(_logHandler)); }

protected:
	// only Application can shutdown a module.
	// it must not be called on a thread owned by the module. (self join)
	friend class Application;
	virtual void Shutdown() = 0;

	virtual Result InitImpl() = 0;
	Application& GetApplication()
	{
		return m_application;
	}

protected:
	// Shutdown() registers the steps of everything it owns, then runs them here.
	ShutdownCoordinator m_shutdownCoordinator;

	static inline std::atomic_bool m_created = false;
	static inline Module* m_self = nullptr;

	ModuleAccessor* m_accessor = nullptr;

	nlohmann::json m_config;
	std::string m_configFilePath;

	Application& m_application;

	Logger& m_refLogger;
};

#ifdef _USRDLL
#include "../module_impl/module_static_impl.h"
#endif