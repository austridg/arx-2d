#pragma once

#include <functional>

class Entity;

// A behavior is one piece of per-frame logic attached to an Entity: movement,
// AI, lifetime, etc. It's just a callable taking the entity it lives on (`self`)
// and the frame's delta time.
//
//   - simple / stateless logic  -> a lambda
//   - logic that needs its own state -> a struct with operator() (state in members),
//     or a `mutable` lambda (state in the capture)
//
// Both fit in a BehaviorFn, so there's no base class to inherit. Behaviors are how
// genres differ: the SAME Entity becomes a ship, a platformer hero, or a Zelda
// character depending on which movement behavior it carries.
using BehaviorFn = std::function<void(Entity& self, float dt)>;
