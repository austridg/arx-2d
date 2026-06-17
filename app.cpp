#include "engine.hpp"

#include <cmath>
#include <memory>
#include <vector>

using namespace arx;

// ===========================================================================
//  Asteroids — the dogfood game. A pointer-ship the player aims with the mouse
//  and drives with WASD, shooting asteroids that flash when hit and play a
//  death animation when their hp runs out. Built entirely on the engine: every
//  "type" is a factory function, reusable behaviors live in the FunctionRegistry,
//  and the scene's hooks are named scripts pulled from the script registry.
// ===========================================================================

// --- ids (the engine keys scenes/entities/actions on ints; we name our own) ---
enum Action  { MOVE_UP, MOVE_DOWN, MOVE_LEFT, MOVE_RIGHT };
enum SceneId { SCENE_PLAY };
enum EntId   { E_BG, E_PLAYER, E_ASTEROID, E_BULLET };
enum TexId   { T_SHOOTER, T_AST1, T_AST2, T_AST3, T_BULLET, T_BG };
enum AnimId  { A_IDLE, A_HIT, A_DEATH, A_BULLET };

// --- tuning ---
constexpr int   SCREEN_W = 1280, SCREEN_H = 720;   // PI comes from raylib
constexpr int   SPRITE_SIZE = 48;          // 32px art drawn a bit larger
constexpr float PLAYER_SPEED = 320.0f;
constexpr float BULLET_SPEED = 720.0f;
constexpr int   PLAYER_HP = 3;
constexpr int   ASTEROID_HP = 3;
constexpr float FLASH_TIME = 0.12f;        // how long the hit frame shows
constexpr float DEATH_TIME = 0.35f;        // 3 death frames @0.1s + a little hold
constexpr float PLAYER_RADIUS = 18.0f;
constexpr float ASTEROID_RADIUS = 20.0f;
constexpr float BULLET_RADIUS = 6.0f;
constexpr float AST_MIN_SPEED = 40.0f, AST_MAX_SPEED = 130.0f;

// Bundle of shared resources the factories need. Reference members, so copying it
// into a lambda just copies the handles -- they all outlive the game loop.
struct Deps {
    rad2d::TextureRegistry&       tex;
    rad2d::AnimationRegistry&     anim;
    FunctionRegistry<BehaviorFn>& beh;
    Rng&                          rng;
};

// Per-entity combat/visual state. Shared (via shared_ptr) between an entity's
// collision responder (which deals damage) and its behavior (which ticks the
// flash/death timers) -- the clean stand-in for a component store.
struct Vitals {
    int hp = 1;
    enum State { ALIVE, FLASHING, DYING } state = ALIVE;
    float timer = 0.0f;   // counts down the current flash or death window
};

static rad2d::Sprite* asSprite(Entity& e) {
    return static_cast<rad2d::Sprite*>(e.drawable.get());
}

// Apply one hit. iFrames = ignore hits unless fully ALIVE (brief invulnerability),
// which the player needs because it overlaps an asteroid for many frames; bullets
// are consumed on contact so asteroids don't need it.
static void applyDamage(Entity& self, Vitals& v, bool iFrames) {
    if (v.state == Vitals::DYING) { return; }
    if (iFrames && v.state != Vitals::ALIVE) { return; }

    v.hp--;
    rad2d::Sprite* spr = asSprite(self);
    if (v.hp <= 0) {
        v.state = Vitals::DYING;
        v.timer = DEATH_TIME;
        self.radius = 0.0f;                  // stop colliding while the death anim plays
        spr->setAnimation(A_DEATH, true);    // play frames 3,4,5 once
    } else {
        if (v.state != Vitals::FLASHING) { spr->setAnimation(A_HIT); }  // show frame 2
        v.state = Vitals::FLASHING;
        v.timer = FLASH_TIME;
    }
}

// Ticks the flash/death timers and drives the sprite back to idle / fires onDeathDone.
static BehaviorFn vitalsTick(std::shared_ptr<Vitals> v, std::function<void(Entity&)> onDeathDone) {
    return [v, onDeathDone](Entity& self, float dt) {
        if (v->state == Vitals::FLASHING) {
            v->timer -= dt;
            if (v->timer <= 0.0f) {
                v->state = Vitals::ALIVE;
                asSprite(self)->setAnimation(A_IDLE);   // back to frame 1
            }
        } else if (v->state == Vitals::DYING) {
            v->timer -= dt;
            if (v->timer <= 0.0f) { onDeathDone(self); }
        }
    };
}

// A 32x32-sheet sprite wired with the shared idle/hit/death animations.
static std::unique_ptr<rad2d::Sprite> makeSheetSprite(const Deps& d, int texId, int z) {
    auto s = std::make_unique<rad2d::Sprite>("s", &d.anim, 0, 0, z, SPRITE_SIZE, SPRITE_SIZE);
    s->setTexture(d.tex.get(texId));
    s->addAnimation(A_IDLE);
    s->addAnimation(A_HIT);
    s->addAnimation(A_DEATH);
    s->setAnimation(A_IDLE);     // sit on frame 1
    return s;
}

static std::unique_ptr<Entity> makeBullet(const Deps& d, Vec2 pos, Vec2 dir) {
    auto e = std::make_unique<Entity>(E_BULLET);
    e->position = pos;
    e->velocity = dir * BULLET_SPEED;
    e->radius   = BULLET_RADIUS;

    auto spr = std::make_unique<rad2d::Sprite>("bullet", &d.anim, 0, 0, 5, 16, 16);
    spr->setTexture(d.tex.get(T_BULLET));
    spr->addAnimation(A_BULLET);
    spr->setAnimation(A_BULLET);
    e->drawable = std::move(spr);

    e->addBehavior(d.beh.get("integrate"));
    e->addBehavior(d.beh.get("despawnOffscreen"));
    e->addCollisionResponder([](Entity& self, Entity& other, SceneContext&) {
        if (other.id == E_ASTEROID) { self.alive = false; }   // consumed on impact
    });
    return e;
}

static std::unique_ptr<Entity> makeAsteroid(const Deps& d, Vec2 pos, Vec2 vel) {
    auto e = std::make_unique<Entity>(E_ASTEROID);
    e->position = pos;
    e->velocity = vel;
    e->radius   = ASTEROID_RADIUS;
    e->drawable = makeSheetSprite(d, T_AST1 + d.rng.range(0, 2), 1);   // random variant

    auto v = std::make_shared<Vitals>();
    v->hp = ASTEROID_HP;
    e->addBehavior(d.beh.get("integrate"));
    e->addBehavior(d.beh.get("wrap"));
    e->addBehavior(vitalsTick(v, [](Entity& self) { self.alive = false; }));  // death anim done -> gone
    e->addCollisionResponder([v](Entity& self, Entity& other, SceneContext&) {
        if (other.id == E_BULLET) { applyDamage(self, *v, false); }
    });
    return e;
}

static std::unique_ptr<Entity> makePlayer(const Deps& d, std::shared_ptr<Vitals> v) {
    auto e = std::make_unique<Entity>(E_PLAYER);
    e->position = { SCREEN_W / 2.0f, SCREEN_H / 2.0f };
    e->radius   = PLAYER_RADIUS;
    e->drawable = makeSheetSprite(d, T_SHOOTER, 10);

    e->addBehavior(d.beh.get("integrate"));
    e->addBehavior(d.beh.get("wrap"));
    e->addBehavior(vitalsTick(v, [v](Entity& self) {
        // death anim finished -> respawn at center with full hp
        v->state = Vitals::ALIVE;
        v->hp = PLAYER_HP;
        self.position = { SCREEN_W / 2.0f, SCREEN_H / 2.0f };
        self.velocity = { 0.0f, 0.0f };
        self.radius   = PLAYER_RADIUS;
        asSprite(self)->setAnimation(A_IDLE);
    }));
    e->addCollisionResponder([v](Entity& self, Entity& other, SceneContext&) {
        if (other.id == E_ASTEROID) { applyDamage(self, *v, true); }   // i-frames during the flash
    });
    return e;
}

// Spawn one asteroid at a random spot away from the player's start, drifting in a
// random direction (it wraps around the screen edges).
static void spawnAsteroid(SceneContext& ctx, const Deps& d) {
    const Vec2 center{ SCREEN_W / 2.0f, SCREEN_H / 2.0f };
    Vec2 pos;
    do {
        pos = { d.rng.range(0.0f, (float)SCREEN_W), d.rng.range(0.0f, (float)SCREEN_H) };
    } while ((pos - center).length() < 200.0f);

    Vec2 vel = Vec2::fromAngle(d.rng.angle()) * d.rng.range(AST_MIN_SPEED, AST_MAX_SPEED);
    ctx.scene.add(makeAsteroid(d, pos, vel));
}

int main() {
    Game game(SCREEN_W, SCREEN_H, 60, false, "2d-ge: Asteroids");

    // Textures load to the GPU, so this must happen AFTER the Game ctor's InitWindow.
    rad2d::TextureRegistry tex;
    tex.load(T_SHOOTER, "assets/space_shooter.png");
    tex.load(T_AST1,    "assets/asteroid1.png");
    tex.load(T_AST2,    "assets/asteroid2.png");
    tex.load(T_AST3,    "assets/asteroid3.png");
    tex.load(T_BULLET,  "assets/bullet.png");
    tex.load(T_BG,      "assets/space_background.png");

    // One set of animations, reused by the player and every asteroid: the 160x32
    // sheets share the same 32px frame layout, and an Animation only stores the
    // crop rectangles (the texture lives on each Sprite).
    rad2d::AnimationRegistry anim;
    auto f32 = [](int i, float t) {
        return rad2d::Frame(t, Rectangle{ (float)(i * 32), 0.0f, 32.0f, 32.0f });
    };
    anim.add(A_IDLE,   std::make_shared<rad2d::Animation>("idle",
                       std::vector<rad2d::Frame>{ f32(0, 0.0f) },               rad2d::AnimRule(false, false, false)));
    anim.add(A_HIT,    std::make_shared<rad2d::Animation>("hit",
                       std::vector<rad2d::Frame>{ f32(1, 0.0f) },               rad2d::AnimRule(false, false, false)));
    anim.add(A_DEATH,  std::make_shared<rad2d::Animation>("death",
                       std::vector<rad2d::Frame>{ f32(2, 0.1f), f32(3, 0.1f), f32(4, 0.1f) },
                                                                                rad2d::AnimRule(false, false, false)));
    anim.add(A_BULLET, std::make_shared<rad2d::Animation>("bullet",
                       std::vector<rad2d::Frame>{ rad2d::Frame(0.0f, Rectangle{ 0, 0, 16, 16 }) },
                                                                                rad2d::AnimRule(false, false, false)));

    // WASD movement through the engine's Input. (The mouse aim + left-click shoot
    // are read straight from raylib in the input script: Input is keyboard-only.)
    Input& in = game.getInput();
    in.bind(MOVE_UP, KEY_W);
    in.bind(MOVE_DOWN, KEY_S);
    in.bind(MOVE_LEFT, KEY_A);
    in.bind(MOVE_RIGHT, KEY_D);

    // Reusable, stateless behaviors -> the behavior registry.
    auto& beh = game.getBehaviors();
    beh.add("integrate", [](Entity& self, float dt) {
        self.position += self.velocity * dt;
    });
    beh.add("wrap", [](Entity& self, float) {
        if (self.position.x < 0)        { self.position.x += SCREEN_W; }
        if (self.position.x > SCREEN_W) { self.position.x -= SCREEN_W; }
        if (self.position.y < 0)        { self.position.y += SCREEN_H; }
        if (self.position.y > SCREEN_H) { self.position.y -= SCREEN_H; }
    });
    beh.add("despawnOffscreen", [](Entity& self, float) {
        const float m = 48.0f;
        if (self.position.x < -m || self.position.x > SCREEN_W + m ||
            self.position.y < -m || self.position.y > SCREEN_H + m) { self.alive = false; }
    });

    Deps deps{ tex, anim, beh, game.getRng() };

    // The player is persistent (outlives scenes) and gets attached to the play scene
    // as a full participant -> it's swept against the scene's asteroids.
    auto playerVitals = std::make_shared<Vitals>();
    playerVitals->hp = PLAYER_HP;
    game.getPersistent().add(makePlayer(deps, playerVitals));

    // Scene hooks live in the script registry, then get bound onto the scene.
    auto& scripts = game.getScripts();

    scripts.add("playEnter", [deps, playerVitals](SceneContext& ctx) {
        // scrolling space background (scene-owned, drawn behind everything)
        auto bg = std::make_unique<Entity>(E_BG);
        auto bgd = std::make_unique<rad2d::Background>("bg", nullptr, 0, 0, -100, SCREEN_W, SCREEN_H);
        bgd->setTexture(deps.tex.get(T_BG));
        bgd->setScrollSpeed(24.0f, 14.0f);
        bg->drawable = std::move(bgd);
        ctx.scene.add(std::move(bg));

        // reset + attach the persistent player
        if (Entity* p = ctx.pRegistry.find(E_PLAYER)) {
            p->position = { SCREEN_W / 2.0f, SCREEN_H / 2.0f };
            p->velocity = { 0.0f, 0.0f };
            p->rotation = 0.0f;
            p->radius   = PLAYER_RADIUS;
            playerVitals->state = Vitals::ALIVE;
            playerVitals->hp = PLAYER_HP;
            asSprite(*p)->setAnimation(A_IDLE);
            ctx.scene.attach(p);
        }

        for (int i = 0; i < 6; ++i) { spawnAsteroid(ctx, deps); }
    });

    scripts.add("playInput", [deps, playerVitals](SceneContext& ctx) {
        Entity* p = ctx.pRegistry.find(E_PLAYER);
        if (!p) { return; }
        if (playerVitals->state == Vitals::DYING) { p->velocity = { 0.0f, 0.0f }; return; }

        // WASD -> velocity
        Vec2 dir{ 0.0f, 0.0f };
        if (ctx.input.isDown(MOVE_UP))    { dir.y -= 1.0f; }
        if (ctx.input.isDown(MOVE_DOWN))  { dir.y += 1.0f; }
        if (ctx.input.isDown(MOVE_LEFT))  { dir.x -= 1.0f; }
        if (ctx.input.isDown(MOVE_RIGHT)) { dir.x += 1.0f; }
        p->velocity = dir.normalized() * PLAYER_SPEED;

        // aim the tip at the mouse (art points up at rest -> +90 deg)
        Vector2 mp = GetMousePosition();
        Vec2 aim = Vec2{ mp.x, mp.y } - p->position;
        float ang = atan2f(aim.y, aim.x);
        p->rotation = ang + PI * 0.5f;

        // left click -> bullet along the aim direction
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vec2 d = aim.length() > 1.0f ? aim.normalized() : Vec2::fromAngle(ang);
            ctx.scene.add(makeBullet(deps, p->position + d * 26.0f, d));
        }
    });

    auto spawnTimer = std::make_shared<float>(1.5f);
    scripts.add("playUpdate", [deps, spawnTimer](SceneContext& ctx) {
        *spawnTimer -= ctx.dt;
        if (*spawnTimer <= 0.0f) {
            *spawnTimer = 1.5f;
            int count = 0;
            for (auto& e : ctx.scene.entities) { if (e->id == E_ASTEROID) { ++count; } }
            if (count < 12) { spawnAsteroid(ctx, deps); }
        }
    });

    Scene& play = game.addScene(SCENE_PLAY);
    play.onEnter       = scripts.get("playEnter");
    play.onHandleInput = scripts.get("playInput");
    play.onUpdate      = scripts.get("playUpdate");

    game.setActive(SCENE_PLAY);
    game.run();
    return 0;
}
