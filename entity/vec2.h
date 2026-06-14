#pragma once

#include <cmath>

// A 2D vector: a position, a velocity, or a direction. Bundles x/y so movement
// math reads as one expression (position = position + velocity * dt).
struct Vec2 {
    float x = 0.0f, y = 0.0f;

    Vec2() = default;
    Vec2(float x, float y) : x(x), y(y) {}

    Vec2 operator+(const Vec2& o) const { return { x + o.x, y + o.y }; }
    Vec2 operator-(const Vec2& o) const { return { x - o.x, y - o.y }; }
    Vec2 operator*(float s)       const { return { x * s,   y * s   }; }

    Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }

    float length() const { return std::sqrt(x * x + y * y); }

    // same direction, length 1 (zero vector stays zero, no divide-by-zero)
    Vec2 normalized() const {
        float len = length();
        return len > 0.0f ? Vec2{ x / len, y / len } : Vec2{ 0.0f, 0.0f };
    }

    // a unit vector pointing at `radians` (e.g. a ship's facing)
    static Vec2 fromAngle(float radians) {
        return { std::cos(radians), std::sin(radians) };
    }
};
