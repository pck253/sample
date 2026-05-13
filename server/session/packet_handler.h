#pragma once

// --------------- client to server ----------------------------
bool Handler(UserSessionShared_t& _user, Client::TestMessage&& _packet);

// --------------- server common ----------------------------
bool Handler(ServerSessionShared_t& _server, ServerCommon::Activation&& _packet);
bool Handler(ServerSessionShared_t& _server, ServerCommon::Shutdown&& _packet);

// --------------- server to server ----------------------------
bool Handler(ServerSessionShared_t& _server, ServerTest::Test&& _packet);

