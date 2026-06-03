#include "pch.h"

static_assert(WEB_MODULE == 1);

Restful::Restful(Web& _webModule)
	: m_webModule(_webModule)
{
	m_afterShutdown = [this]()
		{
			{
				SCOPED_WRITE_LOCK(m_restfulRequestMutex);
				for (auto& [requestId, request] : m_waitRestfulRequests)
				{
					request.reply(status_codes::OK, L"processing shutdown");
				}
				m_waitRestfulRequests.clear();
			}

			for (auto& [name, listenerInfo] : m_restfulListeners)
			{
				if (listenerInfo.nowUsing)
				{
					listenerInfo.nowUsing = false;
					listenerInfo.listener->close()
						.then([_name = name]() { Log("Closed Restful - {}", _name); })
						.wait();
				}

				SAFE_DELETE(listenerInfo.listener);
			}
		};
}

Restful::~Restful()
{
}

Result Restful::Init(const nlohmann::json& _config)
{
	auto listen = _config["restful listen"];
	if (!listen.is_array())
	{
		return EError::InvalidConfig;
	}

	// ------------------------------------------------------------------------
	// need restful handler to listen : see SetRestfulHandler
	// ------------------------------------------------------------------------
	auto listens = listen.get<std::vector<nlohmann::json>>();
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
			LogError("Restful error exception : {}", e.what());
			return EError::RestfulException;
		}

		m_restfulListeners.emplace(name, RestfulListenerInfo(listener));
	}

	return Result();
}

Result Restful::SetRestfulHandler(const std::string& _listenerName, const RestfulHandler_t _handler)
{
	const auto found = m_restfulListeners.find(_listenerName);
	if (m_restfulListeners.end() == found)
	{
		return EError::NotExistRestfulListener;
	}

	bool expect = false;
	if (!found->second.nowUsing.compare_exchange_strong(expect, true))
	{
		return EError::AlreadyUsingRestfulListener;
	}

	found->second.listener->support(methods::GET,
		[this, &listenerRef = found->second, _handler](http_request _req)
		{
			if (IsShutdown())
			{
				_req.reply(status_codes::NotFound, "shutdown.");
				return;
			}

			auto path = _req.request_uri().path();
			auto query = web::uri::decode(_req.request_uri().query());

			if (not listenerRef.nowUsing.load())
			{
				_req.reply(status_codes::NotFound, "not work.");
				return;
			}

			auto requestId = ++m_restfulReqIdSequence;

			{
				SCOPED_WRITE_LOCK(m_restfulRequestMutex);
				m_waitRestfulRequests.emplace(requestId, _req);
			}

			(*_handler)(requestId, path, query, static_cast<WebAccessor*>(m_webModule.GetAccessor()));
		});

	found->second.listener->support(methods::OPTIONS,
		[this](http_request _req)
		{
			if (IsShutdown())
			{
				http_response response(status_codes::ServiceUnavailable);
				_req.reply(response);
				return;
			}
			http_response response(status_codes::OK);

			response.headers().add(U("Access-Control-Allow-Origin"), U("*")); // allow all domain
			response.headers().add(U("Access-Control-Allow-Methods"), U("POST, GET, OPTIONS"));
			response.headers().add(U("Access-Control-Allow-Headers"), U("Content-Type"));

			_req.reply(response);
		});
	//found->second.listener->support(methods::POST,
	//	[this, &listenerRef = found->second, _handler](http_request _req)
	// {
	//		auto path = _req.request_uri().path();
	//		auto query = web::uri::decode(_req.request_uri().query());

	//		auto jsonTask = _req.extract_json();

	//		try {
	//			auto jsonValue = jsonTask.get();
	//			//jsonValue.
	//		}
	//		catch (const std::exception& e) {
	//			LogError("Rest error exception : {}", e.what());
	//			_req.reply(status_codes::OK, L"invalid request");
	//			return;
	//		}

	//		if (not listenerRef.nowUsing.load())
	//		{
	//			_req.reply(status_codes::NotFound, "not work.");
	//			return;
	//		}

	//		auto requestId = ++m_restfulReqIdSequence;

	//		{
	//			SCOPED_WRITE_LOCK(m_restfulRequestMutex);
	//			m_waitRestfulRequests.emplace(requestId, _req);
	//		}

	//		(*_handler)(requestId, path, query, static_cast<WebAccessor*>(m_webModule.GetAccessor()));
	//	});

	std::string uriString;
	StringUtility::UnicodeToUtf8(found->second.listener->uri().to_string(), uriString);
	try {
		found->second.listener->open()
			.then([_name = found->first, &uriString]() { Log("Started Restful - {}({})", _name, uriString.c_str()); })
			.wait();
	}
	catch (const std::exception& e) {
		LogError("Rest error exception : {}", e.what());
		return EError::RestfulException;
	}

	return EError::Success;
}

void Restful::Response(const RestufulRequestId_t& _requestId, nlohmann::json&& _reply)
{
	http_request req;
	{
		SCOPED_WRITE_LOCK(m_restfulRequestMutex);
		auto found = m_waitRestfulRequests.find(_requestId);
		if (found == m_waitRestfulRequests.end())
		{
			return;
		}
		req = found->second;
		m_waitRestfulRequests.erase(found);
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