// src/Culling/Frustum.cpp

#include "Frustum.h"
#include <cmath>

void Frustum::update(const glm::mat4& vp) {
    // Transpose so that vpt[i] gives row i of the VP matrix.
    glm::mat4 t = glm::transpose(vp);

    // Griggs-Hartmann plane extraction:
    //   Left   =  row3 + row0
    //   Right  =  row3 - row0
    //   Bottom =  row3 + row1
    //   Top    =  row3 - row1
    //   Near   =  row3 + row2
    //   Far    =  row3 - row2
    planes_[0] = t[3] + t[0]; // left
    planes_[1] = t[3] - t[0]; // right
    planes_[2] = t[3] + t[1]; // bottom
    planes_[3] = t[3] - t[1]; // top
    planes_[4] = t[3] + t[2]; // near
    planes_[5] = t[3] - t[2]; // far

    // Normalize each plane so the distance has metric meaning.
    for (int i = 0; i < 6; ++i) {
        float len = std::sqrt(planes_[i].x * planes_[i].x +
                              planes_[i].y * planes_[i].y +
                              planes_[i].z * planes_[i].z);
        if (len > 0.0f) {
            planes_[i] /= len;
        }
    }
}

bool Frustum::isAABBVisible(const glm::vec3& min, const glm::vec3& max) const {
    for (int i = 0; i < 6; ++i) {
        const glm::vec4& p = planes_[i];
        // P-vertex: the AABB corner farthest in the direction of the plane normal.
        glm::vec3 pv;
        pv.x = (p.x >= 0.0f) ? max.x : min.x;
        pv.y = (p.y >= 0.0f) ? max.y : min.y;
        pv.z = (p.z >= 0.0f) ? max.z : min.z;

        // If the P-vertex is behind this plane the entire AABB is outside.
        if (glm::dot(glm::vec3(p), pv) + p.w < 0.0f) {
            return false;
        }
    }
    return true;
}

bool Frustum::isSphereVisible(const glm::vec3& center, float radius) const {
    for (int i = 0; i < 6; ++i) {
        float d = glm::dot(glm::vec3(planes_[i]), center) + planes_[i].w;
        if (d < -radius) {
            return false;
        }
    }
    return true;
}
