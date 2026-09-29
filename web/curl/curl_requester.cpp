#include "pch.h"

static_assert(WEB_MODULE == 1);

namespace
{
	struct CurlHandleHolder
	{
		CURL* handle = nullptr;

		~CurlHandleHolder()
		{
			if (nullptr != handle)
			{
				curl_easy_cleanup(handle);
				handle = nullptr;
			}
		}
	};

	CURL* GetThreadCurlHandle()
	{
		thread_local CurlHandleHolder holder;	// reuse

		if (nullptr == holder.handle)
		{
			holder.handle = curl_easy_init();
		}
		else
		{
			curl_easy_reset(holder.handle);
		}

		return holder.handle;
	}

	size_t CurlWriteBufferCallback(char* _contents, size_t _size, size_t _nmemb, std::string* _response)
	{
		const size_t count = _size * _nmemb;
		if (nullptr != _response && 0 < count)
		{
			_response->append(_contents, count);
		}

		return count;
	}
}

void CurlRequesterImpl::RegisterShutdownSteps(ShutdownCoordinator& _coordinator)
{
	_coordinator.Push("curl requester processing requests",
		[this]()
		{
			return (0 < m_processingCount.load()) ? EStepResult::Wait : EStepResult::Done;
		},
		[this]() { return std::format("remain={}", m_processingCount.load()); });
}

CurlRequestId_t CurlRequesterImpl::Request(CurlRequest&& _request, const CurlResponseHandler_t _handler)
{
	if (nullptr == _handler || _request.url.empty())
	{
		return INVALID_CURL_REQUEST_ID;
	}

	// count up before reading the shutdown state.
	// the shutdown step reads the counter after the state is written, so the two orders are
	// reversed and one of them always sees the other. see development_rule.txt 4)
	m_processingCount.fetch_add(1);
	if (IsShutdownStarted())
	{
		m_processingCount.fetch_sub(1);
		return INVALID_CURL_REQUEST_ID;
	}

	CurlRequestId_t requestId = (++m_sequence);
	if (INVALID_CURL_REQUEST_ID == requestId)
	{
		requestId = (++m_sequence);
	}

	const uint32_t defaultTimeoutSec = m_curl.GetDefaultTimeoutSec();

	m_curl.GetWorkerThreadPool().PushJob(
		[this, requestId, _handler, defaultTimeoutSec, request = std::move(_request)]() mutable
		{
			CurlResponse response;
			response.requestId = requestId;
			response.result = Perform(request, defaultTimeoutSec, response);

			if (!response.result)
			{
				LogError("curl request failed - {}({}) : {}", requestId, request.url, response.result);
			}

			// the handler is a global function of another Module. see development_rule.txt 7-1)
			// it runs on this worker thread, so it must not work here. see CurlResponseHandler_t
			(*_handler)(std::move(response));

			m_processingCount.fetch_sub(1);
		});

	return requestId;
}

Result CurlRequesterImpl::Perform(const CurlRequest& _request, const uint32_t& _defaultTimeoutSec, CurlResponse& _response)
{
	CURL* handle = GetThreadCurlHandle();
	if (nullptr == handle)
	{
		return EError::FailedCurlInit;
	}

	curl_slist* headers = nullptr;
	for (const auto& header : _request.headers)
	{
		headers = curl_slist_append(headers, header.c_str());
	}

	FinalJob releaseHeaders([&headers]()
		{
			if (nullptr != headers)
			{
				curl_slist_free_all(headers);
			}
		});

	const long timeoutSec = static_cast<long>((0 < _request.timeoutSec) ? _request.timeoutSec : _defaultTimeoutSec);

	curl_easy_setopt(handle, CURLOPT_URL, _request.url.c_str());
	curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(handle, CURLOPT_TIMEOUT, timeoutSec);
	curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);

	// CURLOPT_WRITEFUNCTION & CURLOPT_WRITEDATA pair
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, CurlWriteBufferCallback);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, &_response.body);

	curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, _request.verifySsl ? 1L : 0L);
	curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, _request.verifySsl ? 2L : 0L);

	if (nullptr != headers)
	{
		curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);
	}

	// the request lives until the job is done, so curl does not have to copy the body
	switch (_request.method)
	{
	case ECurlMethod::Get:
		curl_easy_setopt(handle, CURLOPT_HTTPGET, 1L);
		break;
	case ECurlMethod::Post:
		curl_easy_setopt(handle, CURLOPT_POST, 1L);
		curl_easy_setopt(handle, CURLOPT_POSTFIELDS, _request.body.c_str());
		curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE, static_cast<long>(_request.body.size()));
		break;
	case ECurlMethod::Put:
		curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, "PUT");
		curl_easy_setopt(handle, CURLOPT_POSTFIELDS, _request.body.c_str());
		curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE, static_cast<long>(_request.body.size()));
		break;
	case ECurlMethod::Delete:
		curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, "DELETE");
		break;
	default:
		return EError::InvalidParam;
	}

	const CURLcode errorCode = curl_easy_perform(handle);
	if (CURLE_OK != errorCode)
	{
		return Result(EError::FailedCurlRequest, curl_easy_strerror(errorCode));
	}

	long httpCode = 0;
	curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &httpCode);
	_response.httpCode = httpCode;

	return EError::Success;
}

// ------------------------------------------------------------------------------------------

Result Curl::Init(const nlohmann::json& _config)
{
	uint16_t threadCount{ WebConfigConst::DEFAULT_CURL_THREAD_COUNT };
	m_defaultTimeoutSec = WebConfigConst::DEFAULT_CURL_TIMEOUT_SEC;

	auto curlConfig = _config["curl"];
	if (!curlConfig.is_null())
	{
		if (curlConfig["thread count"].is_number_unsigned())
		{
			threadCount = curlConfig["thread count"].get<uint16_t>();
		}
		if (curlConfig["timeout"].is_number_unsigned())
		{
			m_defaultTimeoutSec = curlConfig["timeout"].get<uint32_t>();
		}
	}

	if (0 == threadCount)
	{
		return EError::InvalidThreadCount;
	}

	const CURLcode errorCode = curl_global_init(CURL_GLOBAL_DEFAULT);
	if (CURLE_OK != errorCode)
	{
		LogError("curl_global_init failed : {}", curl_easy_strerror(errorCode));
		return EError::FailedCurlInit;
	}

	m_workerThreadPool.Init(threadCount);

	m_initialized.store(true, std::memory_order_release);

	Log("Started Curl - thread={}, timeout={}s", threadCount, m_defaultTimeoutSec);

	return EError::Success;
}

CurlRequesterImplShared_t Curl::AllocCurlRequester()
{
	if (!m_initialized.load(std::memory_order_acquire))
	{
		LogError("curl is not initialized");
		return nullptr;
	}

	// if lock after check shutdown
	//   IsShutdownStarted() false |
	//                             | the shutdown step moves m_requesters out
	//   m_requesters.emplace_back |
	SCOPED_WRITE_LOCK(m_mutex);
	if (IsShutdownStarted())
	{
		return nullptr;
	}

	auto requester = CurlRequesterImpl::Create(*this);
	m_requesters.emplace_back(requester);

	return requester;
}

void Curl::RegisterShutdownSteps(ShutdownCoordinator& _coordinator)
{
	// AllocCurlRequester checks the shutdown state under the same lock,
	// so anything not moved out here shuts itself down.
	// the owner Module normally did it already. this only covers the ones that were left
	decltype(m_requesters) targets;
	{
		SCOPED_WRITE_LOCK(m_mutex);
		targets = std::move(m_requesters);
	}

	for (auto& requester : targets)
	{
		requester->Shutdown(_coordinator, "curl requester shutdown.");
	}

	_coordinator.Push("curl requester release", [targets = std::move(targets)]() mutable
		{
			targets.clear();

			return EStepResult::Done;
		});

	m_workerThreadPool.Shutdown(_coordinator, "curl worker thread pool shutdown.");

	// after the worker threads are joined. the thread local handles are cleaned up there
	_coordinator.Push("curl global cleanup", [this]()
		{
			if (m_initialized.load(std::memory_order_acquire))
			{
				curl_global_cleanup();
			}

			return EStepResult::Done;
		});
}
