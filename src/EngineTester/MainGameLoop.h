// Legacy — see Engine class for the active entry point.
//
// Created by Joseph Alai on 6/30/21.
//

#ifndef ENGINE_MAINGAMELOOP_H
#define ENGINE_MAINGAMELOOP_H


#include "../Terrain/Terrain.h"
#include "../Guis/Text/FontMeshCreator/GUIText.h"

class MainGameLoop {
public:
    static void main();

    static glm::vec3 generateRandomPosition(Terrain *terrain, float yOffset = 0.0f);

    static glm::vec3 generateRandomRotation();

    static float generateRandomScale(float min, float max);
};

#endif //ENGINE_MAINGAMELOOP_H
