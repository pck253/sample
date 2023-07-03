#include "pch.h"

MODULE_STATIC_IMPL(Network);

Network::Network(const std::string& _configFilePath)
	: Module(_configFilePath),
	m_listener(this),
	m_connecter(this)
{
	m_accessor = NetworkAccessorImpl::Create(this);
}

Network::~Network()
{
}

Result Network::InitImpl()
{
	auto listener = m_config["listener"];
	if (!listener.is_null())
	{
		uint16_t threadCount = 0;
		auto thread = listener["thread"];
		if (thread.is_number_integer())
		{
			threadCount = thread.get<uint16_t>();
		}

		if (threadCount == 0)
		{
			threadCount = std::thread::hardware_concurrency();
		}

		our::vector<std::tuple<std::string, ip::tcp::endpoint, bool>> addresses;
		auto listen = listener["listen"];
		if (listen.is_array())
		{
			auto listens = listen.get<our::vector<nlohmann::json>>();
			for (auto& ls : listens)
			{
				auto name = ls["name"].get<std::string>();
				auto ip = ls["ip"].get<std::string>();
				auto port = ls["port"].get<uint16_t>();
				auto isPublic = ls["public"].get<bool>();

				ip::tcp::endpoint endpoint(ip::address::from_string(ip), port);
				addresses.push_back(std::make_tuple(name, endpoint, isPublic));
			}
		}

		if (threadCount == 0 || addresses.empty())
		{
			return EError::InvalidConfig;
		}

		m_listener.Init(addresses, threadCount);
	}

	auto connecter = m_config["connecter"];
	if (!connecter.is_null())
	{
		uint16_t threadCount = 0;
		auto thread = connecter["thread"];
		if (thread.is_number_integer())
		{
			threadCount = thread.get<uint16_t>();
		}

		if (threadCount == 0)
		{
			return EError::InvalidConfig;
		}
		m_connecter.Init(threadCount, connecter["connect"]);
	}

	return EError::Success;
}

void Network::Shutdown()
{
	m_listener.Shutdown();
	m_connecter.Shutdown();
}