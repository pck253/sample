#pragma once

static_assert(WEB_MODULE == 1);

class Curl;

class CurlRequesterImpl;
using CurlRequesterImplShared_t = std::shared_ptr<CurlRequesterImpl>;

class CurlRequesterImpl : public CurlRequester
{
public:
	static CurlRequesterImplShared_t Create(Curl& _curl)
	{
		return CurlRequesterImplShared_t(new CurlRequesterImpl(_curl));
	}

	~CurlRequesterImpl() = default;

	virtual CurlRequestId_t Request(CurlRequest&& _request, const CurlResponseHandler_t _handler) final;

protected:
	virtual void RegisterShutdownSteps(ShutdownCoordinator& _coordinator) override;

private:
	CurlRequesterImpl(Curl& _curl)
		: m_curl(_curl)
	{
	}

	static Result Perform(const CurlRequest& _request, const uint32_t& _defaultTimeoutSec, CurlResponse& _response);

	Curl& m_curl;

	std::atomic<CurlRequestId_t> m_sequence{ INVALID_CURL_REQUEST_ID };
	std::atomic<int32_t> m_processingCount{ 0 };
};

// ------------------------------------------------------------------------------------------
// - owns the worker ThreadPool that runs curl_easy_perform and the response handler
// - owns curl_global_init / curl_global_cleanup
// ------------------------------------------------------------------------------------------
class Curl : public UseShutdown
{
public:
	Curl() = default;
	~Curl() = default;

	Result Init(const nlohmann::json& _config);

	CurlRequesterImplShared_t AllocCurlRequester();

	ThreadPool& GetWorkerThreadPool() { return m_workerThreadPool; }
	const uint32_t& GetDefaultTimeoutSec() const { return m_defaultTimeoutSec; }

protected:
	virtual void RegisterShutdownSteps(ShutdownCoordinator& _coordinator) override;

private:
	std::atomic_bool m_initialized = false;
	uint32_t m_defaultTimeoutSec{};

	// the requests are blocking. the concurrent request count is the thread count
	ThreadPool m_workerThreadPool{ "curl worker thread pool" };

	std::shared_mutex m_mutex;
	std::vector<CurlRequesterImplShared_t> m_requesters;
};
