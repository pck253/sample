#pragma once

enum class EModule : uint8_t
{
	None = 0,
	Network = 1,
	Restful = 2,
	Timer = 3,
	Server = 4,
	Max
};

template <>
struct std::formatter<EModule, char> : std::_Formatter_base<EModule, char, std::_Basic_format_arg_type::_Custom_type>
{
	template <class _FormatContext>
	auto format(const EModule& _Val, _FormatContext& _FormatCtx) const {
		return std::format_to(_FormatCtx.out(), "{}", std::underlying_type_t<EModule>(_Val));
	}
};

class ModuleAccessor
{
public:
	virtual ~ModuleAccessor() {};

	template<class T> requires std::derived_from<T, ModuleAccessor>
	T& Get()
	{
		return static_cast<T&>(*this);
	}

protected:
	// -------------------------------------------------------------------
	// accessor must be created in module dll
	// -------------------------------------------------------------------
	ModuleAccessor() {};
};

// ----------------------------------------------------------------------------------

class Module;
using CreateModuleFunc_t = Module * (*)(const char*, MemoryPool*);

class Application;
class Module
{
public:
	virtual ~Module() = default;

	template<class T> requires std::derived_from<T, Module>
	static Module* Create(const std::string& _configFilePath)
	{
		// -------------------------------------------------------------------
		// module is singleton
		// -------------------------------------------------------------------
		std::call_once(m_onceFlag, [_configFilePath]() {
				m_self = new T(_configFilePath);
			});

		return m_self;
	}

	virtual bool IsBusinessModule() = 0;
	virtual EModule GetModuleType() = 0;
	virtual const char* GetModuleName() = 0;

	template<class T = Module> requires std::derived_from<T, Module>
	static T* Get()
	{
		return static_cast<T*>(m_self);
	}

	virtual Result Init()
	{
		m_refLogger->SetPrefix(GetModuleName());

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
			std::unordered_set<EDebugLogCategory> useDebugLogCategory;

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

			m_refLogger->SetConfiguration(logLevel, std::move(useDebugLogCategory));
		}

		return InitImpl();
	}

	virtual void Shutdown() = 0;

	void SetApplication(Application* _application) noexcept { m_application = _application; }
	ModuleAccessor* GetAccessor() { return m_accessor; }

	inline void SetLogHandler(LogHandler_t&& _logHandler) { m_refLogger->SetLogHandler(std::move(_logHandler)); }

protected:
	Module(const std::string& _configFilePath) : m_configFilePath(_configFilePath)
	{
		m_refLogger = &g_logger;
	};

	virtual Result InitImpl() = 0;
	Application* GetApplication()
	{
		return m_application;
	}

protected:
	static Module* m_self;
	ModuleAccessor* m_accessor = nullptr;

	nlohmann::json m_config;
	std::string m_configFilePath;

	Application* m_application = nullptr;

	Logger* m_refLogger = nullptr;

private:
	static std::once_flag m_onceFlag;
};

#ifdef _USRDLL
#include "../module/module_static_impl.h"
#endif