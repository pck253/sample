#pragma once

using AuthKey_t = UniqueId_t;

using ServerId_t = StrongId<uint16_t, 0ui16>;
using ServerGroupId_t = StrongId<uint16_t, 0ui16>;

using WorldPos_t = Vector<double>;

class Actor;
using ActorShared_t = std::shared_ptr<Actor>;
class ActorManager;

class Grid;