#include "game.h"

namespace arx {

/*
=== === ===
RNG
=== === ===
*/

Rng::Rng() : engine(std::random_device{}()) {}

Rng::Rng(unsigned seed) : engine(seed) {}

float Rng::range(float min,float max) {
    std::uniform_real_distribution<float> dist(min,max);
    return dist(engine);
}

int Rng::range(int min,int max) {
    std::uniform_int_distribution<int> dist(min,max);
    return dist(engine);
}

float Rng::angle() {
    float twoPi = 6.2831853f;
    return range(0.0f, twoPi);
}

bool Rng::chance(float probability) {
    return range(0.0f,1.0f) < probability;
}

/*
=== === ===
Game
=== === ===
*/

Game::Game() {
    if(!windowSetup()) { std::cerr << "[ERROR] - Raylib Setup Failed" << std::endl; return; };
}

Game::Game(int windowW,int windowH,int fps,bool fullscreen,const std::string& title)
    : windowSizeWidth(windowW),windowSizeHeight(windowH),fpsTarget(fps),startFullscreen(fullscreen),gameTitle(title) {
        if(!windowSetup()) { std::cerr << "[ERROR] - Raylib Setup Failed" << std::endl; return; };
}

bool Game::windowSetup() {

    InitWindow(windowSizeWidth,windowSizeHeight,gameTitle.c_str());
    SetTargetFPS(fpsTarget);

    if(startFullscreen) { fullscreenToggle(true); }

    if(!IsWindowReady()) { std::cerr << "[ERROR] - Window initialization failed" << std::endl; return false; }
    return true;

}

void Game::fullscreenToggle(bool on) {

    if(on && !IsWindowFullscreen()) {
        int monitor = GetCurrentMonitor();
        SetWindowSize(GetMonitorWidth(monitor), GetMonitorHeight(monitor));
        ToggleFullscreen();
    }
    else if(!on && IsWindowFullscreen()) {
       ToggleFullscreen();
       SetWindowSize(windowSizeWidth,windowSizeHeight); 
    }

}

void Game::gameSetup() {

}

void Game::run() {
    if (!active) {
        std::cerr << "[ERROR] - run(): no active scene set (call setActive first)" << std::endl;
    }

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        if (dt > 0.05f) { dt = 0.05f; }   // clamp spikes so nothing teleports

        if (active) {
            SceneContext ctx{ *active, persistent, input, rng, dt };

            active->handleInput(ctx);   // 1. input
            active->update(ctx);        // 2. update (entities + onUpdate)

            BeginDrawing();             // 3. draw
                ClearBackground(BLACK);
                active->draw(ctx);
                // Persistent entities are just a store; if a game wants them
                // simulated/drawn, its scene scripts do that via ctx.pRegistry.
            EndDrawing();
        } else {
            // no scene yet: still pump the window so it can be closed
            BeginDrawing();
                ClearBackground(BLACK);
            EndDrawing();
        }
    }

    CloseWindow();
}

Scene& Game::addScene(int id) {
    Scene& s = scenes[id];   // default-constructs a new scene if the id is new
    s.id = id;
    return s;
}

void Game::setActive(int id) {
    auto it = scenes.find(id);
    if (it == scenes.end()) {
        std::cerr << "[ERROR] - setActive(): no scene with id " << id << std::endl;
        return;
    }
    active = &it->second;

    SceneContext ctx{ *active, persistent, input, rng, 0.0f };   // dt unused outside update
    active->enter(ctx);   // run the scene's onEnter (spawn entities, bind keys, ...)
}

Rng&                           Game::getRng()       { return rng; }
Input&                         Game::getInput()     { return input; }
PersistentRegistry&            Game::getPersistent(){ return persistent; }
FunctionRegistry<SceneScript>& Game::getScripts()   { return scripts; }
FunctionRegistry<BehaviorFn>&  Game::getBehaviors() { return behaviors; }

} // namespace arx