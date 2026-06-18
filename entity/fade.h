#pragma once

#include <functional>
#include <utility>

#include "rad2d.hpp"        // rad2d::Drawable::get/setTint
#include "entity.h"         // Entity + its component API
#include "game/timing.h"    // Tween + ease curves

namespace arx {

// A per-entity alpha fade: a Tween in [0,1] multiplied into the entity drawable's
// tint alpha each frame. This is the generic, sprite-agnostic counterpart to a
// death-frame animation -- and it composes with one, since the animation drives
// the frame and this only touches the tint's alpha channel.
//
// Usage (two lines, or one via startFade): add the component, then make sure the
// applyFade behavior is on the entity so the tween actually advances + writes.
// (named AlphaFade, not Fade, to avoid clashing with raylib's global Fade(Color,float))
struct AlphaFade {
    Tween alpha;                            // 0 = invisible, 1 = opaque
    std::function<void(Entity&)> onDone;    // fired once, the frame the tween completes
};

// BehaviorFn (signature: (Entity&, float)). Advances the Fade tween and writes its
// value into the drawable's tint alpha. No-ops when there's no Fade, no drawable,
// or the tween is idle (so a finished fade leaves the alpha where it landed). The
// component PERSISTS after completion so it can be restarted (e.g. fade in, later
// fade out) without re-adding this behavior.
inline void applyFade(Entity& self, float dt) {
    AlphaFade* f = self.get<AlphaFade>();
    if (!f || !f->alpha.running()) { return; }

    bool done = f->alpha.tick(dt);
    if (self.drawable) {
        Color t = self.drawable->getTint();                       // every rad2d::Drawable has a tint
        t.a = (unsigned char)(255.0f * ease::clamp01(f->alpha.value()));
        self.drawable->setTint(t);
    }
    if (done && f->onDone) { f->onDone(self); }                   // e.g. [](Entity& e){ e.alive = false; }
}

// Convenience: start (or restart) a fade on an entity, wiring the component and the
// applyFade behavior exactly once. Returns the Fade so callers can set onDone.
//   fade in : startFade(e, 0, 1, 0.3f)
//   fade out: startFade(e, 1, 0, 0.4f).onDone = [](Entity& e){ e.alive = false; };
inline AlphaFade& startFade(Entity& e, float from, float to, float dur,
                            std::function<float(float)> easing = ease::linear) {
    bool firstTime = !e.has<AlphaFade>();
    AlphaFade& f = firstTime ? e.addComponent<AlphaFade>() : *e.get<AlphaFade>();
    if (firstTime) { e.addBehavior(applyFade); }   // component persists, so this stays single
    f.alpha.start(from, to, dur, std::move(easing));
    return f;
}

} // namespace arx
