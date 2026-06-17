#include "game.h"

#include <algorithm>

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
    InitAudioDevice();   // sound + music; paired with CloseAudioDevice() in run()

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
    if (sceneStack.empty()) {
        std::cerr << "[ERROR] - run(): no active scene set (call setActive first)" << std::endl;
    }

    // draw every scene in the stack bottom-to-top into the current render target
    auto drawStack = [&]() {
        ClearBackground(BLACK);
        for (Scene* s : sceneStack) {
            SceneContext dctx{ *s, persistent, input, rng, *this, fixedDt };
            s->draw(dctx);
        }
    };

    while (!WindowShouldClose()) {
        float frame = GetFrameTime();
        if (frame > 0.25f) { frame = 0.25f; }   // clamp huge stalls so we don't spiral

        audio.update();   // feed the active music stream

        // input once per frame (raylib's pressed/released are per render frame);
        // only the top scene updates -> an overlay freezes the scenes beneath it
        Scene* top = sceneStack.empty() ? nullptr : sceneStack.back();
        if (top) {
            SceneContext ctx{ *top, persistent, input, rng, *this, fixedDt };
            top->handleInput(ctx);
            if (!paused) {
                accumulator += frame;
                while (accumulator >= fixedDt) {
                    top->update(ctx);   // ctx.dt is fixedDt
                    accumulator -= fixedDt;
                }
            }
        }

        if (useVirtual) {
            if (!targetReady) {
                virtualTarget = LoadRenderTexture(virtualW, virtualH);
                SetTextureFilter(virtualTarget.texture, TEXTURE_FILTER_POINT);
                targetReady = true;
            }
            // letterbox-fit the virtual frame to the window; tell Input the mapping so
            // mousePosition() comes back in virtual coords
            float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
            float scale = std::min(sw / virtualW, sh / virtualH);
            float dw = virtualW * scale, dh = virtualH * scale;
            float ox = (sw - dw) * 0.5f, oy = (sh - dh) * 0.5f;
            input.setVirtualTransform({ scale, scale }, { ox, oy });

            BeginTextureMode(virtualTarget);
                drawStack();
            EndTextureMode();

            BeginDrawing();
                ClearBackground(BLACK);
                Rectangle src{ 0.0f, 0.0f, (float)virtualW, -(float)virtualH };   // flip Y (RT is upside down)
                Rectangle dst{ ox, oy, dw, dh };
                DrawTexturePro(virtualTarget.texture, src, dst, { 0.0f, 0.0f }, 0.0f, WHITE);
            EndDrawing();
        } else {
            BeginDrawing();
                drawStack();
            EndDrawing();
        }
    }

    if (targetReady) { UnloadRenderTexture(virtualTarget); }
    audio.unloadAll();
    CloseAudioDevice();
    CloseWindow();
}

void Game::setVirtualResolution(int w, int h) {
    virtualW = w; virtualH = h;
    useVirtual = (w > 0 && h > 0);
}

void Game::pause()        { paused = true; }
void Game::resume()       { paused = false; }
void Game::togglePause()  { paused = !paused; }
bool Game::isPaused() const { return paused; }
void Game::setFixedStep(float dt) { if (dt > 0.0f) { fixedDt = dt; } }

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

    // leave the current top, then replace the whole stack with the new scene
    if (!sceneStack.empty()) {
        SceneContext ctx{ *sceneStack.back(), persistent, input, rng, *this, 0.0f };
        sceneStack.back()->exit(ctx);
    }
    sceneStack.clear();

    Scene* s = &it->second;
    sceneStack.push_back(s);
    SceneContext ctx{ *s, persistent, input, rng, *this, 0.0f };   // dt unused outside update
    s->enter(ctx);   // run the scene's onEnter (spawn entities, bind keys, ...)
}

void Game::pushScene(int id) {
    auto it = scenes.find(id);
    if (it == scenes.end()) {
        std::cerr << "[ERROR] - pushScene(): no scene with id " << id << std::endl;
        return;
    }
    Scene* s = &it->second;
    sceneStack.push_back(s);                                  // scenes beneath are left intact
    SceneContext ctx{ *s, persistent, input, rng, *this, 0.0f };
    s->enter(ctx);
}

void Game::popScene() {
    if (sceneStack.empty()) { return; }
    SceneContext ctx{ *sceneStack.back(), persistent, input, rng, *this, 0.0f };
    sceneStack.back()->exit(ctx);   // the revealed scene below is NOT re-entered (preserved)
    sceneStack.pop_back();
}

Rng&                           Game::getRng()       { return rng; }
Input&                         Game::getInput()     { return input; }
Audio&                         Game::getAudio()     { return audio; }
Blackboard&                    Game::getState()     { return state; }
PersistentRegistry&            Game::getPersistent(){ return persistent; }
FunctionRegistry<SceneScript>& Game::getScripts()   { return scripts; }
FunctionRegistry<BehaviorFn>&  Game::getBehaviors() { return behaviors; }

} // namespace arx