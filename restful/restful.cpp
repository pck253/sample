#include "pch.h"

MODULE_STATIC_IMPL(Restful);

Restful::Restful(const std::string& _configFilePath)
	: Module(_configFilePath)
{
	m_accessor = RestfulAccessorImpl::Create(this);
}

Restful::~Restful()
{
}

Result Restful::InitImpl()
{
	auto listen = m_config["listen"];
	if (!listen.is_array())
	{
		return EError::InvalidConfig;
	}

	// ------------------------------------------------------------------------
	// need handler to listen : see SetRequestHandler
	// ------------------------------------------------------------------------
	auto listens = listen.get<our::vector<nlohmann::json>>();
	for (auto& ls : listens)
	{
		auto name = ls["name"].get<std::string>();
		auto ip = ls["ip"].get<std::string>();
		auto port = ls["port"].get<uint16_t>();
		uint32_t timout = 10;
		if (ls["timeout"].is_number_unsigned())
		{
			timout = ls["timeout"].get<uint32_t>();
		}

		uri_builder builder;
		builder.set_scheme(L"http");
		builder.set_host(std::wstring(ip.begin(), ip.end()));
		builder.set_port(port);

		http_listener_config listenConfig;
		listenConfig.set_timeout(utility::seconds(timout));

		http_listener* listener = nullptr;
		try {
			listener = new http_listener(builder.to_uri(), listenConfig);
		}
		catch (const std::exception& e) {
			LogError("Rest error exception : {}", e.what());
			return EError::RestfulException;
		}

		m_listeners.emplace(name, ListenerInfo(listener));
	}

	return EError::Success;
}

void Restful::Shutdown()
{
	m_shutdown = true;

	{
		SCOPED_WRITE_LOCK(m_waitRequestMutex);
		for (auto& [requestId, request] : m_waitRequests)
		{
			request.reply(status_codes::OK, L"processing shutdown");
		}
		m_waitRequests.clear();
	}

	for (auto& [name, listenerInfo] : m_listeners)
	{
		if (listenerInfo.nowUsing)
		{
			listenerInfo.listener->close()
				.then([_name = name]() { Log("Closed Restful - {}", _name); })
				.wait();
		}

		SAFE_DELETE(listenerInfo.listener);
	}
	m_listeners.clear();
}

Result Restful::SetRequestHandler(const std::string& _listenerName, RequestHandler_t&& _handler)
{
	auto found = m_listeners.find(_listenerName);
	if (m_listeners.end() == found)
	{
		return EError::NotExistRestfulListener;
	}

	bool expect = false;
	if (!found->second.nowUsing.compare_exchange_strong(expect, true))
	{
		return EError::AlreadyUsingRestfulListener;
	}
	
	found->second.listener->support(methods::GET,
		[this, _name = found->first, _handler = std::move(_handler)](http_request _req) {
			auto path = _req.request_uri().path();
			auto query = web::uri::decode(_req.request_uri().query());

			auto requestId = ++m_RequestIdSequence;
			if (!m_shutdown)
			{
				SCOPED_WRITE_LOCK(m_waitRequestMutex);
				m_waitRequests.emplace(requestId, _req);
			}
			else
			{
				_req.reply(status_codes::OK, L"invalid request");
				return;
			}
			_handler(requestId, path, query, static_cast<RestfulAccessor*>(GetAccessor()));
		});

	try {
		found->second.listener->open()
			.then([_name = found->first]() { Log("Started Restful - {}", _name); })
			.wait();
	}
	catch (const std::exception& e) {
		LogError("Rest error exception : {}", e.what());
		return EError::RestfulException;
	}

	return EError::Success;
}

void Restful::Response(const RestfulRequestId_t& _requestId, nlohmann::json&& _reply)
{
	http_request req;
	{
		SCOPED_WRITE_LOCK(m_waitRequestMutex);
		auto found = m_waitRequests.find(_requestId);
		if (found == m_waitRequests.end())
		{
			return;
		}
		req = found->second;
		m_waitRequests.erase(found);
	}
	
	utility::string_t jsonString;
	if (StringUtility::Utf8ToUnicode(_reply.dump(), jsonString))
	{
		json::value jsonReply(jsonString);
		req.reply(status_codes::OK, jsonReply);
	}
	else
	{
		req.reply(status_codes::OK, "failed to make reply.");
	}
}