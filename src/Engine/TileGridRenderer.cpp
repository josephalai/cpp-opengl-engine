// src/Engine/TileGridRenderer.cpp

#include "TileGridRenderer.h"
#include "../Physics/DebugLineShader.h"
#include "../Terrain/Terrain.h"

#include <cmath>
#include <algorithm>

// ---------------------------------------------------------------------------

TileGridRenderer::TileGridRenderer() = default;

TileGridRenderer::~TileGridRenderer() {
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    delete shader_;
}

void TileGridRenderer::init() {
    shader_ = new DebugLineShader();

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    // Pre-allocate for up to 32 k vertices (more than enough for the grid)
    glBufferData(GL_ARRAY_BUFFER, sizeof(LineVertex) * 32768, nullptr, GL_DYNAMIC_DRAW);

    // position (attrib 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex),
                          reinterpret_cast<void*>(0));
    // colour (attrib 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex),
                          reinterpret_cast<void*>(3 * sizeof(float)));

    glBindVertexArray(0);
}

// ---------------------------------------------------------------------------

float TileGridRenderer::terrainY(Terrain* t, float wx, float wz, float fallback) const {
    if (!t) return fallback;
    return t->getHeightOfTerrain(wx, wz) + kYOffset;
}

void TileGridRenderer::addLine(float x0, float y0, float z0,
                                float x1, float y1, float z1,
                                float r,  float g,  float b) {
    vertices_.push_back({x0, y0, z0, r, g, b});
    vertices_.push_back({x1, y1, z1, r, g, b});
}

// ---------------------------------------------------------------------------

void TileGridRenderer::render(const EditorState& state,
                               Terrain*           terrain,
                               const glm::mat4&   view,
                               const glm::mat4&   projection) {
    if (!shader_ || !vao_) return;

    vertices_.clear();

    const float ts  = state.tileSize;
    const int   cx  = state.ghostTileX;
    const int   cz  = state.ghostTileZ;
    const float baseY = state.ghostPosition.y;  // fallback height when no terrain

    // -----------------------------------------------------------------------
    // 1. Draw the grid lines — one set of parallel lines in X, one in Z.
    //    Each line spans the full visible range (2*kGridRadius cells).
    // -----------------------------------------------------------------------
    const int minTX = cx - kGridRadius;
    const int maxTX = cx + kGridRadius;
    const int minTZ = cz - kGridRadius;
    const int maxTZ = cz + kGridRadius;

    // Lines parallel to X axis (varying Z, constant X extent)
    for (int iz = minTZ; iz <= maxTZ + 1; ++iz) {
        float wz = static_cast<float>(iz) * ts;
        float wx0 = static_cast<float>(minTX) * ts;
        float wx1 = static_cast<float>(maxTX + 1) * ts;
        float y0 = terrainY(terrain, wx0, wz, baseY);
        float y1 = terrainY(terrain, wx1, wz, baseY);
        addLine(wx0, y0, wz, wx1, y1, wz,  0.4f, 0.4f, 0.4f);  // dim grey
    }

    // Lines parallel to Z axis (varying X, constant Z extent)
    for (int ix = minTX; ix <= maxTX + 1; ++ix) {
        float wx = static_cast<float>(ix) * ts;
        float wz0 = static_cast<float>(minTZ) * ts;
        float wz1 = static_cast<float>(maxTZ + 1) * ts;
        float y0 = terrainY(terrain, wx, wz0, baseY);
        float y1 = terrainY(terrain, wx, wz1, baseY);
        addLine(wx, y0, wz0, wx, y1, wz1,  0.4f, 0.4f, 0.4f);  // dim grey
    }

    // -----------------------------------------------------------------------
    // 2. Highlight occupied tiles (red border) and ghost tile (green/red).
    // -----------------------------------------------------------------------
    auto drawTileBorder = [&](int tx, int tz, float r, float g, float b) {
        float x0 = static_cast<float>(tx)     * ts;
        float x1 = static_cast<float>(tx + 1) * ts;
        float z0 = static_cast<float>(tz)     * ts;
        float z1 = static_cast<float>(tz + 1) * ts;
        float y00 = terrainY(terrain, x0, z0, baseY);
        float y10 = terrainY(terrain, x1, z0, baseY);
        float y11 = terrainY(terrain, x1, z1, baseY);
        float y01 = terrainY(terrain, x0, z1, baseY);

        addLine(x0, y00, z0,  x1, y10, z0,  r, g, b);
        addLine(x1, y10, z0,  x1, y11, z1,  r, g, b);
        addLine(x1, y11, z1,  x0, y01, z1,  r, g, b);
        addLine(x0, y01, z1,  x0, y00, z0,  r, g, b);
    };

    // Highlight all occupied tiles within the visible range.
    for (const auto& oc : state.occupiedTiles) {
        int otx = oc.first;
        int otz = oc.second;
        if (otx >= minTX && otx <= maxTX && otz >= minTZ && otz <= maxTZ) {
            drawTileBorder(otx, otz, 1.0f, 0.2f, 0.2f);  // red
        }
    }

    // Ghost tile: bright green (free) or bright red (occupied).
    if (state.hasGhostEntity) {
        if (state.ghostOnOccupiedTile) {
            drawTileBorder(cx, cz, 1.0f, 0.0f, 0.0f);    // bright red
        } else {
            drawTileBorder(cx, cz, 0.2f, 1.0f, 0.2f);    // bright green
        }
    }

    if (vertices_.empty()) return;

    // -----------------------------------------------------------------------
    // 3. Upload to GPU and draw.
    // -----------------------------------------------------------------------
    shader_->start();
    shader_->loadProjectionMatrix(projection);
    shader_->loadViewMatrix(view);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);

    static size_t vboCapacity = 32768 * sizeof(LineVertex);
    size_t byteSize = vertices_.size() * sizeof(LineVertex);
    if (byteSize > vboCapacity) {
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(byteSize),
                     vertices_.data(), GL_DYNAMIC_DRAW);
        vboCapacity = byteSize;
    } else {
        glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(byteSize),
                        vertices_.data());
    }

    // Disable depth test so the grid is always visible even when the camera
    // is close to the terrain and the lines would otherwise be clipped.
    GLboolean depthWasOn = GL_FALSE;
    glGetBooleanv(GL_DEPTH_TEST, &depthWasOn);
    glDisable(GL_DEPTH_TEST);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertices_.size()));
    if (depthWasOn) glEnable(GL_DEPTH_TEST);

    glBindVertexArray(0);
    shader_->stop();
}
