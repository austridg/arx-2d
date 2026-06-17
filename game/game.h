#pragma once

#include <iostream>
#include <string>
#include <random>
#include <unordered_map>
#include <vector>
#include <functional>

#include "input.h"
#include "scene.h"               // Scene + SceneContext (pulls in entity.h)
#include "function_registry.h"
#include "audio.h"               // Audio: sound + music
#include "blackboard.h"          // Blackboard: global key/value state + save/load
#include "entity/p_registry.h"   // PersistentRegistry

namespace arx {

class Rng {
private:
    std::mt19937 engine;
public:
    Rng();

    explicit Rng(unsigned seed);

    float range(float min, float max);
    int range(int min, int max);
    float angle();
    bool chance(float probability);
};

class Game {
private:
    int windowSizeWidth = 1280, windowSizeHeight = 720;
    int fpsTarget = 60;
    bool startFullscreen = false;

    std::string gameTitle = "2d-ge";

    Rng rng;
    Input input;
    Audio audio;
    Blackboard state;
    PersistentRegistry persistent;

    // fixed-timestep loop state: update() runs in whole `fixedDt` steps so physics
    // is framerate-independent and deterministic, regardless of render fps.
    float fixedDt = 1.0f / 60.0f;
    float accumulator = 0.0f;
    bool  paused = false;

    // optional virtual resolution: render the scenes into a fixed-size target, then
    // scale it (letterboxed) to the window. Off by default -> native window coords.
    int  virtualW = 0, virtualH = 0;
    bool useVirtual = false;
    RenderTexture2D virtualTarget {};
    bool targetReady = false;

    // scene storage: scenes by id + the one currently running. unordered_map
    // keeps references/pointers to its elements valid across inserts, so `active`
    // stays good even as more scenes get added.
    std::unordered_map<int, Scene> scenes;

    // scene stack: the top is the active scene (updates + takes input); every scene in
    // the stack still draws, bottom-to-top, so an overlay (pause/inventory) shows the
    // frozen scene behind it. setActive replaces the stack; push/pop layer onto it.
    std::vector<Scene*> sceneStack;

    // central libraries of reusable, named functions (empty until you fill them)
    FunctionRegistry<SceneScript> scripts;     // reusable scene hooks
    FunctionRegistry<BehaviorFn>  behaviors;   // reusable entity behaviors

public:
    Game(); // create game (default variables)
    Game(int windowW,int windowH,int fps,bool fullscreen,const std::string& title);

    bool windowSetup();
    void fullscreenToggle(bool on);
    void gameSetup();

    // scenes
    Scene& addScene(int id);    // register (or fetch) a scene by its id (use your own enum)
    void setActive(int id);     // replace the stack with this scene; exits old top, enters new
    void pushScene(int id);     // overlay a scene on top (scenes beneath keep drawing, frozen)
    void popScene();            // remove the top scene, revealing the one beneath (preserved)

    void run();

    // pause freezes the fixed-step update (drawing + input still run, so a pause
    // menu can react and unpause).
    void pause();
    void resume();
    void togglePause();
    bool isPaused() const;

    void setFixedStep(float dt);   // seconds per update step (default 1/60)

    // render at a fixed internal resolution, scaled to the window (great for pixel art).
    // 0/0 (default) disables it. With it on, lay out in virtual coords and read the mouse
    // via Input::mousePosition() (it returns virtual coords).
    void setVirtualResolution(int w, int h);

    // accessors
    Rng& getRng();
    Input& getInput();
    Audio& getAudio();
    Blackboard& getState();
    PersistentRegistry& getPersistent();
    FunctionRegistry<SceneScript>& getScripts();
    FunctionRegistry<BehaviorFn>& getBehaviors();
};

} // namespace arx
