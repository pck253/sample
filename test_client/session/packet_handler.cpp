#include "pch.h"

bool g_printTestMessage = false;

bool Handler(SessionShared_t& _session, const Client::TestMessage* _packetBody, our::vector<uint8_t>&& _rawData)
{
	std::string title = _packetBody->title()->c_str();
	std::string body = _packetBody->body()->c_str();

	_session->PushJob([_session, _title = std::move(title), _body = std::move(body), _receivedByte = _rawData.size()]()
		{
			if (g_printTestMessage)
			{
				Log("{} : {}", _title, _body);
			}

			_session->SetReceivedInfo(_receivedByte);
		});

	return true;
}