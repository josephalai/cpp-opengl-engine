// src/Config/UiLayoutLoader.cpp

#include "UiLayoutLoader.h"
#include "../Guis/UiMaster.h"
#include "../Guis/GuiComponent.h"
#include "../Guis/Rect/GuiRect.h"
#include "../Guis/Texture/GuiTexture.h"
#include "../Guis/Text/GUIText.h"
#include "../Guis/Text/Rendering/TextMaster.h"
#include "../Guis/Constraints/UiConstraints.h"
#include "../Guis/Constraints/UiNormalizedConstraint.h"
#include "../Toolbox/Color.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Helper: build UiConstraints from a JSON "constraints" sub-object.
// Keys: x, y, width, height (all normalised 0..1 relative to parent).
// ---------------------------------------------------------------------------
static UiConstraints* constraintsFromJson(const json& c,
                                           float parentW = 800.0f,
                                           float parentH = 600.0f) {
    float x = c.value("x",      0.0f);
    float y = c.value("y",      0.0f);
    float w = c.value("width",  0.0f);
    float h = c.value("height", 0.0f);
    (void)parentW; (void)parentH;
    return new UiConstraints(
        new UiNormalizedConstraint(XAxis, x),
        new UiNormalizedConstraint(YAxis, y),
        w, h);
}

// ---------------------------------------------------------------------------
// Helper: parse [r, g, b] JSON array into Color.
// ---------------------------------------------------------------------------
static Color colorFromJson(const json& arr, float defaultVal = 1.0f) {
    if (arr.is_array() && arr.size() >= 3) {
        return Color(arr[0].get<float>(),
                     arr[1].get<float>(),
                     arr[2].get<float>());
    }
    return Color(defaultVal, defaultVal, defaultVal);
}

// ---------------------------------------------------------------------------
// Recursive component builder
// ---------------------------------------------------------------------------
GuiComponent* UiLayoutLoader::buildComponent(const json&   node,
                                              GuiComponent* parent,
                                              FontModel*    font,
                                              FontType*     fontType,
                                              Loader*       loader) {
    if (!node.contains("type")) {
        std::cerr << "[UiLayoutLoader] Node missing 'type' field — skipped.\n";
        return nullptr;
    }

    std::string type = node.value("type", "container");
    std::string name = node.value("name", "unnamed");

    UiConstraints* c = node.contains("constraints")
        ? constraintsFromJson(node["constraints"])
        : new UiConstraints(0.0f, 0.0f, 0.0f, 0.0f);

    GuiComponent* comp = nullptr;

    if (type == "container") {
        comp = new GuiComponent(Container::CONTAINER, c);
        comp->setName(name);
        if (parent) {
            parent->addChild(comp, c);
        }

    } else if (type == "rect") {
        Color col = node.contains("color")
            ? colorFromJson(node["color"])
            : Color(0.2f, 0.2f, 0.2f);
        float alpha = node.value("alpha", 1.0f);
        glm::vec2 pos  = c->getConstraintPosition();
        glm::vec2 size = c->getSize();
        glm::vec2 scale(1.0f);
        auto* rect = new GuiRect(col, pos, size, scale, alpha);
        rect->setName(name);
        if (parent) parent->addChild(rect, c);
        UiMaster::addToLayerQueue(rect);
        comp = rect;

    } else if (type == "text") {
        if (!font || !fontType) {
            std::cerr << "[UiLayoutLoader] text widget '" << name
                      << "' requires font — skipped.\n";
            return nullptr;
        }
        std::string txt       = node.value("text", "");
        float       fontSize  = node.value("font_size", 1.0f);
        bool        centered  = node.value("centered", false);
        Color       col       = node.contains("color")
            ? colorFromJson(node["color"], 1.0f)
            : Color(1.0f, 1.0f, 1.0f);
        glm::vec2   pos       = c->getConstraintPosition();
        float       maxLine   = node.value("max_line_length", 0.5f);

        auto* text = new GUIText(txt, fontSize, font, fontType, pos,
                                  col, maxLine, centered);
        text->setName(name);
        TextMaster::loadText(text);
        if (parent) parent->addChild(text, c);
        UiMaster::addToLayerQueue(text);
        comp = text;

    } else {
        std::cerr << "[UiLayoutLoader] Unknown widget type '" << type
                  << "' — skipped.\n";
        return nullptr;
    }

    // Recurse into children.
    if (comp && node.contains("children")) {
        for (const auto& child : node["children"]) {
            buildComponent(child, comp, font, fontType, loader);
        }
    }

    return comp;
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------
GuiComponent* UiLayoutLoader::load(const std::string& jsonPath,
                                    GuiComponent*      parent,
                                    FontModel*         font,
                                    FontType*          fontType,
                                    Loader*            loader) {
    std::ifstream f(jsonPath);
    if (!f.is_open()) {
        std::cerr << "[UiLayoutLoader] Cannot open '" << jsonPath << "'\n";
        return nullptr;
    }

    json root;
    try {
        f >> root;
    } catch (const json::parse_error& e) {
        std::cerr << "[UiLayoutLoader] JSON parse error in '" << jsonPath
                  << "': " << e.what() << "\n";
        return nullptr;
    }

    // Top-level node is treated as a container.
    if (!root.contains("type")) root["type"] = "container";

    GuiComponent* panel = buildComponent(root, parent, font, fontType, loader);

    // Visibility.
    bool vis = root.value("visible", true);
    if (panel && !vis) panel->hide();

    if (panel) {
        UiMaster::applyConstraints(parent ? parent : UiMaster::getMasterComponent());
        std::cout << "[UiLayoutLoader] Loaded '" << jsonPath << "'\n";
    }
    return panel;
}
