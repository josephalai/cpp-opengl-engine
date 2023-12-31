//
// Created by Joseph Alai on 7/3/21.
//

#ifndef ENGINE_LIGHT_H
#define ENGINE_LIGHT_H

#include "glm/glm.hpp"

using namespace glm;

struct Lighting {
    vec3 position;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

class Light {
private:
    vec3 position;
    vec3 color;
    Lighting lighting;
public:
    explicit Light(const vec3 position = vec3(1.2f, 1.0f, 2.0f), const vec3 color = vec3(1.0f, 1.0f, 1.0f),
          Lighting lighting = {
            .ambient =  glm::vec3(0.2f, 0.2f, 0.2f),
            .diffuse =  glm::vec3(1.0f, 1.0f, 1.0f),
            .specular =  glm::vec3(1.0f, 1.0f, 1.0f)})
            : position(position), color(color), lighting(lighting) {
        lighting.position = this->position;
    }

    Lighting getLighting() const {
        return lighting;
    }

    void setLighting(const Lighting &lighting) {
        this->lighting = lighting;
    }

    vec3 getPosition() {
        return this->lighting.position;
    }

    void setPosition(vec3 position) {
        this->lighting.position = position;
    }

    vec3 getColor() const {
        return color;
    }

    void setColor(vec3 color) {
        this->color = color;
    }
};

#endif //ENGINE_LIGHT_H
