#include "pch.h"

static_assert(WEB_MODULE == 1);

Restful::Restful(Web& _webModule)
	: m_webModule(_webModule)
{
}

Restful::~Restful()
{
}

void Restful::RegisterShutdownSteps(ShutdownCoordinator& _coordinator)
{
	_coordinator.Push("restful in flight requests",
		[this]()
		{
			return (0 < m_inFlightRequests.load()) ? EStepResult::Wait : EStepResult::Done;
		},
		[this]() { return std::format("remain={}", m_inFlightRequests.load()); });

	_coordinator.Push("restful reply waiting requests", [this]()
		{
			decltype(m_waitRestfulRequests) waitRequests;
			{
				SCOPED_WRITE_LOCK(m_restfulRequestMutex);
				waitRequests = std::move(m_waitRestfulRequests);
			}

			for (auto& [requestId, request] : waitRequests)
			{
				try {
					request.reply(status_codes::OK, L"processing shutdown");
				}
				catch (const std::exception& e) {
					LogError("Restful failed to reply on shutdown - {} : {}", requestId, e.what());
				}
			}

			return EStepResult::Done;
		});

	_coordinator.Push("restful listener close", [this]()
		{
			// check opening stage after setted shutdown state
			// SetRestfulHandler : reverse order
			auto result = EStepResult::Done;
			for (auto& [name, listenerInfo] : m_restfulListeners)
			{
				const auto state = listenerInfo.state.load();
				if (EListenerState::Opening == state)
				{
					result = EStepResult::Wait;
					continue;
				}

				if (EListenerState::Opened != state)
				{
					continue;
				}
				listenerInfo.state.store(EListenerState::Closed);

				try {
					m_closingListeners.emplace_back(name, listenerInfo.listener->close());
				}
				catch (const std::exception& e) {
					LogError("Restful failed to close - {} : {}", name, e.what());
				}
			}

			return result;
		});

	_coordinator.Push("restful listener close wait", [this]()
		{
			for (auto& closing : m_closingListeners)
			{
				if (!closing.second.is_done())
				{
					return EStepResult::Wait;
				}
			}

			for (auto& [name, closeTask] : m_closingListeners)
			{
				try {
					closeTask.wait();
					Log("Closed Restful - {}", name);
				}
				catch (const std::exception& e) {
					LogError("Restful failed to wait close - {} : {}", name, e.what());
				}
			}
			m_closingListeners.clear();

			return EStepResult::Done;
		});

	_coordinator.Push("restful in flight requests after close",
		[this]()
		{
			return (0 < m_inFlightRequests.load()) ? EStepResult::Wait : EStepResult::Done;
		},
		[this]() { return std::format("remain={}", m_inFlightRequests.load()); });
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

		std::unique_ptr<http_listener> listener;
		try {
			listener = std::make_unique<http_listener>(builder.to_uri(), listenConfig);
		}
		catch (const std::exception& e) {
			LogError("Restful error exception : {}", e.what());
			return EError::RestfulException;
		}

		m_restfulListeners.try_emplace(name, std::move(listener));
	}

	m_initialized.store(true, std::memory_order_release);

	return Result();
}

Result Restful::SetRestfulHandler(const std::string& _listenerName, const RestfulHandler_t _handler)
{
	if (!m_initialized.load(std::memory_order_acquire))
	{
		return EError::NotInitializedRestful;
	}

	const auto found = m_restfulListeners.find(_listenerName);
	if (m_restfulListeners.end() == found)
	{
		return EError::NotExistRestfulListener;
	}

	// check shutdown state after set opening stage
	// shutdown step : reverse order
	auto expect = EListenerState::None;
	if (!found->second.state.compare_exchange_strong(expect, EListenerState::Opening))
	{
		return EError::AlreadyUsingRestfulListener;
	}

	if (IsShutdownStarted())
	{
		found->second.state.store(EListenerState::Closed);
		return EError::Shutdown;
	}

	found->second.listener->support(methods::GET,
		[this, _handler](http_request _req)
		{
			m_inFlightRequests.fetch_add(1);
			if (IsShutdownStarted())
			{
				m_inFlightRequests.fetch_sub(1);
				_req.reply(status_codes::NotFound, "shutdown.");
				return;
			}

			auto path = _req.request_uri().path();
			auto query = web::uri::decode(_req.request_uri().query());

			auto requestId = ++m_restfulReqIdSequence;

			{
				SCOPED_WRITE_LOCK(m_restfulRequestMutex);
				m_waitRestfulRequests.emplace(requestId, _req);
			}

			(*_handler)(requestId, path, query, static_cast<WebAccessor*>(m_webModule.GetAccessor()));

			m_inFlightRequests.fetch_sub(1);
		});

	found->second.listener->support(methods::OPTIONS,
		[this](http_request _req)
		{
			m_inFlightRequests.fetch_add(1);
			if (IsShutdownStarted())
			{
				m_inFlightRequests.fetch_sub(1);

				http_response response(status_codes::ServiceUnavailable);
				_req.reply(response);
				return;
			}
			http_response response(status_codes::OK);

			response.headers().add(U("Access-Control-Allow-Origin"), U("*")); // allow all domain
			response.headers().add(U("Access-Control-Allow-Methods"), U("POST, GET, OPTIONS"));
			response.headers().add(U("Access-Control-Allow-Headers"), U("Content-Type"));

			_req.reply(response);

			m_inFlightRequests.fetch_sub(1);
		});
	//found->second.listener->support(methods::POST,
	//	[this, _handler](http_request _req)
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
			.then([name = found->first, uriString]() { Log("Started Restful - {}({})", name, uriString.c_str()); })
			.wait();
	}
	catch (const std::exception& e) {
		LogError("Rest error exception : {}", e.what());
		found->second.state.store(EListenerState::None);
		return EError::RestfulException;
	}

	found->second.state.store(EListenerState::Opened);

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

	// reply throws when the request is already gone. the listener timeout can take it away
	// while the business logic is still working on the response.
	utility::string_t jsonString;
	try {
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
	catch (const std::exception& e) {
		LogError("Restful failed to reply - {} : {}", _requestId, e.what());
	}
}