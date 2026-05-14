#pragma once

using RestufulRequestId_t = StrongId<uint64_t, 0ui64>;

class WebAccessor;
using RestfulHandler_t = bool(*)(const RestufulRequestId_t&, const std::wstring&, const std::wstring&, WebAccessor*);

struct WebParams
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
					m_result = EError::FailedParseWeb;
					break;
				}
				if (key.empty())
				{
					if (StringUtility::UnicodeToUtf8(split, key) <= 0)
					{
						m_result = EError::FailedParseWeb;
						break;
					}
				}
				else
				{
					if (StringUtility::UnicodeToUtf8(split, value) <= 0)
					{
						m_result = EError::FailedParseWeb;
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
				m_result = EError::FailedParseWeb;
				return;
			}
			m_params.emplace(key, value);
			startIndex = _query.size();
		}

		if (startIndex != _query.size())
		{
			m_result = EError::FailedParseWeb;
		}
    }

	std::unordered_map<std::string, std::string> m_params;
	Result m_result;
};

class WebAccessor : public ModuleAccessor
{
public:
	WebAccessor() = default;
	virtual ~WebAccessor() = default;

	virtual Result SetRestfulHandler(const std::string& _listenerName, const RestfulHandler_t _handler) = 0;
	virtual void Response(const RestufulRequestId_t& _requestId, nlohmann::json&& _reply) = 0;
};

struct WebHelper
{
	Result SettingByConfig(const nlohmann::json& _config, Application* _application, ModuleAccessor*& _webAccessor, const RestfulHandler_t _requestHandler)
	{
		auto webConfig = _config["restful config"];
		if (!webConfig.is_null())
		{
			auto webModule = _application->GetModule(EModule::Web);
			if (!webModule)
			{
				return EError::NotExistModule;
			}
			_webAccessor = webModule->GetAccessor();

			auto useListenersConfig = webConfig["target restful listeners"];
			if (useListenersConfig.is_array())
			{
				auto useListeners = useListenersConfig.get<std::vector<nlohmann::json>>();
				for (auto& listenerName : useListeners)
				{
					auto ret = _webAccessor->As<WebAccessor>().SetRestfulHandler(listenerName, _requestHandler);
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