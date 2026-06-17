#pragma once

#include "raylib.h"        // Camera2D + GetWorldToScreen2D / GetScreenToWorld2D / GetScreenWidth/Height
#include "entity/vec2.h"   // Vec2

namespace arx {

class Entity;   // follow() reads its position; full definition in entity/entity.h

// A 2D view onto the world: a thin wrapper over raylib's Camera2D. The GAME drives
// it (points it at whatever it wants to follow); the Scene applies it during draw
// by bracketing the world pass in BeginMode2D/EndMode2D. A scene with no camera
// draws straight in screen space, exactly as before -- so this is purely additive.
struct Camera {
    Vec2  target;            // world point the camera centers on
    Vec2  offset;            // screen point `target` maps to (usually the screen center)
    float rotation = 0.0f;   // degrees (raylib's convention)
    float zoom     = 1.0f;

    // --- screen-shake state (ticked by the scene each frame; applied in raw()) ---
    float shakeMag = 0.0f, shakeDur = 0.0f, shakeTime = 0.0f;
    Vec2  shakeOffset;

    // a camera whose offset is the current window's center -- the common follow-cam
    // setup, so `target` lands in the middle of the screen.
    static Camera centered();

    Camera2D raw() const;    // hand off to BeginMode2D (includes any shake offset)

    void follow(const Entity& e);                              // snap target onto the entity
    void followSmooth(const Entity& e, float dt, float speed); // ease target toward the entity (lazy cam)

    // keep the visible rect inside [min,max] world bounds (viewSize = screen px). If the
    // world is smaller than the view on an axis, it centers on that axis instead.
    void clampToBounds(Vec2 min, Vec2 max, Vec2 viewSize);

    void shake(float magnitude, float duration);   // kick off a decaying screen shake
    void update(float dt);                          // advance the shake (Scene calls this)

    Vec2 screenToWorld(Vec2 screen) const;   // e.g. mouse position -> world (picking)
    Vec2 worldToScreen(Vec2 world) const;
};

} // namespace arx
