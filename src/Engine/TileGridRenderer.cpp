// src/Engine/TileGridRenderer.cpp

#include "TileGridRenderer.h"

#include "../Physics/DebugLineShader.h"
#include "../ECS/Components/EditorPlacedComponent.h"
#include "../ECS/Components/TransformComponent.h"
#include "../Config/PrefabManager.h"

#include <cmath>

// ---------------------------------------------------------------------------
// Destructor
// ---------------------------------------------------------------------------

TileGridRenderer::~TileGridRenderer() {
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    delete shader_;
}

// ---------------------------------------------------------------------------
// init — create GPU resources
// ---------------------------------------------------------------------------

void TileGridRenderer::init() {
    if (initialised_) return;
    initialised_ = true;

    shader_ = new DebugLineShader();

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);

    const std::size_t kInitialBytes = sizeof(LineVertex) * 65536;
    vboCapacity_ = kInitialBytes;

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(kInitialBytes),
                 nullptr, GL_DYNAMIC_DRAW);

    // position — attribute 0
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex),
                          reinterpret_cast<void*>(0));
    // color — attribute 1
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex),
                          reinterpret_cast<void*>(3 * sizeof(float)));

    glBindVertexArray(0);
}

// ---------------------------------------------------------------------------
// Helper — append a coloured line segment
// ---------------------------------------------------------------------------

void TileGridRenderer::addLine(const glm::vec3& a, const glm::vec3& b,
                                float r, float g, float bv) {
    vertices_.push_back({a.x, a.y, a.z, r, g, bv});
    vertices_.push_back({b.x, b.y, b.z, r, g, bv});
}

// ---------------------------------------------------------------------------
// render
// ---------------------------------------------------------------------------

void TileGridRenderer::render(const EditorState&    es,
                               const entt::registry& registry,
                               const glm::mat4&      view,
                               const glm::mat4&      projection,
                               int                   gridRadius) {
    init();  // no-op after first call

    if (!es.isEditorMode || !es.showTileGrid || !es.hasGhostEntity) return;

    const float ts   = es.tileSize;
    const float yRef = es.ghostPosition.y; // grid sits on the terrain surface

    // --- Build occupancy set ---
    TileSet occupied = TileGrid::buildOccupancy(registry, ts);

    // --- Centre the grid on the ghost tile ---
    TileCoord ghostTile = TileGrid::worldToTile(
        es.ghostPosition.x, es.ghostPosition.z, ts);

    // --- Draw tile cells ---
    for (int dx = -gridRadius; dx <= gridRadius; ++dx) {
        for (int dz = -gridRadius; dz <= gridRadius; ++dz) {
            int tx = ghostTile.x + dx;
            int tz = ghostTile.z + dz;

            glm::vec2 centre = TileGrid::tileCenter(tx, tz, ts);
            float x0 = centre.x - ts * 0.5f;
            float x1 = centre.x + ts * 0.5f;
            float z0 = centre.y - ts * 0.5f;
            float z1 = centre.y + ts * 0.5f;

            TileCoord cell{tx, tz};
            bool isCentreCell = (tx == ghostTile.x && tz == ghostTile.z);
            bool isOccupied   = (occupied.count(cell) > 0);

            float r, g, b;
            if (isCentreCell) {
                // Ghost tile: green = OK, red = blocked
                if (es.placementBlocked || isOccupied) {
                    r = 1.0f; g = 0.2f; b = 0.2f;   // red
                } else {
                    r = 0.2f; g = 1.0f; b = 0.2f;   // green
                }
            } else if (isOccupied) {
                r = 0.9f; g = 0.3f; b = 0.1f;        // orange-red = occupied
            } else {
                r = 0.5f; g = 0.5f; b = 0.5f;        // grey = free
            }

            // Draw the four edges of this tile cell.
            glm::vec3 c0{x0, yRef, z0};
            glm::vec3 c1{x1, yRef, z0};
            glm::vec3 c2{x1, yRef, z1};
            glm::vec3 c3{x0, yRef, z1};

            addLine(c0, c1, r, g, b);
            addLine(c1, c2, r, g, b);
            addLine(c2, c3, r, g, b);
            addLine(c3, c0, r, g, b);
        }
    }

    flush(view, projection);
}

// ---------------------------------------------------------------------------
// flush — upload and draw accumulated line geometry
// ---------------------------------------------------------------------------

void TileGridRenderer::flush(const glm::mat4& view, const glm::mat4& projection) {
    if (vertices_.empty() || !shader_) return;

    shader_->start();
    shader_->loadProjectionMatrix(projection);
    shader_->loadViewMatrix(view);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);

    std::size_t byteSize = vertices_.size() * sizeof(LineVertex);
    if (byteSize > vboCapacity_) {
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(byteSize),
                     vertices_.data(), GL_DYNAMIC_DRAW);
        vboCapacity_ = byteSize;
    } else {
        glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(byteSize),
                        vertices_.data());
    }

    // Draw grid on top of geometry (disable depth test temporarily).
    GLboolean depthWasEnabled = GL_FALSE;
    glGetBooleanv(GL_DEPTH_TEST, &depthWasEnabled);
    glDisable(GL_DEPTH_TEST);

    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertices_.size()));

    if (depthWasEnabled) glEnable(GL_DEPTH_TEST);

    glBindVertexArray(0);
    shader_->stop();

    vertices_.clear();
}
