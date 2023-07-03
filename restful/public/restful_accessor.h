#pragma once

using RestfulRequestId_t = uint64_t;
#define INVALID_RESTFUL_ID 0

class RestfulAccessor;
using RequestHandler_t = std::function<bool(const RestfulRequestId_t&, const std::wstring&, const std::wstring&, RestfulAccessor*)>;

struct RestfulParams
{
public:
    void Parse(const std::wstring& _query)
    {
		wchar_t delims[] = { L'=', L'&' };
		uint8_t delimIndex = 0;
		std::string key;
		std::string value;
		size_t startIndex = 0;
		for (size_t i = 0; i < _query.size(); ++i)
		{
			if (_query[i] == delims[delimIndex % 2])
			{
				std::wstring split = _query.substr(startIndex, i - startIndex);
				if (split.empty())
				{
					m_result = EError::FailedParseRestful;
					break;
				}
				if (key.empty())
				{
					if (StringUtility::UnicodeToUtf8(split, key) <= 0)
					{
						m_result = EError::FailedParseRestful;
						break;
					}
				}
				else
				{
					if (StringUtility::UnicodeToUtf8(split, value) <= 0)
					{
						m_result = EError::FailedParseRestful;
						break;
					}
					m_params.emplace(key, value);
					key.clear();
					value.clear();
				}
				startIndex = i + 1;
				++delimIndex;
			}
		}

		if (!key.empty() && value.empty())
		{
			std::wstring split = _query.substr(startIndex);
			if (split.empty() || StringUtility::UnicodeToUtf8(split, value) <= 0)
			{
				m_result = EError::FailedParseRestful;
				return;
			}
			m_params.emplace(key, value);
			startIndex = _query.size();
		}

		if (startIndex != _query.size())
		{
			m_result = EError::FailedParseRestful;
		}
    }

	our::unordered_map<std::string, std::string> m_params;
	Result m_result;
};

class RestfulAccessor : public ModuleAccessor
{
public:
	RestfulAccessor() = default;
	virtual ~RestfulAccessor() = default;

	virtual Result SetRequestHandler(const std::string& _listenerName, RequestHandler_t&& _handler) = 0;
	virtual void Response(const RestfulRequestId_t& _requestId,	nlohmann::json&& _reply) = 0;
};

struct RestfulHelper
{
	// ---------------------------------------------------------------------------
	// _requestHandler : never change after setted. so dose not use lamda capture that will effect to shutdown or finalize
	// ---------------------------------------------------------------------------
	Result SettingByConfig(const nlohmann::json& _config, Application* _application, ModuleAccessor*& _restfulAccessor, RequestHandler_t&& _requestHandler)
	{
		auto restfulConfig = _config["restful config"];
		if (!restfulConfig.is_null())
		{
			auto restfulModule = _application->GetModule(EModule::Restful);
			if (!restfulModule)
			{
				return EError::NotExistModule;
			}
			_restfulAccessor = restfulModule->GetAccessor();

			auto useListenersConfig = restfulConfig["target restful listeners"];
			if (useListenersConfig.is_array())
			{
				auto useListeners = useListenersConfig.get<our::vector<nlohmann::json>>();
				for (auto& listenerName : useListeners)
				{
					auto copied = _requestHandler;
					auto ret = _restfulAccessor->Get<RestfulAccessor>().SetRequestHandler(listenerName, std::move(copied));
					if (ret != EError::Success)
					{
						return ret;
					}
				}
			}
		}

		return EError::Success;
	}
};