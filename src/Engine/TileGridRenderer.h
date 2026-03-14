// src/Engine/TileGridRenderer.h
// Renders an editor tile grid as GL_LINES so the user can see placement cells
// while the World Editor is open (~).
//
// The grid is centred on the current ghost-preview tile.  Each cell is one
// "tileSize × tileSize" world-unit square.  Cells are coloured:
//   • dim white   — free tile in the surrounding area
//   • bright green — the tile currently under the ghost (free, can place)
//   • bright red   — any tile that is already occupied (including the ghost tile
//                    when placement would be blocked)
//
// The renderer reuses DebugLineShader from the Physics sub-system so no new
// GLSL shaders are needed.

#ifndef ENGINE_TILE_GRID_RENDERER_H
#define ENGINE_TILE_GRID_RENDERER_H

#include "EditorState.h"

#define GL_SILENCE_DEPRECATION
#define GLFW_INCLUDE_GLCOREARB
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vector>

class DebugLineShader;
class Terrain;

class TileGridRenderer {
public:
    TileGridRenderer();
    ~TileGridRenderer();

    /// One-time GPU resource setup — call once after the GL context is active.
    void init();

    /// Build line geometry for the current editor state and upload + draw.
    /// @param state       Current EditorState (ghost tile, occupied tiles, tileSize).
    /// @param terrain     Active terrain used for per-vertex height queries (may be nullptr).
    /// @param view        Camera view matrix.
    /// @param projection  Camera projection matrix.
    void render(const EditorState& state,
                Terrain*           terrain,
                const glm::mat4&   view,
                const glm::mat4&   projection);

private:
    struct LineVertex { float x, y, z, r, g, b; };

    void addLine(float x0, float y0, float z0,
                 float x1, float y1, float z1,
                 float r,  float g,  float b);

    /// Query terrain height at (wx, wz); returns fallback when terrain==nullptr.
    float terrainY(Terrain* t, float wx, float wz, float fallback) const;

    DebugLineShader*        shader_   = nullptr;
    GLuint                  vao_      = 0;
    GLuint                  vbo_      = 0;
    std::vector<LineVertex> vertices_;

    /// Number of tiles visible in each direction from the ghost tile.
    static constexpr int   kGridRadius = 8;
    /// Vertical offset above terrain surface to avoid z-fighting.
    static constexpr float kYOffset    = 0.08f;
};

#endif // ENGINE_TILE_GRID_RENDERER_H
