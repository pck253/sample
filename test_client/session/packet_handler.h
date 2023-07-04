#pragma once

bool Handler(SessionShared_t& _session, const Client::TestMessage* _packetBody, our::vector<uint8_t>&& _rawData);