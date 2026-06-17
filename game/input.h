#pragma once

#include <unordered_map>
#include <vector>

#include "raylib.h"
#include "entity/vec2.h"

namespace arx {

// Where a bound control physically lives. raylib's raw codes overlap across these
// (key 'A' and mouse-left are both small ints), so every binding records its source.
enum class InputSource { Key, MouseButton, GamepadButton };

struct Binding {
    InputSource source;
    int code;
};

// Maps abstract actions (your own enum) to physical controls. Multiple binds on one
// action stack as "any of" -- e.g. bind JUMP to both KEY_SPACE and a gamepad button.
class Input {
private:
    std::unordered_map<int, std::vector<Binding>> bindings;
    int gamepad = 0;            // which gamepad the gamepad bindings/axes read from
    Vec2 vScale{ 1.0f, 1.0f };  // virtual-resolution mapping applied to mousePosition()
    Vec2 vOffset{ 0.0f, 0.0f };
public:
    // bind() defaults to keyboard (the common case, and back-compatible with old code)
    void bind(int action, int key);
    void bindKey(int action, int key);
    void bindMouse(int action, int button);          // MOUSE_BUTTON_*
    void bindGamepadButton(int action, int button);  // GAMEPAD_BUTTON_*

    void rebind(int action, int key);   // replace every bind on action with one key
    void unbind(int action);

    bool isDown(int action) const;      // held this frame
    bool isPressed(int action) const;   // went down this frame
    bool isReleased(int action) const;  // went up this frame

    // --- direct device reads (not action-mapped) ---
    Vec2  mousePosition() const;
    Vec2  mouseDelta() const;
    float mouseWheel() const;
    bool  gamepadAvailable() const;
    float gamepadAxis(int axis) const;  // GAMEPAD_AXIS_*, roughly [-1, 1]
    void  setGamepad(int id);

    // set by Game when virtual resolution is on, so mousePosition() returns virtual
    // (game-space) coordinates instead of raw window pixels.
    void  setVirtualTransform(Vec2 scale, Vec2 offset);

    void setBindings(const std::unordered_map<int, std::vector<Binding>>& b);
    std::unordered_map<int, std::vector<Binding>> getBindings() const;
};

} // namespace arx
