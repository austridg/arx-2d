#include "transition.h"

#include <cmath>   // sqrtf

namespace arx {

float Transition::coverage() const {
    return ease::clamp01(cover.value());
}

bool Transition::covered() const {
    return active && coverage() >= 0.999f;
}

void Transition::tick(float dt) {
    if (!active) { return; }

    // Tween::tick advances and returns true only on the frame it completes. While a
    // phase is still running we just keep painting whatever coverage() reports.
    if (!cover.tick(dt)) { return; }

    switch (phase) {
        case Phase::Out: {
            // fully covered now -> do the hidden work (e.g. swap scenes) exactly once
            if (onSwap) { auto s = onSwap; onSwap = nullptr; s(); }

            if (holdDur > 0.0f) {
                phase = Phase::Hold;
                cover.start(1.0f, 1.0f, holdDur);            // pinned covered for a beat
            } else if (inDur > 0.0f) {
                phase = Phase::In;
                cover.start(1.0f, 0.0f, inDur, inEase);      // reveal
            } else {
                // fadeOut-only: rest fully covered (tween idle at 1) until something
                // reveals us. `active` stays true so the cover keeps painting and
                // input stays locked.
                phase = Phase::Hold;                         // cover is idle at value 1
            }
            break;
        }
        case Phase::Hold: {
            // a real (timed) hold elapsed -> reveal. The fadeOut-only rest never gets
            // here because its cover tween is idle (tick returns false above).
            if (inDur > 0.0f) {
                phase = Phase::In;
                cover.start(1.0f, 0.0f, inDur, inEase);
            } else {
                active = false; phase = Phase::None;
            }
            break;
        }
        case Phase::In: {
            active = false; phase = Phase::None;
            if (onDone) { auto d = onDone; onDone = nullptr; d(); }
            break;
        }
        case Phase::None:
            break;
    }
}

void Transition::paint() const {
    if (!active) { return; }
    float c = coverage();
    if (render) { render(c, phase); return; }
    // default: solid colour fill at alpha = coverage
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(color, c));
}

void Transition::fadeOut(float dur, Color c, std::function<void()> done) {
    color = c; phase = Phase::Out; active = true;
    holdDur = 0.0f; inDur = 0.0f;          // no reveal -> rest covered when Out completes
    onSwap = nullptr; onDone = done;
    cover.start(0.0f, 1.0f, dur, ease::inOutQuad);
}

void Transition::fadeIn(float dur, Color c) {
    color = c; phase = Phase::In; active = true;
    holdDur = 0.0f; inDur = dur;
    onSwap = nullptr; onDone = nullptr;
    cover.start(1.0f, 0.0f, dur, inEase);
}

void Transition::flash(Color c, float dur) {
    runSwap(dur * 0.5f, dur * 0.5f, c, nullptr, 0.0f, nullptr);
}

void Transition::runSwap(float outDur, float in, Color c, std::function<void()> swap,
                         float hold, std::function<void()> done) {
    color = c; phase = Phase::Out; active = true;
    holdDur = hold; inDur = in;
    onSwap = std::move(swap); onDone = std::move(done);
    cover.start(0.0f, 1.0f, outDur, ease::inOutQuad);
}

/*
=== === ===
RENDER RECIPES
=== === ===
*/

namespace transitions {

std::function<void(float, Transition::Phase)> wipe(Color color, Dir dir) {
    return [color, dir](float c, Transition::Phase phase) {
        int W = GetScreenWidth(), H = GetScreenHeight();
        // Out: the covered band grows from one edge (edge fraction = c).
        // In:  it keeps travelling the same way, so the covered band recedes from
        //      that same edge (edge fraction = 1 - c). One continuous sweep.
        float f = (phase == Transition::Phase::In) ? (1.0f - c) : c;
        switch (dir) {
            case Dir::Left:  DrawRectangle(0, 0, (int)(W * f), H, color); break;
            case Dir::Right: DrawRectangle(W - (int)(W * f), 0, (int)(W * f), H, color); break;
            case Dir::Up:    DrawRectangle(0, 0, W, (int)(H * f), color); break;
            case Dir::Down:  DrawRectangle(0, H - (int)(H * f), W, (int)(H * f), color); break;
        }
    };
}

std::function<void(float, Transition::Phase)> iris(Color color, std::function<Vec2()> center) {
    return [color, center](float c, Transition::Phase) {
        int W = GetScreenWidth(), H = GetScreenHeight();
        Vec2 ctr = center ? center() : Vec2{ W * 0.5f, H * 0.5f };
        // DrawRing fills everything OUTSIDE the inner radius, so a shrinking inner
        // radius closes the visible porthole to a point as coverage rises.
        float maxR = sqrtf((float)W * W + (float)H * H) * 0.5f;   // reaches the corners
        float inner = maxR * (1.0f - c);
        DrawRing({ ctr.x, ctr.y }, inner, maxR, 0.0f, 360.0f, 64, color);
    };
}

} // namespace transitions

} // namespace arx
