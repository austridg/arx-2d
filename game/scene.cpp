#include "scene.h"

#include <cmath>

namespace arx {

namespace {
// shape-aware overlap test for the broadphase (circle/box, any combination)
bool overlaps(const Entity& a, const Entity& b) {
    using S = Entity::Shape;
    if (a.shape == S::Circle && b.shape == S::Circle) {
        Vec2 d = b.position - a.position;
        float rs = a.radius + b.radius;
        return d.x * d.x + d.y * d.y <= rs * rs;
    }
    if (a.shape == S::Box && b.shape == S::Box) {
        return std::abs(a.position.x - b.position.x) <= a.halfExtents.x + b.halfExtents.x &&
               std::abs(a.position.y - b.position.y) <= a.halfExtents.y + b.halfExtents.y;
    }
    // circle vs box: clamp the circle's center into the box, then test the distance
    const Entity& c   = (a.shape == S::Circle) ? a : b;
    const Entity& box = (a.shape == S::Circle) ? b : a;
    float cx = std::max(box.position.x - box.halfExtents.x, std::min(c.position.x, box.position.x + box.halfExtents.x));
    float cy = std::max(box.position.y - box.halfExtents.y, std::min(c.position.y, box.position.y + box.halfExtents.y));
    float dx = c.position.x - cx, dy = c.position.y - cy;
    return dx * dx + dy * dy <= c.radius * c.radius;
}
} // namespace

Entity* Scene::add(std::unique_ptr<Entity> e) {
    Entity* ptr = e.get();
    entities.push_back(std::move(e));
    return ptr;
}

void Scene::attach(Entity* e) {
    if (e) { attached.push_back(e); }   // non-owning: the registry keeps it alive, not us
}

Entity* Scene::find(int id) {
    for (auto& e : entities)   { if (e->id == id) { return e.get(); } }
    for (Entity* e : attached) { if (e->id == id) { return e; } }
    return nullptr;
}

std::vector<Entity*> Scene::findAll(int id) {
    std::vector<Entity*> out;
    for (auto& e : entities)   { if (e->id == id) { out.push_back(e.get()); } }
    for (Entity* e : attached) { if (e->id == id) { out.push_back(e); } }
    return out;
}

Entity* Scene::nearest(Vec2 from, int id) {
    Entity* best = nullptr;
    float bestD2 = 0.0f;
    auto consider = [&](Entity* e) {
        if (e->id != id) { return; }
        Vec2 d = e->position - from;
        float d2 = d.x * d.x + d.y * d.y;
        if (!best || d2 < bestD2) { best = e; bestD2 = d2; }
    };
    for (auto& e : entities)   { consider(e.get()); }
    for (Entity* e : attached) { consider(e); }
    return best;
}

std::vector<Entity*> Scene::inRadius(Vec2 center, float r, int id) {
    std::vector<Entity*> out;
    float r2 = r * r;
    auto consider = [&](Entity* e) {
        if (e->id != id) { return; }
        Vec2 d = e->position - center;
        if (d.x * d.x + d.y * d.y <= r2) { out.push_back(e); }
    };
    for (auto& e : entities)   { consider(e.get()); }
    for (Entity* e : attached) { consider(e); }
    return out;
}

void Scene::enter(SceneContext& ctx) {
    entities.clear();            // fresh activation -> rebuild the world from scratch (no
    attached.clear();            // leftover entities stacking up, no stale borrowed pointers)
    if (onEnter) { onEnter(ctx); }
}

void Scene::exit(SceneContext& ctx) { if (onExit) { onExit(ctx); } }

void Scene::handleInput(SceneContext& ctx) { if (onHandleInput) { onHandleInput(ctx); } };

void Scene::update(SceneContext& ctx) {
    for (auto& e : entities)   { e->update(ctx.dt); }   // 1a. owned entities move
    for (Entity* e : attached) { e->update(ctx.dt); }   // 1b. borrowed participants move
    resolveCollisions(ctx);                             // 2. detect overlaps, dispatch responders
    if (onUpdate) { onUpdate(ctx); }                    // 3. game's per-frame logic
    if (camera) { camera->update(ctx.dt); }             // 4. advance camera shake
    reapDead();                                         // 5. reap last, so collision deaths clear now
}

void Scene::resolveCollisions(SceneContext& ctx) {
    // Snapshot everything that collides this frame: owned entities + borrowed
    // participants. Holding raw Entity* is safe even if responders spawn (which
    // appends to `entities` and may reallocate it) -- the Entity objects live on the
    // heap behind unique_ptr, so the pointers stay valid. New spawns land past this
    // snapshot, so they neither get swept nor self-collide until next frame.
    std::vector<Entity*> parts;
    parts.reserve(entities.size() + attached.size());
    for (auto& e : entities)   { parts.push_back(e.get()); }
    for (Entity* e : attached) { parts.push_back(e); }

    const size_t n = parts.size();
    for (size_t i = 0; i < n; ++i) {
        Entity* a = parts[i];
        if (!a->collidable() || !a->alive) { continue; }

        for (size_t j = i + 1; j < n; ++j) {
            if (!a->alive) { break; }   // a died from an earlier pair this frame -> stop pairing it

            Entity* b = parts[j];
            if (!b->collidable() || !b->alive) { continue; }

            if (!overlaps(*a, *b)) { continue; }

            // both react to the mutual hit (bullet dies, asteroid splits)
            a->onHit(*b, ctx);
            b->onHit(*a, ctx);
        }
    }
}

void Scene::draw(SceneContext& ctx) {
    std::vector<Entity*> order;
    for (auto& e : entities)   { if (e->hasDrawable()) { order.push_back(e.get()); } }
    for (Entity* e : attached) { if (e->hasDrawable()) { order.push_back(e); } }   // borrowed draw too, z-sorted together
    std::sort(order.begin(),order.end(),[](Entity* a, Entity* b) { return a->layer() < b->layer(); });

    // world pass: under the camera transform if there is one
    const bool useCam = camera.has_value();
    if (useCam) { BeginMode2D(camera->raw()); }
        for (Entity* e : order) { e->draw(); }
        if (onDrawWorld) { onDrawWorld(ctx); }   // world-space extras share the transform
        if (debugDraw) {
            auto outline = [](Entity* e) {
                if (!e->collidable()) { return; }
                if (e->shape == Entity::Shape::Circle) {
                    DrawCircleLines((int)e->position.x, (int)e->position.y, e->radius, GREEN);
                } else {
                    DrawRectangleLines((int)(e->position.x - e->halfExtents.x),
                                       (int)(e->position.y - e->halfExtents.y),
                                       (int)(e->halfExtents.x * 2), (int)(e->halfExtents.y * 2), GREEN);
                }
            };
            for (auto& e : entities)   { outline(e.get()); }
            for (Entity* e : attached) { outline(e); }
        }
    if (useCam) { EndMode2D(); }

    if (onDraw) { onDraw(ctx); }   // HUD pass: screen space, never moved by the camera
    if (debugDraw) { DrawFPS(GetScreenWidth() - 90, 10); }
}

void Scene::reapDead() {
    // owned: dead entities are destroyed here.
    entities.erase(
        std::remove_if(entities.begin(),entities.end(),[](const std::unique_ptr<Entity>& e) { return !e->alive; }),
        entities.end()
    );
    // borrowed: only stop pointing at dead ones -- the PersistentRegistry owns and
    // destroys them. Dropping them here closes the dangling-pointer window before
    // the registry's own reap runs.
    attached.erase(
        std::remove_if(attached.begin(),attached.end(),[](Entity* e) { return !e->alive; }),
        attached.end()
    );
}

} // namespace arx