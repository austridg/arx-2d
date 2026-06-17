#include "entity.h"

namespace arx {

Entity::Entity(int id) : id(id) {}

void Entity::addBehavior(BehaviorFn b) {
    behaviors.push_back(std::move(b));
}

void Entity::addCollisionResponder(CollisionFn r) {
    collisionResponders.push_back(std::move(r));
}

void Entity::update(float dt) {
    for (auto& b : behaviors) b(*this, dt);   // each behavior acts on this entity
}

void Entity::onHit(Entity& other, SceneContext& ctx) {
    // dispatch this entity's reaction to overlapping `other`. The responders only
    // forward `ctx`, so SceneContext can stay an incomplete type here.
    for (auto& r : collisionResponders) r(*this, other, ctx);
}

void Entity::draw() {
    if (!drawable) return;                             // invisible entities render nothing

    // bridge the float simulation to rad2d's int-based drawable, then draw.
    drawable->setPositionX(static_cast<int>(position.x));
    drawable->setPositionY(static_cast<int>(position.y));
    drawable->setRotation(rotation * 57.2957795f);     // engine stores radians; rad2d wants degrees
    drawable->draw();
}

bool Entity::hasDrawable() const { return drawable != nullptr; }

int Entity::layer() const {
    return drawable ? drawable->getLayerZ() : 0;
}

} // namespace arx
