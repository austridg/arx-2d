#pragma once

#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <algorithm>

#include "entity.h"

class Entity;

class PersistentRegistry {
private:
    std::vector<std::unique_ptr<Entity>> entities; // long-lived entities
public:
    Entity* add(std::unique_ptr<Entity> e);
    Entity* find(int id);

    void reapDead();
};