#include "input.h"

namespace arx {

void Input::bind(int action,int key) { bindings[action].push_back(key); }

void Input::rebind(int action,int key) { bindings[action] = { key };}

void Input::unbind(int action) { bindings.erase(action); }

bool Input::isDown(int action) const {
    auto it = bindings.find(action);
    if (it == bindings.end()) { return false; } // not in bindings
    for (int key : it->second) {
        if(IsKeyDown(key)) { return true; }
    }
    return false;
}

bool Input::isPressed(int action) const {
    auto it = bindings.find(action);
    if (it == bindings.end()) { return false; } // not in bindings
    for (int key : it->second) {
        if(IsKeyPressed(key)) { return true; }
    }
    return false;
}

bool Input::isReleased(int action) const {
    auto it = bindings.find(action);
    if (it == bindings.end()) { return false; } // not in bindings
    for (int key : it->second) {
        if(IsKeyReleased(key)) { return true; }
    }
    return false;
}

void Input::setBindings(const std::unordered_map<int,std::vector<int>>& b) {
    bindings = b;
}

std::unordered_map<int,std::vector<int>> Input::getBindings() const { // return snapshot of current bindings, not a reference
    return bindings;
}

} // namespace arx