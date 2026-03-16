// src/Config/UiLayoutLoader.h
//
// Phase 1 — Data-driven UI layouts.
//
// Parses a UI definition JSON file and builds the UiMaster component hierarchy
// dynamically.  This removes the need to hardcode GuiRect / GuiTexture /
// GUIText placements in C++.
//
// JSON format (example: ui_inventory.json):
// {
//   "name": "inventory_panel",
//   "visible": false,
//   "constraints": {
//     "x": 0.75, "y": 0.0,
//     "width": 0.25, "height": 0.5,
//     "anchor": "top_right"
//   },
//   "children": [
//     {
//       "type": "rect",
//       "name": "background",
//       "color": [0.08, 0.08, 0.08],
//       "alpha": 0.9,
//       "constraints": { "x": 0, "y": 0, "width": 1.0, "height": 1.0 }
//     },
//     {
//       "type": "text",
//       "name": "title",
//       "text": "Inventory",
//       "font_size": 1.2,
//       "constraints": { "x": 0.05, "y": 0.02 }
//     }
//   ]
// }

#ifndef ENGINE_UILAYOUTLOADER_H
#define ENGINE_UILAYOUTLOADER_H

#include <string>
#include <nlohmann/json.hpp>

class GuiComponent;
class FontModel;
class FontType;
class Loader;

class UiLayoutLoader {
public:
    /// Parse a UI layout JSON file and attach the resulting component hierarchy
    /// as a child of @p parent.  Returns the newly created root component, or
    /// nullptr on failure (e.g. missing file, unknown widget type).
    ///
    /// @param jsonPath  Absolute path to the JSON layout file.
    /// @param parent    UiMaster component to attach the panel to.
    /// @param font      Font to use for text widgets (may be nullptr if no text).
    /// @param fontType  Font type for text rendering.
    /// @param loader    Resource loader (for texture loading if needed).
    static GuiComponent* load(const std::string& jsonPath,
                               GuiComponent*      parent,
                               FontModel*         font     = nullptr,
                               FontType*          fontType = nullptr,
                               Loader*            loader   = nullptr);

private:
    static GuiComponent* buildComponent(const nlohmann::json& node,
                                         GuiComponent*         parent,
                                         FontModel*            font,
                                         FontType*             fontType,
                                         Loader*               loader);
};

#endif // ENGINE_UILAYOUTLOADER_H
