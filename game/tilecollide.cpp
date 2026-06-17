#include "tilecollide.h"

#include <algorithm>
#include <cmath>

namespace arx {

void resolveTileCollision(Entity& e, const TileGrid& g) {
    if (g.tileSize <= 0 || !g.tileAt || !g.isSolid) { return; }

    Vec2 half = (e.shape == Entity::Shape::Box) ? e.halfExtents : Vec2{ e.radius, e.radius };
    if (half.x <= 0.0f || half.y <= 0.0f) { return; }

    // a few passes let a contact spanning several tiles settle
    for (int iter = 0; iter < 4; ++iter) {
        float left   = e.position.x - half.x;
        float right  = e.position.x + half.x;
        float top    = e.position.y - half.y;
        float bottom = e.position.y + half.y;

        int c0 = (int)std::floor((left   - g.origin.x) / g.tileSize);
        int c1 = (int)std::floor((right  - g.origin.x) / g.tileSize);
        int r0 = (int)std::floor((top    - g.origin.y) / g.tileSize);
        int r1 = (int)std::floor((bottom - g.origin.y) / g.tileSize);

        // find the solid cell with the smallest penetration and push out of just that
        // one this pass; repeating converges without the bias of resolving all at once.
        float bestPen  = 1e30f;
        int   bestAxis = 0;
        float bestPush = 0.0f;

        for (int r = r0; r <= r1; ++r) {
            if (g.rows > 0 && (r < 0 || r >= g.rows)) { continue; }
            for (int c = c0; c <= c1; ++c) {
                if (g.cols > 0 && (c < 0 || c >= g.cols)) { continue; }
                if (!g.isSolid(g.tileAt(c, r))) { continue; }

                float tl = g.origin.x + c * (float)g.tileSize;
                float tr = tl + g.tileSize;
                float tt = g.origin.y + r * (float)g.tileSize;
                float tb = tt + g.tileSize;

                float ox = std::min(right, tr) - std::max(left, tl);   // x overlap depth
                float oy = std::min(bottom, tb) - std::max(top, tt);   // y overlap depth
                if (ox <= 0.0f || oy <= 0.0f) { continue; }

                if (ox < oy) {   // shallower on x -> resolve horizontally
                    float push = (e.position.x < tl + g.tileSize * 0.5f) ? -ox : ox;
                    if (ox < bestPen) { bestPen = ox; bestAxis = 0; bestPush = push; }
                } else {         // resolve vertically
                    float push = (e.position.y < tt + g.tileSize * 0.5f) ? -oy : oy;
                    if (oy < bestPen) { bestPen = oy; bestAxis = 1; bestPush = push; }
                }
            }
        }

        if (bestPen >= 1e30f) { break; }   // no overlaps left -> settled

        if (bestAxis == 0) {
            e.position.x += bestPush;
            if ((bestPush < 0.0f && e.velocity.x > 0.0f) || (bestPush > 0.0f && e.velocity.x < 0.0f)) { e.velocity.x = 0.0f; }
        } else {
            e.position.y += bestPush;
            if ((bestPush < 0.0f && e.velocity.y > 0.0f) || (bestPush > 0.0f && e.velocity.y < 0.0f)) { e.velocity.y = 0.0f; }
        }
    }
}

} // namespace arx
