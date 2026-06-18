#pragma once

#include <functional>

#include "raylib.h"        // Color, DrawRectangle, Fade, GetScreenWidth/Height
#include "entity/vec2.h"   // Vec2 (iris center provider)
#include "timing.h"        // Tween + ease curves (the shared interpolation primitive)

namespace arx {

// A full-screen presentation overlay. The Game ticks it on REAL frame time and
// draws it LAST -- on top of everything, including the letterbox in virtual-res
// mode. It is driven entirely by a Tween, so it reuses the engine's single
// interpolation primitive rather than inventing a parallel one.
//
// It knows NOTHING about the scene: only a `coverage` value (0 = scene fully
// visible, 1 = fully hidden), a `phase`, and a `color`. That keeps the render
// hook a pure function and the boundary clean -- the moment an effect needs the
// scene's actual pixels (a cross-dissolve, a pixelate) it is a different, heavier
// feature and does NOT belong here.
struct Transition {
    enum class Phase { None, Out, Hold, In };

    bool  active = false;             // a transition is in progress (locks input + paints)
    Phase phase  = Phase::None;
    Color color  = BLACK;             // fade-to-black = BLACK, fade-to-white = WHITE, any = anything

    float holdDur = 0.0f;             // seconds held fully covered between Out and In
    float inDur   = 0.0f;             // seconds of the reveal (cached while the Out tween runs)
    std::function<float(float)> inEase = ease::inOutQuad;

    Tween cover;                      // drives coverage 0..1

    std::function<void()> onSwap;     // fires ONCE at the Out->In boundary (fully covered)
    std::function<void()> onDone;     // fires once the screen is fully revealed again

    // The overlay painter. Null = the built-in solid-`color` fade. Assign a recipe
    // from transitions:: (wipe / iris) for something fancier. It receives the live
    // coverage and phase so directional effects can pick a consistent direction.
    std::function<void(float coverage, Phase phase)> render;

    // --- queries -----------------------------------------------------------
    bool  locksInput() const { return active; }   // input is suppressed for the WHOLE transition
    float coverage() const;                        // current 0..1 (clamped Tween value)
    bool  covered() const;                         // fully hidden right now (safe swap point)

    // --- driven by Game ----------------------------------------------------
    void tick(float dt);   // advance the tween + phase machine; may fire onSwap / onDone
    void paint() const;    // draw the overlay (no-op when inactive)

    // --- start one --------------------------------------------------------
    // fade the screen TO `color` and stay covered (input stays locked until a
    // fadeIn / runSwap reveals it). Good for "to black, then load".
    void fadeOut(float dur, Color c = BLACK, std::function<void()> done = nullptr);
    // reveal the scene FROM `color`. Works whether or not the screen was covered.
    void fadeIn (float dur, Color c = BLACK);
    // a quick cover-and-reveal with no hold -- damage feedback / stingers.
    void flash  (Color c = WHITE, float dur = 0.15f);
    // fade out -> run `swap` while fully covered -> fade in. The headline used by
    // Game::transitionTo to change scenes without a pop.
    void runSwap(float outDur, float inDur, Color c, std::function<void()> swap,
                 float holdDur = 0.0f, std::function<void()> done = nullptr);
};

// Ready-made render hooks. They sit BESIDE Transition (like ease:: sits beside
// Tween), not inside it, so the struct stays a pure phase machine. Assign one to
// Transition::render. Each is a pure function of coverage + phase + screen size.
namespace transitions {
    enum class Dir { Left, Right, Up, Down };

    // a doorway that sweeps fully across once: covers from one edge during Out,
    // then keeps travelling the same way (uncovers from that edge) during In.
    std::function<void(float, Transition::Phase)> wipe(Color color, Dir dir = Dir::Left);

    // a shrinking circular porthole centred on `center()` (screen coords). Pass a
    // provider to track e.g. the player; null defaults to the screen centre.
    std::function<void(float, Transition::Phase)> iris(Color color,
        std::function<Vec2()> center = nullptr);
}

} // namespace arx
