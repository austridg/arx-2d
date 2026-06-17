#include "camera.h"

#include "entity/entity.h"   // full Entity, needed by follow()/followSmooth()

namespace arx {

Camera Camera::centered() {
    Camera c;
    c.offset = { GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f };
    return c;
}

Camera2D Camera::raw() const {
    Camera2D c;
    c.offset   = { offset.x, offset.y };
    c.target   = { target.x, target.y };
    c.rotation = rotation;
    c.zoom     = zoom;
    return c;
}

void Camera::follow(const Entity& e) { target = e.position; }

void Camera::followSmooth(const Entity& e, float dt, float speed) {
    // move a fraction of the remaining distance each frame -> a soft, lazy cam.
    float t = speed * dt;
    if (t > 1.0f) { t = 1.0f; }   // never overshoot, even on a big dt spike
    target += (e.position - target) * t;
}

Vec2 Camera::screenToWorld(Vec2 screen) const {
    Vector2 w = GetScreenToWorld2D({ screen.x, screen.y }, raw());
    return { w.x, w.y };
}

Vec2 Camera::worldToScreen(Vec2 world) const {
    Vector2 s = GetWorldToScreen2D({ world.x, world.y }, raw());
    return { s.x, s.y };
}

} // namespace arx
