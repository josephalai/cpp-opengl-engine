// src/Engine/UISystem.h
// Subsystem that handles GUI texture rendering, text rendering, UiMaster,
// constraint application, and click-based object picking each frame.

#ifndef ENGINE_UISYSTEM_H
#define ENGINE_UISYSTEM_H

#include "ISystem.h"
#include <vector>
#include <glm/glm.hpp>

class MasterRenderer;
class Entity;
class GUIText;
class GuiComponent;
class GuiRenderer;
class GuiTexture;

class UISystem : public ISystem {
public:
    UISystem(MasterRenderer*            renderer,
             std::vector<Entity*>&      entities,
             GUIText*                   clickColorText,
             GuiComponent*              masterContainer,
             GuiRenderer*               guiRenderer,
             std::vector<GuiTexture*>&  guis);

    void init()     override {}
    void update(float deltaTime) override;
    void shutdown() override {}

private:
    MasterRenderer*            renderer_;
    std::vector<Entity*>&      entities_;
    GUIText*                   clickColorText_;
    GuiComponent*              masterContainer_;
    GuiRenderer*               guiRenderer_;
    std::vector<GuiTexture*>&  guis_;
};

#endif // ENGINE_UISYSTEM_H
