#include "pch.h"

static_assert(NETWORK_MODULE == 1);

MODULE_STATIC_IMPL(Network);

Network::Network(const std::string& _configFilePath)
	: Module(_configFilePath),
	m_listener(this),
	m_connecter(this),
	m_imnManager(this)
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

		std::vector<std::tuple<std::string, asio::ip::tcp::endpoint, bool>> addresses;
		auto listen = listener["listen"];
		if (listen.is_array())
		{
			auto listens = listen.get<std::vector<nlohmann::json>>();
			for (auto& ls : listens)
			{
				auto name = ls["name"].get<std::string>();
				auto isImn = ls["is imn"];
				if (!isImn.is_null() && isImn.get<bool>())
				{
					m_redirectToImnInfos.emplace(name);
				}
				else
				{
					auto ip = ls["ip"].get<std::string>();
					auto port = ls["port"].get<uint16_t>();
					auto isPublic = ls["public"].get<bool>();

					addresses.push_back(std::make_tuple(name, asio::ip::tcp::endpoint(asio::ip::make_address(ip), port), isPublic));
				}
			}
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
			threadCount = std::thread::hardware_concurrency();
		}

		auto connect = connecter["connect"];
		if (!connect.is_array())
		{
			return EError::NotExistConnectInfo;
		}

		std::unordered_map<std::string, asio::ip::tcp::endpoint> addresses;
		auto connects = connect.get<std::vector<nlohmann::json>>();
		for (auto& con : connects)
		{
			auto name = con["name"].get<std::string>();
			auto foundImn = m_redirectToImnInfos.find(name);
			if (m_redirectToImnInfos.end() == foundImn)
			{
				auto ip = con["ip"].get<std::string>();
				auto port = con["port"].get<uint16_t>();

				addresses.emplace(name, asio::ip::tcp::endpoint(asio::ip::make_address(ip), port));
			}
		}
		m_connecter.Init(threadCount, std::move(addresses));
	}

	m_imnManager.Init(m_redirectToImnInfos, 3);

	return EError::Success;
}

void Network::Shutdown()
{
	m_listener.Shutdown("listener shutdown.");
	m_connecter.Shutdown("connecter shutdown.");
	m_imnManager.Shutdown("internal module network shutdown.");
}