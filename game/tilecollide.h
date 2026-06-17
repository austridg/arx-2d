#pragma once

#include <functional>

#include "entity/vec2.h"
#include "entity/entity.h"

namespace arx {

// The solid-tile grid the resolver tests an entity against. The game supplies how to
// read a cell (e.g. straight from a rad2d::Tilemap::getTile) and which ids are solid,
// so the engine needs no knowledge of the map format. cols/rows = 0 means "unbounded"
// (only the solid predicate decides); set them to clamp to the map's extent.
struct TileGrid {
    std::function<int(int, int)> tileAt;   // (col,row) -> tile id (0 = empty)
    std::function<bool(int)>     isSolid;  // which tile ids block movement
    int  tileSize = 32;
    Vec2 origin;                           // world position of cell (0,0)'s top-left corner
    int  cols = 0, rows = 0;               // grid bounds; 0 = unbounded
};

// Push `e` out of any solid cells along the least-penetration axis, zeroing the
// blocked velocity component (lands on floors, stops at walls). A Circle entity is
// treated as a square AABB of side 2*radius. Call after integrating the entity.
void resolveTileCollision(Entity& e, const TileGrid& grid);

} // namespace arx
