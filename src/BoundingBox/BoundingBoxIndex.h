//
// Created by Joseph Alai on 7/26/21.
//

#ifndef ENGINE_BOUNDINGBOXINDEX_H
#define ENGINE_BOUNDINGBOXINDEX_H

#include "glm/glm.hpp"
#include "../Input/InputMaster.h"
#include "../RenderEngine/DisplayManager.h"
#include <map>
#include "../Toolbox/Color.h"

class BoundingBoxIndex {
/**
 * https://www.mbsoftworks.sk/tutorials/opengl3/18-3d-picking-pt1/
 */
private:
    static glm::ivec3 colors;

    static int totalColors;

public:
    /**
     * Generates a unique ID to assign to a particular object.
     *
     * @return
     */
    static ColorRGB genUniqueId();

    __attribute__((unused)) static int getIndexByColor(int r, int g, int b);

    /**
     * Retrieves the ID when inputting the color.
     *
     * @param color
     * @return
     */
    static int getIndexByColor(Color color);

    /**
     * Retrieves the Color by the index
     *
     * @param index
     * @return
     */
    ColorRGBA getColorByIndex(int index);

private:
    /**
     * Inputs the [0 : 1] color, and returns [0 : 255] color.
     *
     * @param color
     * @return
     */
    static glm::vec3 formatColor(glm::vec3 color);
};

#endif //ENGINE_BOUNDINGBOXINDEX_H
