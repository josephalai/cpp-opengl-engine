//
// Created by Joseph Alai on 7/23/21.
//

#ifndef ENGINE_FONTMODEL_H
#define ENGINE_FONTMODEL_H
class FontModel {
private:
    unsigned int vaoId;
    unsigned int vboId;
    long vertexCount;
public:
    FontModel(unsigned int vaoId, unsigned int vboId, unsigned long vertexCount) {
        this->vaoId = vaoId;
        this->vboId = vboId;
        this->vertexCount = static_cast<long>(vertexCount);
    }
    unsigned int getVaoId() const {
        return vaoId;
    }
    unsigned int getVboId() const {
        return vboId;
    }
    int getVertexCount() const {
        return static_cast<int>(vertexCount);
    }
};
#endif //ENGINE_FONTMODEL_H
