//
// Created by Joseph Alai on 7/23/21.
//

#include "TextMaster.h"

std::map<FontType*, std::vector<GUIText*>> *TextMaster::texts;
Loader *TextMaster::loader = nullptr;
FontRenderer *TextMaster::renderer;

void TextMaster::init(Loader *theLoader) {
    renderer = new FontRenderer();
    loader = theLoader;
    texts = new std::map<FontType*, std::vector<GUIText*>>();
}

/**
 * @brief adds the GUIText* to a map[FontType]std::vector<GUIText*>{} map.
 *
 * @param text
 */
void TextMaster::loadText(GUIText *text) {
    FontType *font = text->getFontType();
    auto batchIterator = texts->find(font);
    if (batchIterator != texts->end()) {
        batchIterator->second.push_back(text);
    } else {
        std::vector<GUIText *> newBatch;
        newBatch.push_back(text);
        (*texts)[font] = newBatch;
    }
}

void TextMaster::render() {
    renderer->render(texts);
}

void TextMaster::cleanUp() {
    renderer->cleanUp();
}

void TextMaster::remove(GUIText *text) {
    FontType *font = text->getFontType();
    if (texts == nullptr) {
        std::cout << "tests (std::map<FontType*, std::vector<GUIText*>>) is empty" << std::endl;
        return;
    }
    auto mapIterator = texts->find(font);
    if (mapIterator != texts->end()) {
        auto guiVec = mapIterator->second;

        // If element was found
        guiVec.erase(std::remove(guiVec.begin(), guiVec.end(), text), guiVec.end());
        if (guiVec.empty()) {
            texts->erase(mapIterator);
        }
    } else {
        std::cout << "Font not found in map, so could not delete. Please check logic." << std::endl;
    }
}

FontRenderer *TextMaster::getRenderer() {
    return renderer;
}
