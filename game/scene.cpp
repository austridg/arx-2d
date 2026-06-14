#include "scene.h"

Entity* Scene::add(std::unique_ptr<Entity> e) {
    Entity* ptr = e.get();
    entities.push_back(std::move(e));
    return ptr;
}

void Scene::enter(SceneContext& ctx) { if (onEnter) { onEnter(ctx); } }

void Scene::handleInput(SceneContext& ctx) { if (onHandleInput) { onHandleInput(ctx); } };

void Scene::update(SceneContext& ctx) {
    for(auto& e : entities) { e->update(ctx.dt); }
    reapDead();
    if (onUpdate) { onUpdate(ctx); }
}

void Scene::draw(SceneContext& ctx) {
    std::vector<Entity*> order;
    for (auto& e : entities) { if (e->hasDrawable()) { order.push_back(e.get()); } }
    std::sort(order.begin(),order.end(),[](Entity* a, Entity* b) { return a->layer() < b->layer(); });
    for (Entity* e : order) { e->draw(); }
    if (onDraw) onDraw(ctx);
}

void Scene::reapDead() {
    entities.erase(
        std::remove_if(entities.begin(),entities.end(),[](const std::unique_ptr<Entity>& e) { return !e->alive; }),
        entities.end()
    );
}