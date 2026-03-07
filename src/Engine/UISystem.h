// src/Engine/UISystem.h
// Subsystem that handles GUI texture rendering, text rendering, UiMaster,
// constraint application, and click-based object picking each frame.
//
// Phase 2 Step 3 — Pure Systems:
//   allBoxes is no longer a constructor argument.  UISystem builds the list
//   of pickable Interactive* objects from registry views each frame.

#ifndef ENGINE_UISYSTEM_H
#define ENGINE_UISYSTEM_H

#include "ISystem.h"
#include <entt/entt.hpp>
#include <vector>
#include <glm/glm.hpp>

class MasterRenderer;
class GUIText;
class GuiComponent;
class GuiRenderer;
class GuiTexture;

class UISystem : public ISystem {
public:
    UISystem(MasterRenderer*            renderer,
             entt::registry&            registry,
             GUIText*                   clickColorText,
             GuiComponent*              masterContainer,
             GuiRenderer*               guiRenderer,
             std::vector<GuiTexture*>&  guis);

    void init()     override {}
    void update(float deltaTime) override;
    void shutdown() override {}

private:
    MasterRenderer*           renderer_;
    entt::registry&           registry_;
    GUIText*                  clickColorText_;
    GuiComponent*             masterContainer_;
    GuiRenderer*              guiRenderer_;
    std::vector<GuiTexture*>& guis_;
};

#endif // ENGINE_UISYSTEM_H
