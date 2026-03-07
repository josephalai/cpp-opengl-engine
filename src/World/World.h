//
// Created by Joseph Alai on 7/30/21.
//

#ifndef ENGINE_WORLD_H
#define ENGINE_WORLD_H

#include <cstdlib>
#include <cmath>
#ifndef HEADLESS_SERVER
#include "GLFW/glfw3.h"
#endif
#include <utility>
#include <vector>
#include <map>
#include "glm/glm.hpp"
#include "../Util/CommonHeader.h"
#include "HashMap.h"
#include "Chunk.h"
#include "WorldCommon.h"

class World {
public:
    std::vector<Chunk *> chunks = std::vector<Chunk *>(MAX_CHUNKS);

    World();

    World(glm::vec3 &charPos);

    /**
     * @brief setBlock sets the specific block in the location.
     * @param x
     * @param y
     * @param z
     * @param texture
     */
    void setBlock(int x, int y, int z, int texture);

    /**
     *  @brief findChunk
     *  searches through the positioning of the chunks and sees if one already exists
     *  and if it does, it returns it (assigns it to the pointer).
     *
     *  @var ChunkStruct *chunks
     *  @var int chunk_count
     *  @var int x_chunk_pos
     *  @var int z_chunk_pos
     *
     */
    Chunk *findChunk(int xPos, int zPos);

    /**
     *  @brief ensure_chunks: deletes any chunks outside the range. also deletes
     *                        or removes any model data, VAO, etc, that won't be
     *                        used or needed.
     *                           Then, we remove the entire chunk from the system,
     *                        and replace it with another chunk. But we first load
     *                        the world in the map, then with chunk->ensure(), we
     *                        load anything into the VAO that is needed to be loaded.
     *
     *  @var int x_char_map : the position p of the person divided by max chunks :
     *        floorf(roundf(char_x) / CHUNK_SIZE)
     *  @var int z_char_map : the position p of the person divided by max chunks :
     *        floorf(roundf(char_z) / CHUNK_SIZE)
     */
    void ensureChunks(int xCharMap, int zCharMap);

    int collide(int height, glm::vec3 &charPos);

    int hitTest(int previous, const glm::vec3 &charPos, float rx, float ry,
                int *bestX, int *bestY, int *bestZ);

    /**
     *  @brief getSightVector
     *         Returns the current line of sight vector indicating the direction
     *         the player is looking
     *  (-0.000000 169 710800176 -399691872)
     *
     *  @var float rx rotation axis p
     *  @var float ry rotation axis y
     *  @var float dx vector p
     *  @var float dy vector y
     *  @var float dz vector q
     *
     */
    void getSightVector(float rx, float ry, float *dx, float *dy, float *dz);

    /**
     *
     * @param w
     * @return
     */
    int isObstacle(int w);

    int playerIntersects(int height, float x, float y, float z, int hx, int hy, int hz);

    glm::vec2 getWorldPos(float xCharPos, float yCharPos );


};

#endif //ENGINE_WORLD_H
