// src/Engine/TileGridRenderer.h
// Renders a tile-placement grid overlay in the World Editor using OpenGL lines.
//
// The grid is centred on the current ghost position and shows a window of
// (2*radius + 1) × (2*radius + 1) tile cells.  Tile cells that contain an
// existing EditorPlacedComponent entity are drawn in red; the tile cell under
// the ghost preview is drawn in green (placement allowed) or red (blocked).
// Empty cells are drawn in a dim white/grey.
//
// The renderer reuses the existing DebugLineShader so no new GLSL is required.

#ifndef ENGINE_TILEGRIDRENDERER_H
#define ENGINE_TILEGRIDRENDERER_H

#define GL_SILENCE_DEPRECATION
#define GLFW_INCLUDE_GLCOREARB
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <entt/entt.hpp>
#include <vector>

#include "EditorState.h"
#include "TileGrid.h"

class DebugLineShader;

class TileGridRenderer {
public:
    TileGridRenderer() = default;
    ~TileGridRenderer();

    /// One-time GL resource initialisation — call after the GL context exists.
    /// May also be called lazily on first render.
    void init();

    /// Draw the tile grid overlay.
    ///
    /// @param editorState  Current editor state (ghost position, tile settings).
    /// @param registry     ECS registry used to build tile occupancy.
    /// @param view         Camera view matrix.
    /// @param projection   Camera projection matrix.
    /// @param gridRadius   How many tiles to show in each direction from the
    ///                     ghost position (default: 8).
    void render(const EditorState&    editorState,
                const entt::registry& registry,
                const glm::mat4&      view,
                const glm::mat4&      projection,
                int                   gridRadius = 8);

private:
    struct LineVertex { float x, y, z, r, g, b; };

    void addLine(const glm::vec3& a, const glm::vec3& b,
                 float r, float g, float bv);

    void flush(const glm::mat4& view, const glm::mat4& projection);

    bool             initialised_  = false;
    GLuint           vao_          = 0;
    GLuint           vbo_          = 0;
    std::size_t      vboCapacity_  = 0;
    DebugLineShader* shader_       = nullptr;

    std::vector<LineVertex> vertices_;
};

#endif // ENGINE_TILEGRIDRENDERER_H
