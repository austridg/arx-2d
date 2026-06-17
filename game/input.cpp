#include "input.h"

namespace arx {

namespace {
// How we're querying a control this frame.
enum class Phase { Down, Pressed, Released };

bool test(const Binding& b, int gamepad, Phase p) {
    switch (b.source) {
        case InputSource::Key:
            return p == Phase::Down     ? IsKeyDown(b.code)
                 : p == Phase::Pressed  ? IsKeyPressed(b.code)
                                        : IsKeyReleased(b.code);
        case InputSource::MouseButton:
            return p == Phase::Down     ? IsMouseButtonDown(b.code)
                 : p == Phase::Pressed  ? IsMouseButtonPressed(b.code)
                                        : IsMouseButtonReleased(b.code);
        case InputSource::GamepadButton:
            return p == Phase::Down     ? IsGamepadButtonDown(gamepad, b.code)
                 : p == Phase::Pressed  ? IsGamepadButtonPressed(gamepad, b.code)
                                        : IsGamepadButtonReleased(gamepad, b.code);
    }
    return false;
}
} // namespace

void Input::bind(int action, int key)               { bindKey(action, key); }
void Input::bindKey(int action, int key)            { bindings[action].push_back({ InputSource::Key, key }); }
void Input::bindMouse(int action, int button)       { bindings[action].push_back({ InputSource::MouseButton, button }); }
void Input::bindGamepadButton(int action, int btn)  { bindings[action].push_back({ InputSource::GamepadButton, btn }); }

void Input::rebind(int action, int key) { bindings[action] = { { InputSource::Key, key } }; }
void Input::unbind(int action)          { bindings.erase(action); }

bool Input::isDown(int action) const {
    auto it = bindings.find(action);
    if (it == bindings.end()) { return false; }
    for (const Binding& b : it->second) { if (test(b, gamepad, Phase::Down)) { return true; } }
    return false;
}

bool Input::isPressed(int action) const {
    auto it = bindings.find(action);
    if (it == bindings.end()) { return false; }
    for (const Binding& b : it->second) { if (test(b, gamepad, Phase::Pressed)) { return true; } }
    return false;
}

bool Input::isReleased(int action) const {
    auto it = bindings.find(action);
    if (it == bindings.end()) { return false; }
    for (const Binding& b : it->second) { if (test(b, gamepad, Phase::Released)) { return true; } }
    return false;
}

Vec2  Input::mousePosition() const {
    Vector2 m = GetMousePosition();
    float sx = vScale.x != 0.0f ? vScale.x : 1.0f;
    float sy = vScale.y != 0.0f ? vScale.y : 1.0f;
    return { (m.x - vOffset.x) / sx, (m.y - vOffset.y) / sy };
}
Vec2  Input::mouseDelta() const    { Vector2 m = GetMouseDelta();    return { m.x / (vScale.x != 0.0f ? vScale.x : 1.0f), m.y / (vScale.y != 0.0f ? vScale.y : 1.0f) }; }
float Input::mouseWheel() const    { return GetMouseWheelMove(); }
bool  Input::gamepadAvailable() const         { return IsGamepadAvailable(gamepad); }
float Input::gamepadAxis(int axis) const       { return GetGamepadAxisMovement(gamepad, axis); }
void  Input::setGamepad(int id)                { gamepad = id; }
void  Input::setVirtualTransform(Vec2 scale, Vec2 offset) { vScale = scale; vOffset = offset; }

void Input::setBindings(const std::unordered_map<int, std::vector<Binding>>& b) { bindings = b; }
std::unordered_map<int, std::vector<Binding>> Input::getBindings() const { return bindings; }

} // namespace arx
