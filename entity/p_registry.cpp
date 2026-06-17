#include "p_registry.h"

namespace arx {

Entity* PersistentRegistry::add(std::unique_ptr<Entity> e) {
    Entity* ptr = e.get();
    entities.push_back(std::move(e));
    return ptr;
}

Entity* PersistentRegistry::find(int id) {
    for (auto& e : entities) { if(e->id == id) return e.get(); }
    return nullptr;
}

void PersistentRegistry::reapDead() {
    entities.erase(
        std::remove_if(entities.begin(),entities.end(),[](const std::unique_ptr<Entity>& e) { return !e->alive; }),
        entities.end()
    );
}

} // namespace arx