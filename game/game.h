#pragma once

#include <iostream>
#include <string>
#include <random>
#include <unordered_map>
#include <functional>

#include "input.h"
#include "scene.h"               // Scene + SceneContext (pulls in entity.h)
#include "function_registry.h"
#include "entity/p_registry.h"   // PersistentRegistry

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
    PersistentRegistry persistent;

    // scene storage: scenes by id + the one currently running. unordered_map
    // keeps references/pointers to its elements valid across inserts, so `active`
    // stays good even as more scenes get added.
    std::unordered_map<int, Scene> scenes;
    Scene* active = nullptr;

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
    void setActive(int id);     // switch the running scene; runs its onEnter

    void run();

    // accessors
    Rng& getRng();
    Input& getInput();
    PersistentRegistry& getPersistent();
    FunctionRegistry<SceneScript>& getScripts();
    FunctionRegistry<BehaviorFn>& getBehaviors();
};
