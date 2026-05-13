#pragma once

#if !defined(_USRDLL) && !defined(_LIB)
#include <minidumpapiset.h>

struct ExceptionInfo
{
	DWORD  threadId = 0;
	PEXCEPTION_POINTERS exception = nullptr;
};

unsigned int __stdcall CreateMiniDump(void* _exceptionInfo)
{
	ExceptionInfo* info = static_cast<ExceptionInfo*>(_exceptionInfo);

	time_t curr;
	time(&curr);

	struct tm currTm;
	localtime_s(&currTm, &curr);

	char dumpPath[_MAX_PATH + 1] = { 0, };
	sprintf_s(dumpPath, sizeof(dumpPath),
		"minidump_%04d%02d%02d_%02d%02d.dmp",
		static_cast<int>(currTm.tm_year + 1900),
		static_cast<int>(currTm.tm_mon + 1),
		static_cast<int>(currTm.tm_mday),
		static_cast<int>(currTm.tm_hour),
		static_cast<int>(currTm.tm_min));

	HANDLE hFile = CreateFile(
		dumpPath,
		GENERIC_WRITE,
		0,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if (INVALID_HANDLE_VALUE == hFile)
	{
		return -1;
	}

	MINIDUMP_EXCEPTION_INFORMATION miniDumpInfo;
	miniDumpInfo.ThreadId = info->threadId;
	miniDumpInfo.ExceptionPointers = info->exception;
	miniDumpInfo.ClientPointers = FALSE;

	MiniDumpWriteDump(
		GetCurrentProcess(),
		GetCurrentProcessId(),
		hFile,
		MiniDumpWithFullMemory,
		info->exception ? &miniDumpInfo : NULL,
		NULL,
		NULL);

	CloseHandle(hFile);

	return 0;
}

LONG UnhandledExceptionHandler(PEXCEPTION_POINTERS _exception)
{
	ExceptionInfo info = { ::GetCurrentThreadId(), _exception };

	if (_exception != nullptr && EXCEPTION_STACK_OVERFLOW == _exception->ExceptionRecord->ExceptionCode)
	{
		auto hThread = _beginthreadex(0, 0, CreateMiniDump, &info, 0, nullptr);
		if (hThread != 0)
		{
			WaitForSingleObject((HANDLE)hThread, INFINITE);
			CloseHandle((HANDLE)hThread);
		}
		else
		{
			CreateMiniDump(&info);
		}
	}
	else
	{
		CreateMiniDump(&info);
	}

	return EXCEPTION_EXECUTE_HANDLER;
}
#endif

// -----------------------------------------------------------------------------------------------

class Application
{
	using Modules_t = std::unordered_map<EModule, Module*>;
public:
	Application()
	{
#if !defined(_USRDLL) && !defined(_LIB)
		SetUnhandledExceptionFilter(UnhandledExceptionHandler);
#endif
		g_memoryPool = &m_memoryPool;
		g_logger.SetPrefix("Application");
		g_logger.SetLogHandler([this](const char* _who, const ELogLevel& _logLevel, const char* _log)
			{
				switch (_logLevel)
				{
				case ELogLevel::Normal:		printf_s("[%s]%s\n", _who, _log); break;
				case ELogLevel::Error:		printf_s("[%s][Error]%s\n", _who, _log); break;
				case ELogLevel::Warning:	printf_s("[%s][Warning]%s\n", _who, _log); break;
				case ELogLevel::Debug:		printf_s("[%s][Debug]%s\n", _who, _log); break;
				}
			});
	}
	virtual ~Application()
	{
		ShutdownBusiness();

		ShutdownCommon();

		Modules_t temp = std::move(m_modules);
		for (auto& module : temp)
		{
			SAFE_DELETE(module.second);
		}

		for (auto& [dllHandler, isBusiness] : m_dllHandlers)
		{
			FreeLibrary(dllHandler);
		}

		Log("exit Application.");
		//g_memoryPool = nullptr;
	}

	Result Build(const std::string& _configFilePath)
	{
		std::ifstream f(_configFilePath);

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
			LogError("", _ex.what());
			return EError::InvalidConfig;
		}

		std::filesystem::path configRootPath;
		{
			const auto application = m_config["application"];
			if (application.is_null())
			{
				LogError("not exist application config data.");
				return EError::InvalidConfig;
			}

			const auto name = application["name"];
			if (name.is_string())
			{
				SetConsoleTitle(name.get<std::string>().c_str());
			}

			const auto configRoot = application["config root"];
			if (!configRoot.is_string())
			{
				LogError("not exist config root data.");
				return EError::InvalidConfig;
			}

			configRootPath = configRoot.get<std::string>();
			try
			{
				configRootPath = std::filesystem::canonical(configRootPath);
				configRootPath += "/";
			}
			catch (std::exception& _ex)
			{
				LogError("config root path : {}", _ex.what());
				return EError::InvalidConfig;
			}

			const auto resourceRoot = application["resource root"];
			if (!resourceRoot.is_string())
			{
				LogError("not exist config root data.");
				return EError::InvalidConfig;
			}

			const auto resourceRootPath = resourceRoot.get<std::string>();
			try
			{
				m_resourceRootPath = std::filesystem::canonical(resourceRootPath).generic_string() + "/";
			}
			catch (std::exception& _ex)
			{
				LogError("resource root path : {}", _ex.what());
				return EError::InvalidConfig;
			}
		}

		auto modules = m_config["modules"];
		if (!modules.is_array())
		{
			LogError("not exist module config data.");
			return EError::InvalidConfig;
		}

		const auto moduleConfigs = modules.get<std::vector<nlohmann::json>>();
		for (const auto& moduleConfig : moduleConfigs)
		{
			if (!moduleConfig["use"].get<bool>())
			{
				continue;
			}

			const std::string dll = moduleConfig["dll"].get<std::string>();

			std::filesystem::path configPath = configRootPath;
			configPath += moduleConfig["config"].get<std::string>();

			try
			{
				configPath = std::filesystem::canonical(configPath);
			}
			catch (std::exception& _ex)
			{
				LogError("module config path : {}", _ex.what());
				return EError::InvalidConfig;
			}

			auto h = LoadLibrary(dll.c_str());
			if (!h)
			{
				LogError("failed to load module dll : {}", dll.c_str());
				return EError::InvalidDll;
			}

			CreateModuleFunc_t createModuleFunc = (CreateModuleFunc_t)GetProcAddress(h, "CreateModule");
			if (!createModuleFunc)
			{
				FreeLibrary(h);
				LogError("CreateModule function is null : {}", dll.c_str());
				return EError::FailedCreateModule;
			}

			auto module = (*createModuleFunc)(configPath.generic_string().c_str(), g_memoryPool);
			if (!module)
			{
				FreeLibrary(h);
				LogError("failed to create module : {}", dll.c_str());
				return EError::FailedCreateModule;
			}

			if (!AddModule(module))
			{
				FreeLibrary(h);
				SAFE_DELETE(module);
				LogError("failed to add module : {}", dll.c_str());
				return EError::DuplicatedModule;
			}

			m_dllHandlers.push_back(std::make_pair(h, module->IsBusinessModule()));
		}

		if (m_modules.empty())
		{
			LogError("module empty : {}");
			return EError::EmptyModule;
		}

		return EError::Success;
	}

	Result InitBusiness()
	{
		Result result = EError::Success;
		for (auto& module : m_modules)
		{
			if (!module.second->IsBusinessModule())
			{
				continue;
			}

			result = module.second->Init();
			if (!result)
			{
				LogError("failed to init business module : {}, {}", Module::GetModuleName(module.second->GetModuleType()), result);
				return result;
			}
		}

		return result;
	}

	Result InitCommon()
	{
		Result result = EError::Success;
		for (auto& module : m_modules)
		{
			if (module.second->IsBusinessModule())
			{
				continue;
			}

			result = module.second->Init();
			if (!result)
			{
				LogError("failed to init common module : {}, {}", Module::GetModuleName(module.second->GetModuleType()), result);
				return result;
			}
		}

		return result;
	}

	Module* GetModule(const EModule& _moduleType)
	{
		auto found = m_modules.find(_moduleType);
		return (found == m_modules.end()) ? nullptr : found->second;
	}


	const auto& GetResourceRootPath() const { return m_resourceRootPath; }

	void Shutdown() { m_shutdown = true; }

	bool IsShutdown() { return m_shutdown; }

protected:
	bool AddModule(Module* _module)
	{
		auto ret = m_modules.emplace(_module->GetModuleType(), _module);
		if (ret.second)
		{
			_module->SetApplication(this);
			LogHandler_t copied = g_logger.GetLogHandler();
			_module->SetLogHandler(std::move(copied));
		}
		return ret.second;
	}

	virtual void ShutdownBusiness()
	{
		std::vector<std::thread> threads;
		for (auto& [moduleType, module] : m_modules)
		{
			if (!module || !module->IsBusinessModule())
			{
				continue;
			}
			threads.emplace_back([_module = module]()
				{
					_module->Shutdown();
				});
		}

		for (auto& thread : threads)
		{
			thread.join();
		}
	}

	virtual void ShutdownCommon()
	{
		std::vector<std::thread> threads;
		for (auto& [moduleType, module] : m_modules)
		{
			if (!module || module->IsBusinessModule())
			{
				continue;
			}
			threads.emplace_back([_module = module]()
				{
					_module->Shutdown();
				});
		}

		for (auto& thread : threads)
		{
			thread.join();
		}
	}

protected:
	MemoryPool m_memoryPool;

	std::atomic_bool m_shutdown = false;

	nlohmann::json m_config;
	std::string m_resourceRootPath;

	Modules_t m_modules;
	std::vector<std::pair<HINSTANCE, bool>> m_dllHandlers;
};