#include "entity.h"

Entity::Entity(int id) : id(id) {}

void Entity::addBehavior(BehaviorFn b) {
    behaviors.push_back(std::move(b));
}

void Entity::update(float dt) {
    for (auto& b : behaviors) b(*this, dt);   // each behavior acts on this entity
}

void Entity::draw() {
    if (!drawable) return;                             // invisible entities render nothing

    // bridge the float simulation to rad2d's int-based drawable, then draw.
    drawable->setPositionX(static_cast<int>(position.x));
    drawable->setPositionY(static_cast<int>(position.y));
    drawable->draw();
}

bool Entity::hasDrawable() const { return drawable != nullptr; }

int Entity::layer() const {
    return drawable ? drawable->getLayerZ() : 0;
}
