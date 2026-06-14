#include "engine.hpp"

int main() {
    enum { UP, DOWN, LEFT, RIGHT};

    Input asteroidInput;
    asteroidInput.bind(UP,KEY_W);
    asteroidInput.bind(DOWN,KEY_S);
    asteroidInput.bind(LEFT,KEY_A);
    asteroidInput.bind(RIGHT,KEY_D);
}