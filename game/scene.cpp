#include "scene.h"

namespace arx {

Entity* Scene::add(std::unique_ptr<Entity> e) {
    Entity* ptr = e.get();
    entities.push_back(std::move(e));
    return ptr;
}

void Scene::attach(Entity* e) {
    if (e) { attached.push_back(e); }   // non-owning: the registry keeps it alive, not us
}

void Scene::enter(SceneContext& ctx) {
    attached.clear();            // rebuild the borrow list from scratch each activation,
    if (onEnter) { onEnter(ctx); }   // so re-entering a scene can't carry a stale pointer
}

void Scene::handleInput(SceneContext& ctx) { if (onHandleInput) { onHandleInput(ctx); } };

void Scene::update(SceneContext& ctx) {
    for (auto& e : entities)   { e->update(ctx.dt); }   // 1a. owned entities move
    for (Entity* e : attached) { e->update(ctx.dt); }   // 1b. borrowed participants move
    resolveCollisions(ctx);                             // 2. detect overlaps, dispatch responders
    if (onUpdate) { onUpdate(ctx); }                    // 3. game's per-frame logic
    reapDead();                                         // 4. reap last, so collision deaths clear now
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
        if (a->radius <= 0.0f || !a->alive) { continue; }

        for (size_t j = i + 1; j < n; ++j) {
            if (!a->alive) { break; }   // a died from an earlier pair this frame -> stop pairing it

            Entity* b = parts[j];
            if (b->radius <= 0.0f || !b->alive) { continue; }

            // squared-distance test: no sqrt
            Vec2 d = b->position - a->position;
            float rSum = a->radius + b->radius;
            if (d.x * d.x + d.y * d.y > rSum * rSum) { continue; }

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
    if (useCam) { EndMode2D(); }

    if (onDraw) { onDraw(ctx); }   // HUD pass: screen space, never moved by the camera
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