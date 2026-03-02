//
// Created by Joseph Alai on 3/1/22.
//

#ifndef ENGINE_GUIRECT_H
#define ENGINE_GUIRECT_H

#include "glm/glm.hpp"
#include "../GuiComponent.h"
#include "../../Toolbox/Color.h"


class GuiRect : public GuiComponent {
private:

    Color color;

    glm::vec2 position;

    glm::vec2 size;

    float alpha;

    /**
     * Scale is the percentage out of the height and width of the window
     */
    glm::vec2 scale;
public:
    explicit GuiRect(Color color, glm::vec2 &position, glm::vec2 &size, glm::vec2 &scale, float alpha);

    const glm::vec2 &getScale();

    const glm::vec3 getColor() const;

    void setColor(Color color);

    const glm::vec2 &getSize() const;

    void setSize(const glm::vec2 &size);

    float getAlpha() const;

    void setAlpha(float alpha);

    void setScale(const glm::vec2 &scale);

private:
};


#endif //ENGINE_GUIRECT_H
