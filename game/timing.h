#pragma once

#include <functional>

namespace arx {

// Easing curves: take a normalized t in [0,1], return an eased [0,1].
namespace ease {
    inline float clamp01(float t) { return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t); }
    inline float linear(float t)   { return t; }
    inline float inQuad(float t)   { return t * t; }
    inline float outQuad(float t)  { return t * (2.0f - t); }
    inline float inOutQuad(float t){ return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t; }
    inline float inCubic(float t)  { return t * t * t; }
    inline float outCubic(float t) { float u = t - 1.0f; return u * u * u + 1.0f; }
    inline float outBack(float t)  { const float c1 = 1.70158f, c3 = c1 + 1.0f; float u = t - 1.0f;
                                     return 1.0f + c3 * u * u * u + c1 * u * u; }
}

// A one-shot countdown. tick() returns true exactly on the frame it elapses, so it
// reads naturally: `if (timer.tick(dt)) { spawn(); }`. Replaces the hand-rolled
// shared_ptr<float> timers (flash window, death delay, spawn cadence, ...).
struct Timer {
    float remaining = 0.0f;
    bool  active = false;

    void start(float seconds) { remaining = seconds; active = true; }
    void stop()               { active = false; }
    bool running() const      { return active; }

    bool tick(float dt) {
        if (!active) { return false; }
        remaining -= dt;
        if (remaining <= 0.0f) { active = false; return true; }
        return false;
    }
};

// A repeating timer: fires every `period` seconds, returning the number of times it
// fired this frame (usually 0 or 1; >1 only after a long stall).
struct Interval {
    float period = 1.0f;
    float accum = 0.0f;

    explicit Interval(float p = 1.0f) : period(p) {}

    int tick(float dt) {
        if (period <= 0.0f) { return 0; }
        accum += dt;
        int fires = 0;
        while (accum >= period) { accum -= period; ++fires; }
        return fires;
    }
};

// Interpolates a float from `from` to `to` over `duration` with an easing curve.
// tick() advances it and returns true on completion; value() reads the current value.
struct Tween {
    float from = 0.0f, to = 0.0f, duration = 0.0f, elapsed = 0.0f;
    std::function<float(float)> easing = ease::linear;
    bool active = false;

    void start(float a, float b, float dur, std::function<float(float)> e = ease::linear) {
        from = a; to = b; duration = dur; elapsed = 0.0f; easing = std::move(e); active = true;
    }
    void stop()        { active = false; }
    bool running() const { return active; }
    bool done() const  { return duration <= 0.0f || elapsed >= duration; }

    float value() const {
        if (duration <= 0.0f) { return to; }
        float t = ease::clamp01(elapsed / duration);
        return from + (to - from) * easing(t);
    }

    bool tick(float dt) {
        if (!active) { return false; }
        elapsed += dt;
        if (elapsed >= duration) { elapsed = duration; active = false; return true; }
        return false;
    }
};

} // namespace arx
