#include "pch.h"

static_assert(WEB_MODULE == 1);

MODULE_STATIC_IMPL(Web);

Web::Web(const std::string& _configFilePath)
	: Module(_configFilePath), m_restful(*this)
{
	m_accessor = WebAccessorImpl::Create(this);
}

Web::~Web()
{
}

//static size_t CurlWriteBufferCallback(char* _contents, size_t _size, size_t _nmemb, std::string* _response)
//{
//	size_t count = _size * _nmemb;
//	if (_response != nullptr && count > 0)
//	{
//		_response->append(_contents, count);
//	}
//
//	return count;
//}

Result Web::InitImpl()
{
	auto ret = m_restful.Init(m_config);
	if (!ret)
	{
		return ret;
	}

	curl_global_init(CURL_GLOBAL_DEFAULT);

	{
		// test

		//CURL* curl = curl_easy_init();
		//if (curl == nullptr)
		//{
		//	LogError("Curl init failed");
		//	return EError::RestfulException;
		//}

		// if CURL instance reuse, call curl_easy_reset

		//static constexpr const char* url = "https://google.com";
		//curl_easy_setopt(curl, CURLOPT_URL, url);


		////curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
		////curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);

		////curl_easy_setopt(curl, CURLOPT_CA_CACHE_TIMEOUT, 604800L);

		////std::string response;
		////curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
		//// CURLOPT_WRITEFUNCTION & CURLOPT_WRITEDATA pair
		////curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteBufferCallback);
		////curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

		////curl_slist* slist = nullptr;
		////slist = curl_slist_append(slist, "Accept: application/json");
		////curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);

		//curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
		//CURLcode err_code = curl_easy_perform(curl);
		//if (err_code != CURLE_OK)
		//{
		//	LogError("curl_easy_perform failed : {}", curl_easy_strerror(err_code));
		//	return EError::RestfulException;
		//}

		//std::size_t response_code = 0;
		//curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
		//Log("http response code : {}", response_code);
		////Log("http response : {}", response);

		////curl_slist_free_all(slist);
		//curl_easy_cleanup(curl);	// or curl_easy_reset
	}

	return EError::Success;
}

void Web::Shutdown()
{
	m_restful.Shutdown(EShutdownMode::RightNow, "restful shutdown.");

	curl_global_cleanup();
}