#pragma once

DEFINE_STRONG_ID(AuthKey_t, uint64_t, 0ui64);

DEFINE_STRONG_ID(ServerId_t, uint16_t, 0ui16);
DEFINE_STRONG_ID(ServerGroupId_t, uint16_t, 0ui16);

using WorldPos_t = Vector<double, 0.0>;

class Actor;
using ActorShared_t = std::shared_ptr<Actor>;
class ActorManager;

class Grid;