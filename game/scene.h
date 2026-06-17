#pragma once

#include <functional>
#include <vector>
#include <memory>
#include <string>
#include <algorithm>
#include <optional>

#include "entity/entity.h"
#include "camera.h"          // Camera (complete type: it's an optional<> member below)

namespace arx {

class Input;
class Rng;
class Scene;
class PersistentRegistry;
class Game;

struct SceneContext {
    Scene& scene;
    PersistentRegistry& pRegistry;
    Input& input;
    Rng& rng;
    Game& game;    // the owning Game: pause(), pushScene/popScene, setActive, getAudio/getState, ...
    float dt;
};

class Scene {
public:
    int id = 0;                  // scene identifier (use your own enum)

    std::function<void(SceneContext&)> onEnter, onHandleInput, onUpdate, onDraw;
    std::function<void(SceneContext&)> onExit;        // cleanup/save when the scene is left or popped
    std::function<void(SceneContext&)> onDrawWorld;   // world-space extras, drawn under the camera

    // Optional view. No camera -> entities draw in screen space (the original
    // behavior). With one, the world pass is bracketed in BeginMode2D/EndMode2D and
    // `onDraw` stays screen-space for the HUD. The game points it (see Camera).
    std::optional<Camera> camera;

    bool debugDraw = false;   // overlay collision shapes (world) + FPS (screen)

    std::vector<std::unique_ptr<Entity>> entities;   // scene-owned: born and reaped with the scene

    // Borrowed participants: persistent (or otherwise externally-owned) entities that
    // take part fully in THIS scene -- updated, collided, and drawn alongside the
    // owned entities. The PersistentRegistry owns their lifetime; the scene only
    // points at them. Rebuilt every activation (see enter()).
    std::vector<Entity*> attached;

    Entity* add(std::unique_ptr<Entity> e);   // hand the scene a new entity it will own
    void    attach(Entity* e);                // borrow an externally-owned entity as a full participant

    // queries over this scene's participants (owned + attached), filtered by id
    Entity*              find(int id);                          // first match, or nullptr
    std::vector<Entity*> findAll(int id);
    Entity*              nearest(Vec2 from, int id);            // closest match, or nullptr
    std::vector<Entity*> inRadius(Vec2 center, float r, int id);

    void enter(SceneContext& ctx);   // clears entities/attached, then runs onEnter
    void exit(SceneContext& ctx);    // runs onExit
    void handleInput(SceneContext& ctx);
    void update(SceneContext&);
    void draw(SceneContext& ctx);
private:
    void resolveCollisions(SceneContext& ctx);   // pairwise circle overlap -> entity responders
    void reapDead();
};

using SceneScript = std::function<void(SceneContext&)>;

} // namespace arx