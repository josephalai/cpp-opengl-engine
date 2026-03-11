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
    // Handle object picking on left-click
    if (InputMaster::hasPendingClick() && InputMaster::mouseClicked(LeftClick)) {
        // Render Player's bounding box only (Player is the only pickable object)
        renderer_->renderBoundingBoxesFromRegistry(registry_, player_);
        Color clickColor = Picker::getColor();
        int element      = BoundingBoxIndex::getIndexByColor(clickColor);

        if (clickColorText_) {
            clickColorText_->updateText(
                ColorName::toString(clickColor) + ", Element: " + std::to_string(element),
                clickColor);
        }

        Entity* pClickedModel = InteractiveModel::getInteractiveBox(element);
        if (pClickedModel) {
            if (auto* p = dynamic_cast<Player*>(pClickedModel)) {
                if (!p->hasMaterial()) {
                    p->setMaterial({500.0f, 500.0f});
                    p->activateMaterial();
                } else {
                    p->disableMaterial();
                }
            }
            printf("position: x, y, z: (%f, %f, %f)\n",
                   pClickedModel->getPosition().x,
                   pClickedModel->getPosition().y,
                   pClickedModel->getPosition().z);
        }
        InputMaster::resetClick();
    }

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
}
