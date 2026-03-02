//
// Created by Joseph Alai on 7/18/21.
//

#ifndef ENGINE_GUIRENDERER_H
#define ENGINE_GUIRENDERER_H

#include "../../../RenderEngine/Loader.h"
#include "../GuiTexture.h"
#include "GuiShader.h"

class GuiRenderer {
private:
    RawModel *quad;
    GuiShader *shader;
public:
    explicit GuiRenderer(Loader *loader);

    void render(std::vector<GuiTexture*> guis);

    void cleanUp();

    void prepareInstance(GuiTexture* gui);

    void render(GuiTexture *gui);
private:
};


#endif //ENGINE_GUIRENDERER_H
