//
// Created by Joseph Alai on 7/23/21.
//

#ifndef ENGINE_TEXTMASTER_H
#define ENGINE_TEXTMASTER_H

#include <iostream>
#include "FontRenderer.h"
#include "../FontMeshCreator/TextMeshData.h"
#include "../GUIText.h"
#include "../../../RenderEngine/Loader.h"

class TextMaster {
private:
    static Loader *loader;
    static std::map<FontType*, std::vector<GUIText*>> *texts;
    static FontRenderer *renderer;
public:
    static void init(Loader *theLoader);

    static void loadText(GUIText *text);

    static void remove(GUIText *text);

    static void cleanUp();

    static void render();

    static FontRenderer *getRenderer();

};


#endif //ENGINE_TEXTMASTER_H
