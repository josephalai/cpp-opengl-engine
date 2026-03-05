//
// Created by Joseph Alai on 3/9/22.
//

#include "MainGuiLoop.h"
#include "../../Engine/Engine.h"

void MainGuiLoop::main() {
    Engine engine;
    engine.init();
    engine.run();
    engine.shutdown();
}
