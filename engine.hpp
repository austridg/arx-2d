#pragma once

// ===========================================================================
//  2d-ge — umbrella header
//
//  Include this single file to pull in the whole engine:
//
//      #include "engine.hpp"
//
//  (Rename this file to your library's name once you've picked one — users will
//   type it a lot, so a short, distinctive name is worth it.)
//
//  The engine is built on top of rad2d and re-exports it here, so including this
//  header also gives you rad2d's Drawables, animations, tiles, and registries.
//
//  For the relative includes below to resolve, the repository root must be on
//  the compiler's include path (e.g. -I. / target_include_directories(...)).
// ===========================================================================

#define GE2D_VERSION_MAJOR 0
#define GE2D_VERSION_MINOR 1
#define GE2D_VERSION_PATCH 0

// --- render layer ----------------------------------------------------------
// rad2d: Drawable/Sprite/Background/Text/UI, animations, tiles, registries.
#include "rad2d.hpp"

// --- engine: math & entities -----------------------------------------------
#include "entity/vec2.h"       // Vec2: 2D position/velocity math
#include "entity/behavior.h"   // Behavior: per-entity logic
#include "entity/entity.h"     // Entity: a thing in the world
#include "entity/p_registry.h" // PersistentRegistry: entities that outlive scenes

// --- engine: core ----------------------------------------------------------
#include "game/input.h"             // Input: actions -> key/mouse/gamepad, isDown/isPressed/isReleased
#include "game/function_registry.h" // FunctionRegistry: named, reusable functions
#include "game/timing.h"            // Timer / Interval / Tween + easing curves
#include "game/audio.h"             // Audio: sound effects + music
#include "game/blackboard.h"        // Blackboard: global key/value state + save/load
#include "game/textutil.h"          // measureText / centeredX-Y helpers
#include "game/camera.h"            // Camera: a 2D follow-cam the scene draws through
#include "game/tilecollide.h"       // TileGrid + resolveTileCollision (AABB vs solid tiles)
#include "game/game.h"              // Game (window + loop) and Rng
#include "game/scene.h"             // Scene + SceneContext

// --- engine: draw helpers --------------------------------------------------
#include "draw/draw.h"    // engine draw helpers (Level, ...)
