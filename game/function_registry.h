#pragma once

#include <unordered_map>
#include <string>

namespace arx {

// A named store of reusable functions. Instantiate it for whatever callable
// shape you want to organize, e.g.:
//
//   FunctionRegistry<std::function<void(SceneContext&)>> scripts;   // scene hooks
//   FunctionRegistry<BehaviorFn>                          behaviors; // entity behaviors
//
// Register a function under a name, then pull it back by name to reuse it across
// many scenes/entities. get() on an unknown name returns an empty (falsy)
// callable, so a missing entry degrades gracefully instead of crashing.
template <typename Fn>
class FunctionRegistry {
private:
    std::unordered_map<std::string, Fn> fns;
public:
    void add(const std::string& name, Fn fn) { fns[name] = std::move(fn); }
    bool has(const std::string& name) const  { return fns.count(name) > 0; }

    // a copy, so the caller's reused function is independent of the stored one
    Fn get(const std::string& name) const {
        auto it = fns.find(name);
        return it != fns.end() ? it->second : Fn{};
    }
};

} // namespace arx
