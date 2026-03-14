//
// Created by Joseph Alai on 7/26/21.
//

#ifndef ENGINE_TERRAINPICKER_H
#define ENGINE_TERRAINPICKER_H

#include "MousePicker.h"
class TerrainPicker : public MousePicker  {
private:
    constexpr static int kRecursionCount = 200;
    constexpr static float kRayRange = 200;

    Camera *camera;

    Terrain *terrain;
    glm::vec3 currentTerrainPoint;

public:
    TerrainPicker(Camera *cam, glm::mat4 projection, Terrain *terrain);
    glm::vec3 getCurrentTerrainPoint();
    void update();

    /// Return the terrain used for height queries (may be nullptr).
    Terrain* getTerrain() const { return terrain; }

private:
    Terrain *getTerrain(int x, int y);

    glm::vec3 binarySearch(int count, float start, float finish, glm::vec3 ray);
    bool intersectionInRange(float start, float finish, glm::vec3 ray);
    bool isUnderGround(glm::vec3 testPoint);
};


#endif //ENGINE_TERRAINPICKER_H
