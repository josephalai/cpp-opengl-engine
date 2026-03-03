// WaterTile.h — a horizontal quad at a configurable Y height.

#ifndef ENGINE_WATERTILE_H
#define ENGINE_WATERTILE_H

#include <glm/glm.hpp>

/// One water tile, defined by its world-space centre and a height.
struct WaterTile {
    static constexpr float kTileSize = 60.0f;

    float centerX;
    float height;
    float centerZ;

    WaterTile(float x, float height, float z)
        : centerX(x), height(height), centerZ(z) {}

    glm::vec3 getCenter() const { return glm::vec3(centerX, height, centerZ); }
};

#endif // ENGINE_WATERTILE_H
