#pragma once

using AcceptedHandler_t = std::function<bool(const Result&, ConnectionShared_t)>;
using ConnectedHandler_t = std::function<bool(const Result&, const std::string&, ConnectionShared_t)>;

class NetworkAccessor : public ModuleAccessor
{
public:
	virtual ~NetworkAccessor() = default;

	virtual Result RegistAcceptedHandler(const std::string& _listenerName, AcceptedHandler_t&& _handler) = 0;
	virtual Result RequestConnect(const std::string& _address, const uint16_t& _port, ConnectedHandler_t&& _handler) = 0;
	virtual Result RequestConnect(const std::string& _connecterName, ConnectedHandler_t&& _handler, const uint16_t& _tryReconnectCount) = 0;

	// this function for business module.
	virtual void StopPublicListen(const std::string& _listenerName) = 0;
	virtual void ClosePublicConnection(const std::string& _listenerName) = 0;

protected:
	NetworkAccessor() = default;
};

struct NetworkHelper
{
	Result SettingByConfig(const nlohmann::json& _config, Application* _application, ModuleAccessor*& _networkAccessor,
		AcceptedHandler_t&& _acceptedHandler, ConnectedHandler_t&& _connectedHandler)
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
		_networkAccessor = networkModule->GetAccessor();

		if (_acceptedHandler)
		{
			auto useListenersConfig = networkConfig["use listeners"];
			if (!useListenersConfig.is_array())
			{
				return EError::InvalidConfig;
			}

			auto useListeners = useListenersConfig.get<our::vector<nlohmann::json>>();
			for (auto& listenerName : useListeners)
			{
				AcceptedHandler_t copied = _acceptedHandler;
				auto ret = _networkAccessor->Get<NetworkAccessor>().RegistAcceptedHandler(listenerName.get<std::string>(), std::move(copied));
				if (ret != EError::Success)
				{
					return ret;
				}
			}
		}

		if (_connectedHandler)
		{
			auto useConnectorsConfig = networkConfig["use connecters"];
			if (!useConnectorsConfig.is_array())
			{
				return EError::InvalidConfig;
			}
			
			auto useConnecters = useConnectorsConfig.get<our::vector<nlohmann::json>>();
			for (auto& useConnecter : useConnecters)
			{
				auto name = useConnecter["name"].get<std::string>();
				auto reconnectCount = useConnecter["initial reconnect count"].get<uint16_t>();

				ConnectedHandler_t copied = _connectedHandler;
				auto ret = _networkAccessor->Get<NetworkAccessor>().RequestConnect(name, std::move(copied), reconnectCount);
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

		auto useListeners = useListenersConfig.get<our::vector<nlohmann::json>>();
		for (auto& listenerName : useListeners)
		{
			static_cast<NetworkAccessor*>(networkAccessor)->StopPublicListen(listenerName.get<std::string>());
			static_cast<NetworkAccessor*>(networkAccessor)->ClosePublicConnection(listenerName.get<std::string>());
		}
	}
};