#pragma once

#include <memory>
#include <vector>
#include <string>

#include "rad2d.hpp"

#include "vec2.h"
#include "behavior.h"

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

    bool  alive = true;              // set false -> reaped by the owning scene/registry

    // --- visual (optional; null = an invisible logic entity) ---
    std::unique_ptr<rad2d::Drawable> drawable;

    // --- behaviors: what this entity does each frame ---
    std::vector<BehaviorFn> behaviors;

    Entity() = default;
    explicit Entity(int id);

    void addBehavior(BehaviorFn b);

    void update(float dt);           // runs every behavior on this entity
    void draw();                     // pushes the transform into the drawable, then draws

    bool hasDrawable() const;        // is there anything to render?
    int  layer() const;              // z-layer (from the drawable; 0 if none)
};
