//
// Created by Joseph Alai on 7/22/21.
//

#ifndef ENGINE_FONTRENDERER_H
#define ENGINE_FONTRENDERER_H


#include "FontShader.h"
#include "../../../RenderEngine/Loader.h"
#include "../GUIText.h"
#include "../../../Toolbox/Color.h"

class FontRenderer {
private:
    FontShader *shader;
public:
    FontRenderer();
    void cleanUp();
    void render(std::map<FontType*, std::vector<GUIText*>> *texts);
    void render(GUIText *text);
    void renderTextMesh(GUIText *text);
    void endRendering();
    void prepareText(GUIText *text);
    void prepare();
};


#endif //ENGINE_FONTRENDERER_H
