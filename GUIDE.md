# 2d-ge — Engine Guide

A from-scratch 2D game engine in the `arx` namespace, built on top of **rad2d** (a
raylib 5.5 drawing/animation wrapper). rad2d gives you drawing, sprites, animations,
tiles, text, and asset registries. **2d-ge** adds everything a game needs around that:
the window + game loop, input, entities, behaviors, collision, scenes, a camera, audio,
timers, save state, and more.

This document is the complete reference for building games on it. Read sections 1–4
once, then keep the rest as a lookup.

---

## Table of contents

1. [The mental model](#1-the-mental-model)
2. [Build & run](#2-build--run)
3. [The 60-second skeleton](#3-the-60-second-skeleton)
4. [The frame lifecycle](#4-the-frame-lifecycle) — *read this; it explains the ordering everything depends on*
5. [Core types](#5-core-types)
   - [Game](#game) · [Scene & SceneContext](#scene--scenecontext) · [Entity](#entity) · [Vec2](#vec2) · [Behaviors](#behaviors) · [Collision](#collision) · [Components](#components) · [PersistentRegistry & attach](#persistentregistry--attach)
6. [Subsystems](#6-subsystems)
   - [Input](#input) · [Rng](#rng) · [Audio](#audio) · [Camera](#camera) · [Timing](#timing-timer--interval--tween) · [Blackboard](#blackboard-global-state--saveload) · [FunctionRegistry](#functionregistry) · [Tile collision](#tile-collision) · [Virtual resolution](#virtual-resolution) · [Text](#text)
7. [The rad2d layer](#7-the-rad2d-layer) — drawables, animations, asset registries, tiles
8. [Recurring patterns](#8-recurring-patterns)
9. [Genre playbooks](#9-genre-playbooks) — Asteroids, Platformer, ARPG
10. [Gotchas](#10-gotchas)
11. [API cheat sheet](#11-api-cheat-sheet)

---

## 1. The mental model

The engine is built on **composition, not inheritance**. You never subclass an
`Entity` or a `Scene`. Instead you compose them from small, swappable pieces:

- An **Entity** is a plain bag of data: a transform, an optional visual, an optional
  collision shape, a list of **behaviors** (per-frame logic), a list of **collision
  responders**, and a set of **components** (typed state). An entity "type" (a ship,
  an asteroid, a bullet) is just a **factory function** that builds one — there is no
  `class Ship`.
- A **Scene** is a bag of `std::function` **hooks** (`onEnter`, `onUpdate`, …) plus
  the entities it owns. You don't subclass it; you assign lambdas to its hooks.
- **Genres differ by which behaviors and components an entity carries.** The same
  `Entity` becomes a top-down ship or a platformer hero depending on whether it has a
  thrust behavior or a gravity behavior.

Two more principles:

- **Engine detects, game decides.** The engine finds collisions; *your* code (a
  collision responder) decides what happens. The engine fires scene hooks; your
  lambdas decide the game.
- **Everything is keyed by `int`, and you bring your own `enum`s.** Scene ids, entity
  ids, input actions, texture/animation/font/sound ids — all plain ints. You define
  enums for them so the numbers read as names.

```cpp
enum SceneId { SCENE_MENU, SCENE_PLAY };
enum EntId   { E_PLAYER, E_ENEMY, E_BULLET };
enum Action  { MOVE_LEFT, MOVE_RIGHT, JUMP, FIRE };
enum TexId   { T_HERO, T_TILES };
```

Everything lives in `namespace arx`. Your game code (`app.cpp`) is *not* in the
namespace; it pulls it in:

```cpp
#include "engine.hpp"   // the umbrella header: the whole engine + rad2d
using namespace arx;    // arx::Game -> Game, etc. (rad2d:: stays qualified)
```

---

## 2. Build & run

```bash
cmake --build build       # builds rad2d, the engine, and your app (auto-globs new .cpp files)
./build/app               # run FROM THE REPO ROOT so "assets/..." paths resolve
# or in one go:
cmake --build build && ./build/app
```

- `build/` is already configured. To start clean: `cmake -B build && cmake --build build`.
- New `.cpp` files under `game/`, `entity/`, `draw/` are picked up automatically
  (CMake globs with `CONFIGURE_DEPENDS`). Your game itself lives in `app.cpp`.
- rad2d is compiled from source (`../animation`), so edits there flow straight in.

---

## 3. The 60-second skeleton

Every game has the same shape: make a `Game`, load assets, register reusable
functions, define scene hooks, set the first scene, `run()`.

```cpp
#include "engine.hpp"
using namespace arx;

enum SceneId { SCENE_PLAY };
enum EntId   { E_PLAYER };
enum Action  { MOVE_LEFT, MOVE_RIGHT };

int main() {
    Game game(1280, 720, 60, /*fullscreen*/false, "My Game");

    // 1. assets (AFTER the Game ctor — it created the window + GL/audio device)
    rad2d::TextureRegistry tex;
    tex.load(0, "assets/hero.png");

    // 2. input bindings
    Input& in = game.getInput();
    in.bind(MOVE_LEFT,  KEY_A);
    in.bind(MOVE_RIGHT, KEY_D);

    // 3. reusable behaviors (stateless, shared by many entities)
    game.getBehaviors().add("integrate", [](Entity& self, float dt) {
        self.position += self.velocity * dt;
    });

    // 4. a scene
    Scene& play = game.addScene(SCENE_PLAY);
    play.onEnter = [&tex](SceneContext& ctx) {
        auto e = std::make_unique<Entity>(E_PLAYER);
        e->position = { 640, 360 };
        auto spr = std::make_unique<rad2d::Sprite>("hero", nullptr, 0,0,0, 64,64);
        spr->setTexture(tex.get(0));
        // (a sprite needs an animation to draw a frame — see the rad2d section)
        e->drawable = std::move(spr);
        e->addBehavior(ctx.game.getBehaviors().get("integrate"));
        ctx.scene.add(std::move(e));
    };
    play.onHandleInput = [](SceneContext& ctx) {
        if (Entity* p = ctx.scene.find(E_PLAYER)) {
            p->velocity.x = 0;
            if (ctx.input.isDown(MOVE_LEFT))  p->velocity.x = -200;
            if (ctx.input.isDown(MOVE_RIGHT)) p->velocity.x =  200;
        }
    };

    // 5. go
    game.setActive(SCENE_PLAY);
    game.run();
    return 0;
}
```

> The single most complete worked example is the Asteroids game in **`app.cpp`** — it
> exercises nearly every system. Read it alongside this guide.

---

## 4. The frame lifecycle

This is the contract everything depends on. Understand it once.

### The loop (in `Game::run`)
Each rendered frame, in order:

1. **`audio.update()`** — feeds the active music stream.
2. **`handleInput`** is called **once** on the *top* scene. (raylib's "pressed/released"
   are per render frame, so input is read once per frame.)
3. **`update`** runs in **fixed timesteps** on the top scene. The accumulator banks
   real elapsed time and drains it in `1/60`s steps, so `update` may run **0, 1, or
   several times** per rendered frame. `ctx.dt` is **always the fixed step** (`1/60`).
   *If the game is paused, the update steps are skipped (input + draw still run).*
4. **draw** — every scene in the stack is drawn **bottom-to-top** (so overlays sit
   over the scenes beneath them).

**Consequence:** put input→entity logic in `onHandleInput` (runs once/frame, can read
`isPressed`). Put simulation in behaviors / `onUpdate` (run on the fixed step, use
`dt`). Don't read `isPressed` inside a behavior — it may run several times or zero
times in a frame.

### Inside `Scene::update`
1. Every owned entity's behaviors run, then every **attached** entity's.
2. **Collision** is resolved (pairwise; responders fire).
3. **`onUpdate`** (your per-frame scene logic).
4. The scene's **camera shake** is advanced.
5. **Dead entities are reaped** (`alive == false`). Reaping is last, so things killed
   by a collision this frame are cleaned up the same frame.

### Inside `Scene::draw`
1. All drawable entities (owned + attached) are gathered and **z-sorted by layer**.
2. **World pass** — if the scene has a camera, wrapped in its transform: entities draw,
   then `onDrawWorld`, then debug collision shapes (if `debugDraw`).
3. **Screen pass** — `onDraw` (your HUD), then the debug FPS counter (if `debugDraw`).

So: **world-space things** (the game world) go through entity drawables or
`onDrawWorld`; **screen-space things** (HUD, score, cursor) go in `onDraw`. The camera
only moves the world pass.

---

## 5. Core types

### Game

Owns the window, the loop, and all the global services. Construct it first — its
constructor opens the window and initializes the GL + audio devices, so **load all
assets after it exists.**

```cpp
Game game;                                              // 1280x720 @60, windowed, "2d-ge"
Game game(1280, 720, 60, false, "Title");               // explicit
```

| Method | Purpose |
|---|---|
| `addScene(int id) -> Scene&` | register (or fetch) a scene; assign its hooks on the returned ref |
| `setActive(int id)` | replace the scene stack with this scene (exits the old top, enters the new) |
| `pushScene(int id)` | overlay a scene on top (scenes beneath keep drawing, frozen) |
| `popScene()` | remove the top scene, revealing the one beneath (preserved, not re-entered) |
| `run()` | start the loop (blocks until the window closes) |
| `pause() / resume() / togglePause() / isPaused()` | freeze the fixed-step update (input + draw still run) |
| `setFixedStep(float dt)` | seconds per update step (default `1/60`) |
| `setVirtualResolution(int w, int h)` | render at a fixed internal size, scaled to the window (see [Virtual resolution](#virtual-resolution)) |
| `getRng() / getInput() / getAudio() / getState() / getPersistent()` | the global services |
| `getScripts() / getBehaviors()` | the two function registries |

### Scene & SceneContext

A scene is its **hooks** + its **entities** + an optional **camera**. You never subclass
it; you assign lambdas. All hooks are `std::function<void(SceneContext&)>`.

```cpp
Scene& s = game.addScene(SCENE_PLAY);
s.onEnter       = [](SceneContext& ctx){ /* spawn the world, bind keys, load save */ };
s.onExit        = [](SceneContext& ctx){ /* cleanup / save on leave or pop */ };
s.onHandleInput = [](SceneContext& ctx){ /* read input -> set entity intent */ };
s.onUpdate      = [](SceneContext& ctx){ /* per-frame game logic: spawning, win/lose */ };
s.onDraw        = [](SceneContext& ctx){ /* screen-space HUD */ };
s.onDrawWorld   = [](SceneContext& ctx){ /* world-space extras, under the camera */ };
s.camera        = Camera::centered();   // optional; omit for screen-space games
s.debugDraw     = true;                 // overlay collision shapes + FPS
```

**Hook timing:** `onEnter` runs on activation/push (after `entities` and `attached`
are cleared). `onExit` runs on switch/pop — **not** when the window closes. `onHandleInput`
once/frame; `onUpdate` per fixed step; `onDraw`/`onDrawWorld` once/frame.

**Owned entities & queries:**

```cpp
Entity* e = ctx.scene.add(std::make_unique<Entity>(E_BULLET));  // scene owns it; reaped with the scene
Entity* p = ctx.scene.find(E_PLAYER);                           // first with this id (owned or attached)
std::vector<Entity*> all = ctx.scene.findAll(E_ENEMY);
Entity* near = ctx.scene.nearest(p->position, E_ENEMY);
auto hits = ctx.scene.inRadius(blastPos, 120.0f, E_ENEMY);      // everything of an id within a radius
```

**SceneContext** is the handle every hook receives — your gateway to everything:

```cpp
struct SceneContext {
    Scene& scene;                 // the scene being processed
    PersistentRegistry& pRegistry;// long-lived entities
    Input& input;                 // input.isDown(...), input.mousePosition()
    Rng& rng;                     // rng.range(...), rng.chance(...)
    Game& game;                   // game.pushScene(...), game.getAudio(), game.getState(), ...
    float dt;                     // the fixed step (1/60)
};
```

> Because `ctx.game` is here, scene hooks can change scenes, pause, play audio, and
> read/write global state. (Behaviors only get `(self, dt)` — see below.)

### Entity

A "thing in the world." Pure data + lists of logic. An entity with no drawable is a
fine **invisible logic object** (a spawner, a trigger, a score tracker).

```cpp
class Entity {
    int id;                                   // your enum; also the key for find()/registry

    Vec2  position, velocity;                 // the simulation's truth (floats)
    float rotation;                           // RADIANS (pushed to the drawable as degrees)

    enum class Shape { Circle, Box };         // collision shape, centered on position
    Shape shape = Shape::Circle;
    float radius;                             // Circle: 0 = non-colliding
    Vec2  halfExtents;                        // Box: half width/height; (0,0) = non-colliding
    bool  collidable() const;                 // does it collide at all?

    bool  alive = true;                       // set false -> reaped by its owner

    std::unique_ptr<rad2d::Drawable> drawable;// optional visual (null = invisible)

    std::vector<BehaviorFn>  behaviors;       // per-frame logic
    std::vector<CollisionFn> collisionResponders;
    // + a typed component store (see Components)

    void addBehavior(BehaviorFn);
    void addCollisionResponder(CollisionFn);
};
```

Position is the **center** (sprites draw centered and rotate about their center, and
collision shapes are centered on it). Rotation is in radians; the engine converts to
degrees for the drawable.

You rarely construct an `Entity` inline — you write a **factory function** (see
[Patterns](#8-recurring-patterns)).

### Vec2

The 2D vector for positions, velocities, directions.

```cpp
Vec2 a{3, 4};
a + b;  a - b;  a * 2.0f;  a += b;  a -= b;     // arithmetic
a.length();                                      // magnitude
a.normalized();                                  // unit vector (zero stays zero)
Vec2::fromAngle(radians);                         // unit vector at an angle
```

### Behaviors

A **behavior** is one piece of per-frame logic on an entity:

```cpp
using BehaviorFn = std::function<void(Entity& self, float dt)>;
```

Stateless behaviors (movement integration, screen-wrap, despawn-offscreen) belong in
the **behavior registry** so every entity can share one instance:

```cpp
game.getBehaviors().add("integrate", [](Entity& self, float dt){
    self.position += self.velocity * dt;
});
// later, in a factory:
e->addBehavior(deps.beh.get("integrate"));
```

Stateful per-entity logic reads from a **component** (below). **Behaviors only get
`(self, dt)`** — no scene, no input, no rng. So:
- read input in `onHandleInput` and write entity intent (velocity, a component flag);
- if a behavior needs the scene/rng/spawning, do that work in `onUpdate` or a collision
  responder instead (both receive `ctx`).

### Collision

**Detection** is the engine's job; **response** is yours, per entity.

Give an entity a shape:

```cpp
e->shape = Entity::Shape::Circle;  e->radius = 16;            // circle
e->shape = Entity::Shape::Box;     e->halfExtents = {12, 20}; // AABB box
```

The engine sweeps every collidable pair each frame (circle/circle, box/box, circle/box)
and, on overlap, calls **both** entities' responders (symmetric — each reacts from its
own point of view):

```cpp
using CollisionFn = std::function<void(Entity& self, Entity& other, SceneContext& ctx)>;

bullet->addCollisionResponder([](Entity& self, Entity& other, SceneContext&){
    if (other.id == E_ENEMY) self.alive = false;            // bullet dies on impact
});
enemy->addCollisionResponder([](Entity& self, Entity& other, SceneContext& ctx){
    if (other.id == E_BULLET) {
        if (auto* h = self.get<Health>()) { h->hp--; if (h->hp <= 0) self.alive = false; }
    }
});
```

Notes:
- Responders get `ctx`, so they can spawn (`ctx.scene.add(...)`), read `ctx.rng`, play
  audio (`ctx.game.getAudio()`), etc. **Spawning during a collision is safe.**
- An entity that died earlier this frame won't generate more collisions (so one bullet
  kills exactly one enemy).
- Filtering ("should these two interact?") is up to you, via `other.id` checks.
- **Tile collision** (vs a tilemap) is separate — see [Tile collision](#tile-collision).
- The broadphase is O(n²) — fine into the hundreds of entities. It can be upgraded to a
  grid later with **zero change to your responder code**.

### Components

Typed, queryable per-entity state — health, stats, inventory, an AI blackboard. This is
how a behavior and a collision responder on the same entity **share state** without
manual plumbing.

```cpp
struct Health { int hp = 100; };

e->addComponent<Health>();              // default-constructed
e->addComponent<Health>(50);            // constructor args forwarded
Health* h = e->get<Health>();           // nullptr if absent
if (e->has<Health>()) { ... }
e->removeComponent<Health>();
```

```cpp
// a behavior and a responder share the same Health, with no captures:
e->addBehavior([](Entity& self, float){ if (auto* h = self.get<Health>(); h && h->hp <= 0) self.alive = false; });
e->addCollisionResponder([](Entity& self, Entity& o, SceneContext&){ if (o.id==E_HAZARD) self.get<Health>()->hp -= 10; });
```

### PersistentRegistry & attach

`Scene::entities` are born and die with the scene. For entities that must **outlive a
scene** (the player walking between rooms, a boss that persists), use the
**PersistentRegistry** (`game.getPersistent()` / `ctx.pRegistry`):

```cpp
game.getPersistent().add(makePlayer(deps));   // registry owns it across scene swaps
Entity* p = ctx.pRegistry.find(E_PLAYER);
```

A persistent entity is just *stored* there — it isn't updated, drawn, or collided until
a scene **attaches** it as a participant:

```cpp
play.onEnter = [](SceneContext& ctx){
    ctx.scene.attach(ctx.pRegistry.find(E_PLAYER));   // now a full participant of THIS scene
};
```

Once attached it's updated, collided, and z-sorted/drawn alongside the scene's own
entities — but the registry still owns its lifetime. Lifetime is handled for you:
`enter()` rebuilds the attach list each activation, and dead attached entities are
dropped (not deleted) during reaping.

---

## 6. Subsystems

### Input

Maps abstract **actions** (your enum) to physical controls. A control can be a keyboard
key, a mouse button, or a gamepad button — bind several to one action for "any of".

```cpp
Input& in = game.getInput();
in.bind(JUMP, KEY_SPACE);                       // bind() = keyboard
in.bindKey(JUMP, KEY_W);                         // another key for the same action
in.bindMouse(FIRE, MOUSE_BUTTON_LEFT);
in.bindGamepadButton(JUMP, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
in.rebind(JUMP, KEY_UP);                          // replace all binds for JUMP
in.unbind(FIRE);
```

Query (in `onHandleInput`):

```cpp
ctx.input.isDown(JUMP);       // held this frame
ctx.input.isPressed(FIRE);    // went down this frame
ctx.input.isReleased(JUMP);   // went up this frame

Vec2  m  = ctx.input.mousePosition();   // (virtual coords if virtual resolution is on)
Vec2  md = ctx.input.mouseDelta();
float w  = ctx.input.mouseWheel();
float ax = ctx.input.gamepadAxis(GAMEPAD_AXIS_LEFT_X);   // ~[-1, 1]
bool  gp = ctx.input.gamepadAvailable();
```

> Prefer `ctx.input.mousePosition()` over raylib's `GetMousePosition()` — it accounts
> for virtual resolution. Note **ESC is raylib's default window-close key**; use another
> key for pause/back (or call `SetExitKey(KEY_NULL)`).

### Rng

```cpp
Rng& r = ctx.rng;                  // or game.getRng()
r.range(0.0f, 1.0f);               // float in [min, max]
r.range(0, 5);                     // int in [min, max]
r.angle();                         // radians in [0, 2π)
r.chance(0.25f);                   // true 25% of the time
Rng seeded(12345);                 // deterministic, if you make your own
```

### Audio

Owned by `Game`; device opens/closes with the window; the loop feeds music every frame.
Load **after** the `Game` exists.

```cpp
Audio& a = game.getAudio();
a.loadSound(S_SHOOT, "assets/shoot.wav");
a.loadMusic(M_LEVEL, "assets/level.ogg");

a.playSound(S_SHOOT);                 // optional: volume, pitch -> playSound(id, 0.8f, 1.2f)
a.playMusic(M_LEVEL, /*loop*/true);   // one track at a time; switching stops the old
a.setMusicVolume(0.5f);
a.stopMusic();
```

From a script: `ctx.game.getAudio().playSound(S_SHOOT);`. From a collision responder,
same — responders have `ctx`.

### Camera

A 2D view, optional per scene. Without one, the world draws in screen space. With one,
the world pass is wrapped in its transform and **`onDraw` stays screen-space** (so your
HUD never moves). The **game drives it** (you point it); the engine applies it.

```cpp
scene.camera = Camera::centered();   // offset = screen center -> target shows in the middle

// in onUpdate, follow the player:
auto& cam = *ctx.scene.camera;
cam.follow(*player);                          // snap
cam.followSmooth(*player, ctx.dt, 5.0f);      // eased/lazy
cam.clampToBounds({0,0}, {levelW, levelH}, {1280, 720});  // keep the view inside the level
cam.shake(8.0f, 0.3f);                         // magnitude px, seconds (auto-decays; ticked by the scene)
cam.zoom = 1.5f;  cam.rotation = 0.0f;         // raw knobs

Vec2 world = cam.screenToWorld(ctx.input.mousePosition());  // mouse picking in world space
Vec2 scr   = cam.worldToScreen(enemyPos);
```

### Timing (Timer / Interval / Tween)

Header-only helpers (`game/timing.h`) — they replace hand-rolled `float` countdowns.

```cpp
Timer t;                       // one-shot
t.start(0.5f);
if (t.tick(ctx.dt)) { /* fires once, on the frame it elapses */ }

Interval spawner(2.0f);        // repeating
int fires = spawner.tick(ctx.dt);   // how many periods elapsed this frame (usually 0 or 1)
if (fires > 0) spawnEnemy();

Tween fade;                    // interpolate a value over time
fade.start(0.0f, 1.0f, 0.4f, ease::outQuad);
fade.tick(ctx.dt);
float alpha = fade.value();    // eased 0->1 over 0.4s

// easing curves: ease::linear, inQuad, outQuad, inOutQuad, inCubic, outCubic, outBack
```

Keep a `Timer`/`Interval` as a captured `shared_ptr` in a script, or (better) as a
field on a **component** so it lives with the entity.

### Blackboard (global state + save/load)

`game.getState()` / `ctx.game.getState()` — a typed key→value store for non-entity
global state (score, settings, unlocks, save data), persistable to a plain text file.

```cpp
Blackboard& s = ctx.game.getState();
s.setInt("score", 0);
s.setInt("score", s.getInt("score") + 100);
s.setFloat("volume", 0.5f);
s.setBool("tutorialDone", true);
s.setString("playerName", "Ada");

int score = s.getInt("score", /*default*/0);
bool done = s.getBool("tutorialDone", false);

s.save("save.txt");   // writes key=value lines (run from repo root -> file lands there)
s.load("save.txt");   // replaces contents; returns false if the file is missing
```

> `onExit` does **not** fire when the window closes, so don't rely on it to save on
> quit. Save at meaningful moments (e.g. when a new high score is set).

### FunctionRegistry

Named libraries of reusable callables. `Game` keeps two: **scripts** (scene hooks) and
**behaviors** (entity logic).

```cpp
game.getScripts().add("playEnter", [](SceneContext& ctx){ ... });
play.onEnter = game.getScripts().get("playEnter");   // get() returns a copy; empty if missing

game.getBehaviors().add("integrate", [](Entity& self, float dt){ self.position += self.velocity * dt; });
e->addBehavior(game.getBehaviors().get("integrate"));
```

Use it for things you want to define once and reuse across many scenes/entities. (A
behavior that needs per-entity state can't be shared this way — use a component, or a
factory that builds a fresh closure.)

### Tile collision

rad2d draws tilemaps; this resolves an entity's **AABB against solid tiles** (lands on
floors, stops at walls). You describe the grid; the engine pushes the entity out.

```cpp
TileGrid grid;
grid.tileSize = 32;
grid.origin   = {0, 0};                 // world pos of cell (0,0)'s top-left
grid.cols = 100; grid.rows = 30;        // 0 = unbounded
grid.tileAt  = [&](int c, int r){ return tilemap.getTile(c, r); };   // your rad2d::Tilemap
grid.isSolid = [](int id){ return id != 0; };                       // which ids block

// after integrating the entity (e.g. in onUpdate, for each solid mover):
resolveTileCollision(*player, grid);    // pushes out of overlaps, zeroes blocked velocity
```

Use a **Box** shape (`halfExtents`) for platformer/ARPG movers. A Circle is treated as a
square AABB of side `2*radius`. This is a basic least-penetration resolver — great for
tile platformers; for slopes/one-way platforms you'd extend it.

### Virtual resolution

Render at a fixed internal size and letterbox-scale it to the window — ideal for pixel
art. **Off by default** (native window coordinates).

```cpp
game.setVirtualResolution(320, 180);   // design at 320x180; it scales to fill the window
```

With it on: lay out your game in virtual coordinates, and read the mouse via
`ctx.input.mousePosition()` (it returns virtual coords; raw `GetMousePosition()` does
not). Use the virtual dimensions for layout, not `GetScreenWidth()`.

### Text

On-screen text is `rad2d::Text` (see the [rad2d section](#text-rad2dtext) for the full
option list — fonts, typewriter, shake/wave/fade effects). The engine adds a small
measuring helper for centering (`game/textutil.h`):

```cpp
Vec2  size = measureText(*fonts.get(F_MAIN), "PAUSED", 64.0f, /*spacing*/4.0f);
float x    = centeredX(SCREEN_W / 2.0f, size.x);   // top-left x to center it
float y    = centeredY(SCREEN_H / 2.0f, size.y);
```

Draw HUD/menu text in the scene's **`onDraw`** (screen space). Draw world-anchored text
(floating damage numbers) as a `Text` **entity** drawable, or in `onDrawWorld`.

---

## 7. The rad2d layer

`#include "engine.hpp"` pulls in all of rad2d (namespace `rad2d`). This is where the
*visuals* come from. An `Entity`'s `drawable` is a `std::unique_ptr<rad2d::Drawable>`.

### Asset registries (load once, share everywhere)

```cpp
rad2d::TextureRegistry tex;  tex.load(T_HERO, "assets/hero.png");  // get(id) -> shared_ptr<Texture2D>
rad2d::FontRegistry   fonts; fonts.load(F_MAIN, "assets/font.ttf"); // get(id) -> shared_ptr<Font>
rad2d::AnimationRegistry anim;                                      // add(id, anim); get(id)
```

Load these **after** the `Game` exists (they upload to the GPU). Keep them alive for the
program (locals in `main` work — drawables hold `shared_ptr`s to what they use).

### Animations

An `Animation` is a list of `Frame`s (each a crop rectangle into a sheet + a duration)
plus rules. Animations store only the *rectangles* — the texture lives on the sprite —
so one animation can be reused across many sheets with the same frame layout.

```cpp
using namespace rad2d;
auto frame = [](int i, float t){ return Frame(t, Rectangle{ (float)(i*32), 0, 32, 32 }); };

// idle: a single static frame (time 0 = never advances)
anim.add(A_IDLE, std::make_shared<Animation>("idle",
    std::vector<Frame>{ frame(0, 0.0f) }, AnimRule(/*repeat*/false,/*returnToFirst*/false,/*pingPong*/false)));

// run: 4 frames looping at 0.1s each
anim.add(A_RUN, std::make_shared<Animation>("run",
    std::vector<Frame>{ frame(1,0.1f), frame(2,0.1f), frame(3,0.1f), frame(4,0.1f) }, AnimRule(true,false,false)));
```

`AnimRule(isRepeating, returnToFirstFrame, pingPong)`. A non-repeating animation stops
on its last frame when done.

### Sprite

An animated sprite. Declare which animations it may use, set one active, and it crops
the current frame each draw. **Sprites draw centered on the entity's position and rotate
about their center.**

```cpp
auto spr = std::make_unique<rad2d::Sprite>("hero", &anim, /*x*/0,/*y*/0,/*z*/10, /*w*/48,/*h*/48);
spr->setTexture(tex.get(T_HERO));
spr->addAnimation(A_IDLE);
spr->addAnimation(A_RUN);
spr->setAnimation(A_IDLE);          // set without playing -> shows frame 0
spr->setAnimation(A_RUN, /*play*/true);  // set + play
e->drawable = std::move(spr);
```

The `z` argument is the **layer** — entities are drawn low-z first. The `Game` pushes
the entity's position/rotation into the drawable each frame, so you set those on the
*entity*, not the sprite.

### Background

A full-area drawable that either **scrolls** a tileable texture (seamless wrap) or plays
an animation across the whole area.

```cpp
auto bg = std::make_unique<rad2d::Background>("bg", /*reg*/nullptr, 0,0, /*z*/-100, SCREEN_W, SCREEN_H);
bg->setTexture(tex.get(T_BG));     // also flips it to REPEAT wrapping
bg->setScrollSpeed(24.0f, 14.0f);  // px/sec
// attach to an invisible-ish entity at z=-100 so it draws behind everything
```

### Text (`rad2d::Text`)

The full-featured text drawable.

```cpp
auto t = std::make_unique<rad2d::Text>("label", x, y, z);
t->setFont(fonts.get(F_MAIN));     // optional; falls back to raylib's default font
t->setText("HELLO");
t->setFontSize(32.0f);
t->setSpacing(2.0f);
t->setColor(WHITE);                // default is BLACK!

// typewriter reveal
t->enableTypewriter(24.0f);        // chars/second
t->revealAll();  t->isFinished();  t->disableTypewriter();

// per-glyph effects
t->setEffect(rad2d::TextEffect::WAVE);   // NONE / SHAKE / WAVE / FADE_IN / FADE_OUT
t->setEffectParams(8.0f, 6.0f);          // amplitude (px), speed
t->setFadeDuration(0.5f);

// per-glyph reveal callback (e.g. a blip sound)
t->setOnReveal([](int glyphIndex, int codepoint){ /* play a sound */ });

t->draw();   // Text advances its own clocks; just draw it each frame
```

### UI

A composite HUD element: an (optionally animated) background panel + an optional icon +
an optional attached `Text`. Animate the panel with the same `addAnimation`/`setAnimation`
calls as a sprite.

### Tilemap

rad2d **renders** tile layers (collision is the engine's [`resolveTileCollision`](#tile-collision)).

```cpp
rad2d::TileSet set(&anim);
set.defineTile(1, tex.get(T_TILES), Rectangle{0,0,32,32});       // static tile id 1
set.defineAnimatedTile(2, tex.get(T_TILES), A_WATER);            // animated tile id 2
set.update();   // call once/frame to advance animated tiles

auto map = std::make_unique<rad2d::Tilemap>("ground", &set, 0,0, /*z*/-50, /*tileSize*/32);
map->setTiles(idVector, cols, rows);   // row-major ids, 0 = empty (you parse the map format)
int id = map->getTile(col, row);
// give it to an entity's drawable, or draw it in onDrawWorld
```

You bring the map *format* (CSV/JSON/your own); rad2d takes the parsed `int` grid.

---

## 8. Recurring patterns

### Entity factories = your "types"

There is no `class Bullet`. A factory function builds one and returns it:

```cpp
static std::unique_ptr<Entity> makeBullet(const Deps& d, Vec2 pos, Vec2 dir) {
    auto e = std::make_unique<Entity>(E_BULLET);
    e->position = pos;
    e->velocity = dir * BULLET_SPEED;
    e->radius   = 6;
    e->drawable = makeBulletSprite(d);
    e->addBehavior(d.beh.get("integrate"));
    e->addBehavior(d.beh.get("despawnOffscreen"));
    e->addCollisionResponder([](Entity& self, Entity& o, SceneContext&){ if (o.id==E_ENEMY) self.alive=false; });
    return e;
}
// spawn it:  ctx.scene.add(makeBullet(deps, p->position, dir));
```

### A `Deps` bundle for factories

Factories often need the same handles (registries, rng, audio). Bundle references into a
small struct you pass around:

```cpp
struct Deps {
    rad2d::TextureRegistry&       tex;
    rad2d::AnimationRegistry&     anim;
    FunctionRegistry<BehaviorFn>& beh;
    Rng&                          rng;
};
Deps deps{ tex, anim, game.getBehaviors(), game.getRng() };
```

(Reference members → copying `Deps` into a lambda just copies handles, which outlive the
loop.)

### Shared per-entity state → a component

When a behavior *and* a collision responder both need the same state (HP, a flash timer),
make it a **component** and read `self.get<T>()` in each — no captures, no `shared_ptr`
plumbing:

```cpp
struct Vitals { int hp; float flashTimer; enum { ALIVE, FLASHING, DYING } state; };
auto& v = e->addComponent<Vitals>(); v.hp = 3;
e->addBehavior([](Entity& self, float dt){ auto* v=self.get<Vitals>(); /* tick timers */ });
e->addCollisionResponder([](Entity& self, Entity& o, SceneContext&){ self.get<Vitals>()->hp--; });
```

### Input → intent, simulation → behaviors

`onHandleInput` translates input into **intent** (set velocity, set a "wants to jump"
flag on a component). Behaviors/`onUpdate` act on that intent during the fixed step.
This respects the [lifecycle](#4-the-frame-lifecycle) (input once/frame; update fixed-step).

### Scene transitions & overlays

```cpp
// from any scene hook:
ctx.game.setActive(SCENE_GAMEOVER);   // replace the stack
ctx.game.pushScene(SCENE_PAUSE);      // overlay (gameplay beneath freezes but still draws)
ctx.game.popScene();                  // back to the scene beneath (preserved)
```

A pause menu, inventory, or dialogue box is a scene you `pushScene`. Only the top scene
updates, so everything beneath is frozen; all scenes still draw bottom-to-top.

---

## 9. Genre playbooks

### Asteroids / top-down shooter (the reference, in `app.cpp`)
- Entities: ship (persistent + attached), asteroids & bullets (scene-owned), circle
  shapes.
- Movement: a `"integrate"` behavior; **thrust** = `onHandleInput` adds acceleration and a
  `"drag"` behavior decays velocity so you coast; screen-`"wrap"` behavior.
- Aim: `rotation = atan2(mouse - pos) + offset`; fire on `isPressed(FIRE)`.
- Combat: a `Vitals` component shared by the hit-flash behavior and the
  bullet/asteroid collision responders.
- Score in the `Blackboard` with a persisted high score; HUD + custom cursor in `onDraw`;
  pause overlay via `pushScene`.

### Platformer
- Entities use **Box** shapes (`halfExtents`).
- Gravity: a behavior `self.velocity.y += GRAVITY * dt;` plus `"integrate"`.
- Solid world: build a `rad2d::Tilemap` for visuals and a `TileGrid` for collision; call
  `resolveTileCollision(*mover, grid)` in `onUpdate` for each solid mover (it zeroes the
  blocked velocity, so landing/wall-stopping is automatic).
- Jump: in `onHandleInput`, on `isPressed(JUMP)` and "on ground" (you were stopped
  vertically last frame), set `velocity.y = -JUMP_SPEED`.
- Camera: `followSmooth` the player + `clampToBounds` to the level. Consider a smaller
  `setFixedStep` or keep `1/60` for tight, deterministic feel.
- Pixel art: `setVirtualResolution(...)`.

### ARPG (top-down)
- **Player is persistent**; each room is a scene that `attach`es the player in `onEnter`.
  Doors call `ctx.game.setActive(SCENE_NEXT_ROOM)`.
- **Components** carry everything queryable: `Health`, `Stats`, `Inventory`, `StatusEffects`,
  an AI blackboard. Systems live in `onUpdate` (iterate `ctx.scene.findAll(E_ENEMY)`),
  reacting to components.
- Collision: boxes/circles per entity; use `inRadius`/`nearest` for targeting and AoE.
- **Menus, inventory, dialogue** are `pushScene` overlays (gameplay freezes underneath).
- World: `Tilemap` rooms + `TileGrid` collision; camera `follow` + `clampToBounds`.
- **Save/load** via the `Blackboard` (and components serialized as you see fit).
- Click-to-act: `cam.screenToWorld(ctx.input.mousePosition())` to get the world point.

---

## 10. Gotchas

- **Run from the repo root** so `"assets/..."` and save files resolve.
- **`ctx.dt` is always the fixed step** (`1/60`). `update`/behaviors may run 0–n times a
  frame; `handleInput` runs exactly once. Read `isPressed`/`isReleased` only in
  `onHandleInput`.
- **Load assets after constructing `Game`** (window + GL/audio must exist first).
- **`Text` defaults to BLACK** — set a color or it's invisible on a dark background.
- **rad2d animations & background scroll advance in `draw()`** (they read frame time
  themselves). So a *paused* scene still animates sprites and scrolls its background —
  only entity *positions* freeze (update is skipped). Truly freezing visuals would need
  a rad2d change.
- **Sprites are centered**; Background/UI/Tilemap draw from their top-left. Entity
  `position` is the sprite center and the collision-shape center.
- **Entity `rotation` is radians**; the engine converts to degrees for the drawable.
- **`radius == 0` (Circle) or `halfExtents == 0` (Box) means non-colliding** — same idea
  as a null drawable meaning invisible.
- **Persistent entities don't participate until `attach`ed** to a scene.
- **`enter()` clears a scene's entities/attached** — re-activating rebuilds the world (so
  spawn everything in `onEnter`). `onExit` runs on switch/pop, **not** on window close.
- **ESC is raylib's default close key** — don't bind pause/back to it (or `SetExitKey(KEY_NULL)`).
- **Behaviors can't see input/scene/rng** — only `(self, dt)`. Use `onHandleInput`/
  `onUpdate`/responders for those.
- **Mouse: use `ctx.input.mousePosition()`** (handles virtual resolution); raw
  `GetMousePosition()` doesn't.

---

## 11. API cheat sheet

```
Game
  Game(w,h,fps,fullscreen,title)
  addScene(id)->Scene&   setActive(id)   pushScene(id)   popScene()   run()
  pause() resume() togglePause() isPaused()   setFixedStep(dt)   setVirtualResolution(w,h)
  getRng() getInput() getAudio() getState() getPersistent() getScripts() getBehaviors()

Scene  (assign std::function hooks; never subclass)
  onEnter onExit onHandleInput onUpdate onDraw onDrawWorld   (all void(SceneContext&))
  std::optional<Camera> camera;   bool debugDraw;
  add(uptr<Entity>)->Entity*   attach(Entity*)
  find(id)  findAll(id)  nearest(pos,id)  inRadius(center,r,id)

SceneContext { scene, pRegistry, input, rng, game, dt }

Entity
  id; position; velocity; rotation(rad); alive;
  shape{Circle,Box}; radius; halfExtents; collidable();
  drawable(uptr<rad2d::Drawable>);
  addBehavior(fn)   addCollisionResponder(fn)
  addComponent<T>(args...)->T&   get<T>()->T*   has<T>()   removeComponent<T>()
  BehaviorFn  = void(Entity& self, float dt)
  CollisionFn = void(Entity& self, Entity& other, SceneContext& ctx)

Vec2 { x,y; + - *(scalar) += -=; length(); normalized(); Vec2::fromAngle(rad) }

Input  bind/bindKey/bindMouse/bindGamepadButton(action,code)  rebind  unbind
       isDown/isPressed/isReleased(action)
       mousePosition() mouseDelta() mouseWheel() gamepadAxis(ax) gamepadAvailable() setGamepad(id)

Rng    range(lo,hi) [float|int]   angle()   chance(p)

Audio  loadSound(id,path) playSound(id[,vol,pitch])
       loadMusic(id,path) playMusic(id[,loop]) stopMusic() setMusicVolume(v)

Camera Camera::centered()  follow(e)  followSmooth(e,dt,k)  clampToBounds(min,max,view)
       shake(mag,dur)  screenToWorld(v)  worldToScreen(v)   target offset rotation zoom

Timing Timer{start(s) tick(dt)->bool stop() running()}
       Interval(p){tick(dt)->int}
       Tween{start(a,b,dur,ease) value() tick(dt)->bool done()}   ease::{linear,inQuad,outQuad,inOutQuad,inCubic,outCubic,outBack}

Blackboard  setInt/Float/Bool/String(k,v)  getInt/Float/Bool/String(k[,def])  has remove clear  save(path) load(path)

Tiles  TileGrid{ tileAt(c,r)->int, isSolid(id)->bool, tileSize, origin, cols, rows }
       resolveTileCollision(Entity&, TileGrid)

Text   measureText(font,str,size,spacing)->Vec2   centeredX(cx,w)   centeredY(cy,h)

rad2d  TextureRegistry/FontRegistry/AnimationRegistry (load/get/add)
       Sprite(name,&anim,x,y,z,w,h){ setTexture, addAnimation(id), setAnimation(id[,play]) }
       Background{ setTexture, setScrollSpeed(vx,vy) }
       Text{ setFont setText setFontSize setSpacing setColor enableTypewriter setEffect setEffectParams setOnReveal }
       Tilemap(name,&set,x,y,z,tileSize){ setTiles(ids,cols,rows) getTile setTile }   TileSet{ defineTile defineAnimatedTile update }
       Animation(name,frames,AnimRule)   Frame(time,Rectangle)   AnimRule(repeat,returnFirst,pingPong)
```

---

*The Asteroids game in `app.cpp` is the living reference — every system above is used
there. When in doubt, read it.*
