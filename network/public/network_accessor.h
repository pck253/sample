#pragma once

using AcceptedHandler_t = bool(*)(const Result&, ConnectionShared_t);
struct AcceptedConfig
{
	std::atomic_bool isSetted = false;
	AcceptedHandler_t acceptedHandler = nullptr;
	ReceivedHandler_t receivedHandler = nullptr;
	ClosedHandler_t closedHandler = nullptr;

	bool IsValid() const
	{
		return (acceptedHandler && receivedHandler && closedHandler);
	}

	bool Set(const AcceptedHandler_t _acceptedHandler, const ReceivedHandler_t _receivedHandler, const ClosedHandler_t _closedHandler)
	{
		bool expect = false;
		if (not isSetted.compare_exchange_strong(expect, true))
		{
			return false;
		}
		acceptedHandler = _acceptedHandler;
		receivedHandler = _receivedHandler;
		closedHandler = _closedHandler;
		return true;
	}
};

using ConnectedHandler_t = bool(*)(const Result&, const std::string&, ConnectionShared_t);
struct ConnectedConfig
{
	ConnectedHandler_t connectedHandler = nullptr;
	ReceivedHandler_t receivedHandler = nullptr;
	ClosedHandler_t closedHandler = nullptr;

	bool IsValid() const
	{
		return (connectedHandler && receivedHandler && closedHandler);
	}
};

class NetworkAccessor : public ModuleAccessor
{
public:
	virtual ~NetworkAccessor() = default;

	virtual Result RegistAcceptedConfig(const std::string& _listenerName, const AcceptedConfig& _acceptedConfig) = 0;
	virtual Result RequestConnect(const std::string& _address, const uint16_t& _port, const ConnectedConfig& _connectedConfig) = 0;
	virtual Result RequestConnect(const std::string& _connecterName, const ConnectedConfig& _connectedConfig, const uint16_t& _tryReconnectCount) = 0;

	// this function for business module.
	virtual void StopPublicListen(const std::string& _listenerName) = 0;
	virtual void ClosePublicConnection(const std::string& _listenerName) = 0;

protected:
	NetworkAccessor() = default;
};

struct NetworkHelper
{
	Result SettingByConfig(const nlohmann::json& _config, Application* _application, const AcceptedConfig& _acceptedConfig, const ConnectedConfig& _connectedConfig)
	{
		auto networkConfig = _config["network config"];
		if (networkConfig.is_null())
		{
			return EError::InvalidConfig;
		}

		auto networkModule = _application->GetModule(EModule::Network);
		if (!networkModule)
		{
			return EError::NotExistModule;
		}
		
		ModuleAccessor* networkAccessor = networkModule->GetAccessor();
		if (!networkAccessor)
		{
			return EError::NotExistModuleAccessor;
		}

		if (_acceptedConfig.IsValid())
		{
			auto useListenersConfig = networkConfig["use listeners"];
			if (!useListenersConfig.is_array())
			{
				return EError::InvalidConfig;
			}

			auto useListeners = useListenersConfig.get<std::vector<nlohmann::json>>();
			for (auto& listenerName : useListeners)
			{
				auto ret = networkAccessor->Get<NetworkAccessor>().RegistAcceptedConfig(listenerName.get<std::string>(), _acceptedConfig);
				if (ret != EError::Success)
				{
					return ret;
				}
			}
		}

		if (_connectedConfig.IsValid())
		{
			auto useConnectorsConfig = networkConfig["use connecters"];
			if (!useConnectorsConfig.is_array())
			{
				return EError::InvalidConfig;
			}
			
			auto useConnecters = useConnectorsConfig.get<std::vector<nlohmann::json>>();
			for (auto& useConnecter : useConnecters)
			{
				auto name = useConnecter["name"].get<std::string>();
				auto reconnectCount = useConnecter["initial reconnect count"].get<uint16_t>();

				auto ret = networkAccessor->Get<NetworkAccessor>().RequestConnect(name, _connectedConfig, reconnectCount);
				if (ret != EError::Success)
				{
					return ret;
				}
			}
		}

		return EError::Success;
	}

	void ShutdownPublic(const nlohmann::json& _config, Application* _application)
	{
		auto networkConfig = _config["network config"];
		if (networkConfig.is_null())
		{
			return;
		}

		auto networkModule = _application->GetModule(EModule::Network);
		if (!networkModule)
		{
			return;
		}
		ModuleAccessor* networkAccessor = networkModule->GetAccessor();

		auto useListenersConfig = networkConfig["use listeners"];
		if (!useListenersConfig.is_array())
		{
			return;
		}

		auto useListeners = useListenersConfig.get<std::vector<nlohmann::json>>();
		for (auto& listenerName : useListeners)
		{
			static_cast<NetworkAccessor*>(networkAccessor)->StopPublicListen(listenerName.get<std::string>());
			static_cast<NetworkAccessor*>(networkAccessor)->ClosePublicConnection(listenerName.get<std::string>());
		}
	}
};