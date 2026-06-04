#pragma once

// --------------- client to server ----------------------------
bool Handler(UserSession& _user, Client::TestMessage&& _packet);

// --------------- server common ----------------------------
bool Handler(ServerSession& _server, ServerCommon::Activation&& _packet);
bool Handler(ServerSession& _server, ServerCommon::Shutdown&& _packet);
bool Handler(ServerSession& _server, ServerCommon::Ping&& _packet);
bool Handler(ServerSession& _server, ServerCommon::Pong&& _packet);

// --------------- server to server ----------------------------
bool Handler(ServerSession& _server, ServerTest::Test&& _packet);

