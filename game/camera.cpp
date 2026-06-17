#include "camera.h"

#include <algorithm>

#include "entity/entity.h"   // full Entity, needed by follow()/followSmooth()

namespace arx {

Camera Camera::centered() {
    Camera c;
    c.offset = { GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f };
    return c;
}

Camera2D Camera::raw() const {
    Camera2D c;
    c.offset   = { offset.x + shakeOffset.x, offset.y + shakeOffset.y };
    c.target   = { target.x, target.y };
    c.rotation = rotation;
    c.zoom     = zoom;
    return c;
}

void Camera::clampToBounds(Vec2 mn, Vec2 mx, Vec2 viewSize) {
    float halfW = (viewSize.x * 0.5f) / zoom;
    float halfH = (viewSize.y * 0.5f) / zoom;
    float loX = mn.x + halfW, hiX = mx.x - halfW;
    float loY = mn.y + halfH, hiY = mx.y - halfH;
    target.x = (loX > hiX) ? (mn.x + mx.x) * 0.5f : std::min(std::max(target.x, loX), hiX);
    target.y = (loY > hiY) ? (mn.y + mx.y) * 0.5f : std::min(std::max(target.y, loY), hiY);
}

void Camera::shake(float magnitude, float duration) {
    shakeMag = magnitude; shakeDur = duration; shakeTime = duration;
}

void Camera::update(float dt) {
    if (shakeTime <= 0.0f) { shakeOffset = { 0.0f, 0.0f }; return; }
    shakeTime -= dt;
    if (shakeTime <= 0.0f) { shakeTime = 0.0f; shakeOffset = { 0.0f, 0.0f }; return; }
    float k = shakeMag * (shakeTime / shakeDur);   // decays to 0 over the duration
    shakeOffset = { GetRandomValue(-100, 100) / 100.0f * k,
                    GetRandomValue(-100, 100) / 100.0f * k };
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
