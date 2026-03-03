// src/Engine/UISystem.h
// Subsystem that handles UI rendering, constraint application, and
// click-based object picking each frame.

#ifndef ENGINE_UISYSTEM_H
#define ENGINE_UISYSTEM_H

#include "ISystem.h"
#include <vector>
#include <glm/glm.hpp>

class MasterRenderer;
class Interactive;
class GUIText;
class FontModel;
class FontType;
class GuiComponent;
class Loader;
class Picker;
class BoundingBoxIndex;

class UISystem : public ISystem {
public:
    UISystem(MasterRenderer*            renderer,
             std::vector<Interactive*>& allBoxes,
             GUIText*                   clickColorText,
             FontModel*                 fontModel,
             FontType*                  noodleFont,
             GuiComponent*              masterContainer);

    void init()     override {}
    void update(float deltaTime) override;
    void shutdown() override {}

private:
    MasterRenderer*            renderer_;
    std::vector<Interactive*>& allBoxes_;
    GUIText*                   clickColorText_;
    FontModel*                 fontModel_;
    FontType*                  noodleFont_;
    GuiComponent*              masterContainer_;
};

#endif // ENGINE_UISYSTEM_H
