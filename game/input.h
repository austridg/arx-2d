#pragma once

#include <unordered_map>
#include <vector>

#include "raylib.h"

class Input {
private:
    std::unordered_map<int, std::vector<int>> bindings;
public:
    void bind(int action,int key);
    void rebind(int action,int key);
    void unbind(int action);
    bool isDown(int action) const; // action is currently held
    bool isPressed(int action) const; // action went down this frame
    bool isReleased(int action) const; // action went up this frame

    void setBindings(const std::unordered_map<int,std::vector<int>>& b);
    std::unordered_map<int,std::vector<int>> getBindings() const;
};