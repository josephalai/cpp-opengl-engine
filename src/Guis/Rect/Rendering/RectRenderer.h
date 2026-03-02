//
// Created by Joseph Alai on 3/1/22.
//

#ifndef ENGINE_RECTRENDERER_H
#define ENGINE_RECTRENDERER_H

#include "../../../RenderEngine/Loader.h"
#include "../GuiRect.h"
#include "RectShader.h"

class RectRenderer {
private:
    RawModel *quad;
    RectShader *shader;
public:
    explicit RectRenderer(Loader *loader);

    void render(std::vector<GuiRect*> guis);

    void render(GuiRect* gui);

    void cleanUp();

    void prepareInstance(GuiRect* gui);
private:
};


#endif //ENGINE_RECTRENDERER_H
