#include <iostream>
#include "Engine/Engine.h"

int main() {
    std::cout << "GL Engine Started!" << std::endl;

    Engine engine;
    engine.init();
    engine.run();
    engine.shutdown();

    return 0;
}
