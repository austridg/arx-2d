#pragma once

#include <functional>
#include <vector>
#include <memory>
#include <string>
#include <algorithm>

#include "entity/entity.h"

class Input;
class Rng;
class Scene;
class PersistentRegistry;

struct SceneContext {
    Scene& scene;
    PersistentRegistry& pRegistry;
    Input& input;
    Rng& rng;
    float dt;
};

class Scene {
public:
    int id = 0;                  // scene identifier (use your own enum)

    std::function<void(SceneContext&)> onEnter, onHandleInput, onUpdate, onDraw;

    std::vector<std::unique_ptr<Entity>> entities;

    Entity* add(std::unique_ptr<Entity> e);

    void enter(SceneContext& ctx);
    void handleInput(SceneContext& ctx);
    void update(SceneContext&);
    void draw(SceneContext& ctx);
private:
    void reapDead();
};

using SceneScript = std::function<void(SceneContext&)>;