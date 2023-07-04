#pragma once

// --------------- client to server ----------------------------
bool Handler(UserSessionShared_t& _user, const Client::TestMessage* _packetBody, our::vector<uint8_t>&& _rawData);

// --------------- server common ----------------------------
bool Handler(ServerSessionShared_t& _server, const ServerCommon::Activation* _packetBody, our::vector<uint8_t>&& _rawData);
bool Handler(ServerSessionShared_t& _server, const ServerCommon::Shutdown* _packetBody, our::vector<uint8_t>&& _rawData);

// --------------- server to server ----------------------------
bool Handler(ServerSessionShared_t& _server, const ServerTest::Test* _packetBody, our::vector<uint8_t>&& _rawData);

