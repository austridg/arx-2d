#pragma once

#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
#include <typeindex>
#include <utility>

#include "rad2d.hpp"

#include "vec2.h"
#include "behavior.h"   // forward-declares Entity + defines BehaviorFn

namespace arx {

struct SceneContext;     // defined in game/scene.h

// A collision responder reacts when `self` overlaps `other` this frame. Unlike a
// BehaviorFn it also receives the SceneContext, because reacting to a hit often
// means spawning (an asteroid splitting into two) or reading the rng. An entity
// with no responders simply doesn't react to overlaps -- same way no drawable
// means it renders nothing.
using CollisionFn = std::function<void(Entity& self, Entity& other, SceneContext& ctx)>;

// An Entity is a "thing in the world": a transform, an optional visual, and a
// list of behaviors that drive it each frame. Visible or not, everything a scene
// (or the persistent registry) holds is an Entity. An entity with no drawable is
// a pure-logic object (a score tracker, a trigger), which is perfectly fine.
class Entity {
public:
    int id = 0;                      // identifier (use your own enum); used by PersistentRegistry::find

    // --- transform: the simulation's truth (floats) ---
    Vec2  position;
    Vec2  velocity;
    float rotation = 0.0f;           // radians

    // --- collision shape, centered on `position` ---
    enum class Shape { Circle, Box };
    Shape shape = Shape::Circle;
    float radius = 0.0f;             // Circle: 0 = non-colliding (mirrors null drawable = invisible)
    Vec2  halfExtents;               // Box: half width/height; (0,0) = non-colliding

    // does this entity take part in collision at all?
    bool collidable() const {
        return shape == Shape::Circle ? radius > 0.0f
                                      : (halfExtents.x > 0.0f && halfExtents.y > 0.0f);
    }

    bool  alive = true;              // set false -> reaped by the owning scene/registry

    // --- visual (optional; null = an invisible logic entity) ---
    std::unique_ptr<rad2d::Drawable> drawable;

    // --- behaviors: what this entity does each frame ---
    std::vector<BehaviorFn> behaviors;

    // --- collision responders: how this entity reacts to overlapping another ---
    std::vector<CollisionFn> collisionResponders;

    // --- components: typed, queryable per-entity state (Health, Inventory, ...) ---
    // shared across this entity's behaviors/responders/systems without manual plumbing.
    std::unordered_map<std::type_index, std::shared_ptr<void>> components;

    Entity() = default;
    explicit Entity(int id);

    void addBehavior(BehaviorFn b);
    void addCollisionResponder(CollisionFn r);

    // attach a component of type T (constructed from args), replacing any existing one.
    template <class T, class... Args>
    T& addComponent(Args&&... args) {
        auto p = std::make_shared<T>(std::forward<Args>(args)...);
        T& ref = *p;
        components[std::type_index(typeid(T))] = std::move(p);
        return ref;
    }
    // fetch component T, or nullptr if this entity has none.
    template <class T>
    T* get() {
        auto it = components.find(std::type_index(typeid(T)));
        return it == components.end() ? nullptr : static_cast<T*>(it->second.get());
    }
    template <class T>
    bool has() const { return components.find(std::type_index(typeid(T))) != components.end(); }

    template <class T>
    void removeComponent() { components.erase(std::type_index(typeid(T))); }

    void update(float dt);                          // runs every behavior on this entity
    void onHit(Entity& other, SceneContext& ctx);   // runs every collision responder
    void draw();                                    // pushes the transform into the drawable, then draws

    bool hasDrawable() const;        // is there anything to render?
    int  layer() const;              // z-layer (from the drawable; 0 if none)
};

} // namespace arx
