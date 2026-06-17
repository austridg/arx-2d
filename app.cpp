#include "engine.hpp"

#include <cmath>
#include <memory>
#include <vector>
#include <string>

using namespace arx;

// ===========================================================================
//  Asteroids — the dogfood game, written on the engine's idioms: entity "types"
//  are factory functions, reusable behaviors live in the FunctionRegistry, scene
//  hooks are named scripts, per-entity state lives in COMPONENTS, mouse/keyboard
//  go through Input, and the score (with a persisted best) lives in the Blackboard.
// ===========================================================================

enum Action  { MOVE_UP, MOVE_DOWN, MOVE_LEFT, MOVE_RIGHT, SHOOT, PAUSE };
enum SceneId { SCENE_PLAY, SCENE_PAUSE };
enum EntId   { E_BG, E_PLAYER, E_ASTEROID, E_BULLET };
enum TexId   { T_SHOOTER, T_AST1, T_AST2, T_AST3, T_BULLET, T_BG, T_CURSOR };
enum AnimId  { A_IDLE, A_HIT, A_DEATH, A_BULLET };
enum FontId  { F_MAIN };

constexpr int   SCREEN_W = 1280, SCREEN_H = 720;   // PI comes from raylib
constexpr int   SPRITE_SIZE = 48;
constexpr float PLAYER_THRUST    = 950.0f;   // WASD acceleration
constexpr float PLAYER_DRAG      = 1.4f;     // velocity decay/sec -> coasts when you let go
constexpr float PLAYER_MAX_SPEED = 360.0f;
constexpr float BULLET_SPEED = 720.0f;
constexpr int   PLAYER_HP = 3;
constexpr int   ASTEROID_HP = 3;
constexpr int   ASTEROID_POINTS = 100;
constexpr float FLASH_TIME = 0.12f;
constexpr float DEATH_TIME = 0.35f;
constexpr float PLAYER_RADIUS = 18.0f;
constexpr float ASTEROID_RADIUS = 20.0f;
constexpr float BULLET_RADIUS = 6.0f;
constexpr float AST_MIN_SPEED = 40.0f, AST_MAX_SPEED = 130.0f;
constexpr const char* SAVE_PATH = "asteroids_save.txt";

// Shared resources the factories need (reference members -> copying into a lambda
// just copies the handles, which all outlive the game loop).
struct Deps {
    rad2d::TextureRegistry&       tex;
    rad2d::AnimationRegistry&     anim;
    FunctionRegistry<BehaviorFn>& beh;
    Rng&                          rng;
    Blackboard&                   state;   // score / best score live here
};

// Combat + visual state, now a COMPONENT on the entity (e->get<Vitals>()), shared
// between the entity's collision responder and its behavior with no manual plumbing.
struct Vitals {
    int hp = 1;
    enum State { ALIVE, FLASHING, DYING } state = ALIVE;
    float timer = 0.0f;   // counts down the current flash or death window
};

static rad2d::Sprite* asSprite(Entity& e) { return static_cast<rad2d::Sprite*>(e.drawable.get()); }

// Apply one hit, reading the entity's own Vitals. iFrames = ignore hits unless fully
// ALIVE (the player needs it, overlapping an asteroid for many frames; bullets are
// consumed on contact so asteroids don't).
static void applyDamage(Entity& self, bool iFrames) {
    Vitals* v = self.get<Vitals>();
    if (!v || v->state == Vitals::DYING) { return; }
    if (iFrames && v->state != Vitals::ALIVE) { return; }

    v->hp--;
    rad2d::Sprite* spr = asSprite(self);
    if (v->hp <= 0) {
        v->state = Vitals::DYING;
        v->timer = DEATH_TIME;
        self.radius = 0.0f;                  // stop colliding while the death anim plays
        spr->setAnimation(A_DEATH, true);    // frames 3,4,5 once
    } else {
        if (v->state != Vitals::FLASHING) { spr->setAnimation(A_HIT); }   // frame 2
        v->state = Vitals::FLASHING;
        v->timer = FLASH_TIME;
    }
}

// Ticks the entity's Vitals timers and drives the sprite back to idle / fires onDeathDone.
static BehaviorFn vitalsTick(std::function<void(Entity&)> onDeathDone) {
    return [onDeathDone](Entity& self, float dt) {
        Vitals* v = self.get<Vitals>();
        if (!v) { return; }
        if (v->state == Vitals::FLASHING) {
            v->timer -= dt;
            if (v->timer <= 0.0f) { v->state = Vitals::ALIVE; asSprite(self)->setAnimation(A_IDLE); }
        } else if (v->state == Vitals::DYING) {
            v->timer -= dt;
            if (v->timer <= 0.0f) { onDeathDone(self); }
        }
    };
}

static std::unique_ptr<rad2d::Sprite> makeSheetSprite(const Deps& d, int texId, int z) {
    auto s = std::make_unique<rad2d::Sprite>("s", &d.anim, 0, 0, z, SPRITE_SIZE, SPRITE_SIZE);
    s->setTexture(d.tex.get(texId));
    s->addAnimation(A_IDLE);
    s->addAnimation(A_HIT);
    s->addAnimation(A_DEATH);
    s->setAnimation(A_IDLE);
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
        if (other.id == E_ASTEROID) { self.alive = false; }
    });
    return e;
}

static std::unique_ptr<Entity> makeAsteroid(const Deps& d, Vec2 pos, Vec2 vel) {
    auto e = std::make_unique<Entity>(E_ASTEROID);
    e->position = pos;
    e->velocity = vel;
    e->radius   = ASTEROID_RADIUS;
    e->drawable = makeSheetSprite(d, T_AST1 + d.rng.range(0, 2), 1);

    auto& v = e->addComponent<Vitals>();
    v.hp = ASTEROID_HP;

    Blackboard* state = &d.state;   // for scoring on death
    e->addBehavior(d.beh.get("integrate"));
    e->addBehavior(d.beh.get("wrap"));
    e->addBehavior(vitalsTick([state](Entity& self) {
        self.alive = false;
        int s = state->getInt("score") + ASTEROID_POINTS;
        state->setInt("score", s);
        if (s > state->getInt("highscore")) {   // persist a new best immediately
            state->setInt("highscore", s);
            state->save(SAVE_PATH);
        }
    }));
    e->addCollisionResponder([](Entity& self, Entity& other, SceneContext&) {
        if (other.id == E_BULLET) { applyDamage(self, false); }
    });
    return e;
}

static std::unique_ptr<Entity> makePlayer(const Deps& d) {
    auto e = std::make_unique<Entity>(E_PLAYER);
    e->position = { SCREEN_W / 2.0f, SCREEN_H / 2.0f };
    e->radius   = PLAYER_RADIUS;
    e->drawable = makeSheetSprite(d, T_SHOOTER, 10);

    auto& v = e->addComponent<Vitals>();
    v.hp = PLAYER_HP;

    e->addBehavior(d.beh.get("integrate"));
    e->addBehavior(d.beh.get("drag"));              // momentum: coasts when no thrust
    e->addBehavior(d.beh.get("wrap"));
    e->addBehavior(vitalsTick([](Entity& self) {   // death anim done -> respawn at center
        if (Vitals* v = self.get<Vitals>()) { v->state = Vitals::ALIVE; v->hp = PLAYER_HP; }
        self.position = { SCREEN_W / 2.0f, SCREEN_H / 2.0f };
        self.velocity = { 0.0f, 0.0f };
        self.radius   = PLAYER_RADIUS;
        asSprite(self)->setAnimation(A_IDLE);
    }));
    e->addCollisionResponder([](Entity& self, Entity& other, SceneContext&) {
        if (other.id == E_ASTEROID) { applyDamage(self, true); }   // i-frames during the flash
    });
    return e;
}

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

    rad2d::TextureRegistry tex;
    tex.load(T_SHOOTER, "assets/space_shooter.png");
    tex.load(T_AST1,    "assets/asteroid1.png");
    tex.load(T_AST2,    "assets/asteroid2.png");
    tex.load(T_AST3,    "assets/asteroid3.png");
    tex.load(T_BULLET,  "assets/bullet.png");
    tex.load(T_BG,      "assets/space_background.png");
    tex.load(T_CURSOR,  "assets/shooter_cursor.png");
    HideCursor();   // we draw our own crosshair in the HUD pass

    rad2d::AnimationRegistry anim;
    auto f32 = [](int i, float t) { return rad2d::Frame(t, Rectangle{ (float)(i * 32), 0.0f, 32.0f, 32.0f }); };
    anim.add(A_IDLE,   std::make_shared<rad2d::Animation>("idle",  std::vector<rad2d::Frame>{ f32(0, 0.0f) }, rad2d::AnimRule(false, false, false)));
    anim.add(A_HIT,    std::make_shared<rad2d::Animation>("hit",   std::vector<rad2d::Frame>{ f32(1, 0.0f) }, rad2d::AnimRule(false, false, false)));
    anim.add(A_DEATH,  std::make_shared<rad2d::Animation>("death", std::vector<rad2d::Frame>{ f32(2, 0.1f), f32(3, 0.1f), f32(4, 0.1f) }, rad2d::AnimRule(false, false, false)));
    anim.add(A_BULLET, std::make_shared<rad2d::Animation>("bullet", std::vector<rad2d::Frame>{ rad2d::Frame(0.0f, Rectangle{ 0, 0, 16, 16 }) }, rad2d::AnimRule(false, false, false)));

    rad2d::FontRegistry fonts;
    fonts.load(F_MAIN, "assets/Early GameBoy.ttf");
    auto scoreText = std::make_shared<rad2d::Text>("score", 24, 18, 100);
    scoreText->setFont(fonts.get(F_MAIN));
    scoreText->setFontSize(32.0f);
    scoreText->setSpacing(2.0f);
    scoreText->setColor(WHITE);

    auto cursorTex = tex.get(T_CURSOR);

    // WASD through Input; shoot bound to the left mouse button (Input handles mouse now)
    Input& in = game.getInput();
    in.bind(MOVE_UP, KEY_W);
    in.bind(MOVE_DOWN, KEY_S);
    in.bind(MOVE_LEFT, KEY_A);
    in.bind(MOVE_RIGHT, KEY_D);
    in.bindMouse(SHOOT, MOUSE_BUTTON_LEFT);
    in.bind(PAUSE, KEY_P);

    auto& beh = game.getBehaviors();
    beh.add("integrate", [](Entity& self, float dt) { self.position += self.velocity * dt; });
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
    beh.add("drag", [](Entity& self, float dt) {
        self.velocity = self.velocity * (1.0f - PLAYER_DRAG * dt);   // momentum decay -> overshoot
    });

    Deps deps{ tex, anim, beh, game.getRng(), game.getState() };

    // player is persistent (survives scene swaps) and attached as a full participant
    game.getPersistent().add(makePlayer(deps));

    auto& scripts = game.getScripts();

    scripts.add("playEnter", [deps](SceneContext& ctx) {
        deps.state.load(SAVE_PATH);          // restore best score, if a save exists
        deps.state.setInt("score", 0);

        auto bg = std::make_unique<Entity>(E_BG);
        auto bgd = std::make_unique<rad2d::Background>("bg", nullptr, 0, 0, -100, SCREEN_W, SCREEN_H);
        bgd->setTexture(deps.tex.get(T_BG));
        bgd->setScrollSpeed(24.0f, 14.0f);
        bg->drawable = std::move(bgd);
        ctx.scene.add(std::move(bg));

        if (Entity* p = ctx.pRegistry.find(E_PLAYER)) {
            p->position = { SCREEN_W / 2.0f, SCREEN_H / 2.0f };
            p->velocity = { 0.0f, 0.0f };
            p->rotation = 0.0f;
            p->radius   = PLAYER_RADIUS;
            if (Vitals* v = p->get<Vitals>()) { v->state = Vitals::ALIVE; v->hp = PLAYER_HP; }
            asSprite(*p)->setAnimation(A_IDLE);
            ctx.scene.attach(p);
        }

        for (int i = 0; i < 6; ++i) { spawnAsteroid(ctx, deps); }
    });

    scripts.add("playInput", [deps](SceneContext& ctx) {
        if (ctx.input.isPressed(PAUSE)) { ctx.game.pushScene(SCENE_PAUSE); return; }

        Entity* p = ctx.pRegistry.find(E_PLAYER);
        if (!p) { return; }
        Vitals* v = p->get<Vitals>();
        if (v && v->state == Vitals::DYING) { return; }   // no control; drag coasts it to a stop

        // WASD = thrust: accelerate in the pressed direction, clamp to max speed, and let
        // the "drag" behavior coast the ship on release (overshoot / momentum).
        Vec2 dir{ 0.0f, 0.0f };
        if (ctx.input.isDown(MOVE_UP))    { dir.y -= 1.0f; }
        if (ctx.input.isDown(MOVE_DOWN))  { dir.y += 1.0f; }
        if (ctx.input.isDown(MOVE_LEFT))  { dir.x -= 1.0f; }
        if (ctx.input.isDown(MOVE_RIGHT)) { dir.x += 1.0f; }
        if (dir.x != 0.0f || dir.y != 0.0f) {
            p->velocity += dir.normalized() * (PLAYER_THRUST * ctx.dt);
            float spd = p->velocity.length();
            if (spd > PLAYER_MAX_SPEED) { p->velocity = p->velocity * (PLAYER_MAX_SPEED / spd); }
        }

        Vec2 aim = ctx.input.mousePosition() - p->position;
        float ang = atan2f(aim.y, aim.x);
        p->rotation = ang + PI * 0.5f;   // art points up at rest

        if (ctx.input.isPressed(SHOOT)) {
            Vec2 d = aim.length() > 1.0f ? aim.normalized() : Vec2::fromAngle(ang);
            ctx.scene.add(makeBullet(deps, p->position + d * 26.0f, d));
        }
    });

    auto spawn = std::make_shared<Interval>(1.5f);
    scripts.add("playUpdate", [deps, scoreText, spawn](SceneContext& ctx) {
        scoreText->setText("SCORE " + std::to_string(deps.state.getInt("score")) +
                           "   BEST " + std::to_string(deps.state.getInt("highscore")));

        if (spawn->tick(ctx.dt) > 0) {
            int count = 0;
            for (auto& e : ctx.scene.entities) { if (e->id == E_ASTEROID) { ++count; } }
            if (count < 12) { spawnAsteroid(ctx, deps); }
        }
    });

    Scene& play = game.addScene(SCENE_PLAY);
    play.onEnter       = scripts.get("playEnter");
    play.onHandleInput = scripts.get("playInput");
    play.onUpdate      = scripts.get("playUpdate");
    play.onDraw        = [scoreText, cursorTex](SceneContext& ctx) {   // screen-space HUD
        scoreText->draw();
        Vec2 m = ctx.input.mousePosition();
        DrawTexture(*cursorTex, (int)m.x - cursorTex->width / 2, (int)m.y - cursorTex->height / 2, WHITE);
    };

    // pause overlay: pushed by the play scene, it freezes the game beneath (only the
    // top scene updates) and draws a dim screen + a WAVE-animated banner. Shows off the
    // scene stack, ctx.game access from a script, and rad2d::Text's effect options.
    auto pausedText = std::make_shared<rad2d::Text>("paused", 0, 0, 200);
    pausedText->setFont(fonts.get(F_MAIN));
    pausedText->setFontSize(64.0f);
    pausedText->setSpacing(4.0f);
    pausedText->setColor(WHITE);
    pausedText->setText("PAUSED");
    pausedText->setEffect(rad2d::TextEffect::WAVE);
    pausedText->setEffectParams(8.0f, 6.0f);   // amplitude (px), speed
    {
        Vec2 sz = measureText(*fonts.get(F_MAIN), "PAUSED", 64.0f, 4.0f);
        pausedText->setPositionX((int)centeredX(SCREEN_W / 2.0f, sz.x));
        pausedText->setPositionY((int)centeredY(SCREEN_H / 2.0f, sz.y));
    }

    Scene& pause = game.addScene(SCENE_PAUSE);
    pause.onHandleInput = [](SceneContext& ctx) {
        if (ctx.input.isPressed(PAUSE)) { ctx.game.popScene(); }   // P again -> resume
    };
    pause.onDraw = [pausedText](SceneContext&) {
        DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(BLACK, 0.6f));   // dim the frozen game
        pausedText->draw();
    };

    game.setActive(SCENE_PLAY);
    game.run();
    return 0;
}
