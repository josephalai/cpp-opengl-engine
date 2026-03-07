// src/Engine/UISystem.cpp
//
// Phase 2 Step 3 — Pure Systems.
//   allBoxes is built from registry views each frame instead of being
//   passed as a constructor argument.

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
#include "../ECS/Components/EntityOwnerComponent.h"
#include "../Interfaces/Interactive.h"
#include "../Entities/Entity.h"

UISystem::UISystem(MasterRenderer*            renderer,
                   entt::registry&            registry,
                   GUIText*                   clickColorText,
                   GuiComponent*              masterContainer,
                   GuiRenderer*               guiRenderer,
                   std::vector<GuiTexture*>&  guis)
    : renderer_(renderer)
    , registry_(registry)
    , clickColorText_(clickColorText)
    , masterContainer_(masterContainer)
    , guiRenderer_(guiRenderer)
    , guis_(guis)
{}

void UISystem::update(float /*deltaTime*/) {
    // Handle object picking on left-click
    if (InputMaster::hasPendingClick() && InputMaster::mouseClicked(LeftClick)) {
        // Build allBoxes from registry on demand for picking.
        // Only Entity objects (which implement Interactive) are pickable.
        std::vector<Interactive*> allBoxes;
        {
            auto eView = registry_.view<EntityOwnerComponent>();
            for (auto [e, eoc] : eView.each()) {
                if (eoc.ptr && eoc.ptr->getBoundingBox()) allBoxes.push_back(eoc.ptr);
            }
        }

        renderer_->renderBoundingBoxes(allBoxes);
        Color clickColor = Picker::getColor();
        int element      = BoundingBoxIndex::getIndexByColor(clickColor);

        if (clickColorText_) {
            clickColorText_->updateText(
                ColorName::toString(clickColor) + ", Element: " + std::to_string(element),
                clickColor);
        }

        Interactive* pClickedModel = InteractiveModel::getInteractiveBox(element);
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
