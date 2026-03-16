// src/Engine/UISystem.cpp

#include "UISystem.h"
#include "../RenderEngine/MasterRenderer.h"
#include "../Guis/UiMaster.h"
#include "../Guis/GuiComponent.h"
#include "../Guis/Texture/Rendering/GuiRenderer.h"
#include "../Guis/Texture/GuiTexture.h"
#include "../Guis/Text/Rendering/TextMaster.h"
#include "../Input/InputMaster.h"
#include "../Toolbox/Picker.h"
#include "../BoundingBox/BoundingBoxIndex.h"
#include "../Interaction/InteractiveModel.h"
#include "../Util/ColorName.h"
#include "../Guis/Text/GUIText.h"
#include "../Entities/Player.h"
#include "../RenderEngine/DisplayManager.h"

UISystem::UISystem(MasterRenderer*            renderer,
                   entt::registry&            registry,
                   Player*                    player,
                   GUIText*                   clickColorText,
                   GuiComponent*              masterContainer,
                   GuiRenderer*               guiRenderer,
                   std::vector<GuiTexture*>&  guis)
    : renderer_(renderer)
    , registry_(registry)
    , player_(player)
    , clickColorText_(clickColorText)
    , masterContainer_(masterContainer)
    , guiRenderer_(guiRenderer)
    , guis_(guis)
{}

void UISystem::update(float /*deltaTime*/) {
    // Note: Left-click entity picking is now handled by EntityPicker (ray-cast)
    // via InputDispatcher → EntityClickedEvent.  The old colour-picking path
    // (renderBoundingBoxesFromRegistry → Picker::getColor) has been removed
    // because calling render() outside of an FBO caused a full-screen flash on
    // every click.  The reflection-FBO bounding box render in RenderSystem is
    // intentionally kept for the minimap / water-reflection pass.

    // Render GUI texture overlays (lifebar, icons, FBO debug, etc.)
    if (guiRenderer_) guiRenderer_->render(guis_);

    // Render UiMaster components (constraints-based UI)
    UiMaster::render();

    // Render text registered via TextMaster::loadText()
    TextMaster::render();

    // Slide the master container slightly each frame (animated UI demo)
    if (masterContainer_) {
        masterContainer_->getConstraints()->getPosition() += glm::vec2(0.001f, 0.0f);
        UiMaster::applyConstraints(masterContainer_);
    }

    // ----------------------------------------------------------------
    // Phase 2: OSRS-style right-click context menu
    // ----------------------------------------------------------------
    UiMaster::renderContextMenu();

    // ----------------------------------------------------------------
    // Phase 3: Spatial chat box
    // ----------------------------------------------------------------
    UiMaster::renderChatBox();

    // ----------------------------------------------------------------
    // Phase 4: Inventory grid
    // ----------------------------------------------------------------
    UiMaster::renderInventory();

    // ----------------------------------------------------------------
    // Phase 5: Skills panel
    // ----------------------------------------------------------------
    UiMaster::renderSkillsPanel();
}
