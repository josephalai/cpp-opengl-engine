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
#include "../Guis/Text/FontMeshCreator/TextMeshData.h"
#include "../Guis/Text/FontMeshCreator/FontModel.h"
#include "../Entities/Player.h"
#include "../RenderEngine/DisplayManager.h"

UISystem::UISystem(MasterRenderer*            renderer,
                   std::vector<Interactive*>& allBoxes,
                   GUIText*                   clickColorText,
                   FontModel*                 fontModel,
                   FontType*                  noodleFont,
                   GuiComponent*              masterContainer,
                   GuiRenderer*               guiRenderer,
                   std::vector<GuiTexture*>&  guis)
    : renderer_(renderer)
    , allBoxes_(allBoxes)
    , clickColorText_(clickColorText)
    , fontModel_(fontModel)
    , noodleFont_(noodleFont)
    , masterContainer_(masterContainer)
    , guiRenderer_(guiRenderer)
    , guis_(guis)
{}

void UISystem::update(float /*deltaTime*/) {
    // Handle object picking on left-click
    if (InputMaster::hasPendingClick() && InputMaster::mouseClicked(LeftClick)) {
        renderer_->renderBoundingBoxes(allBoxes_);
        Color clickColor = Picker::getColor();
        int element      = BoundingBoxIndex::getIndexByColor(clickColor);

        if (clickColorText_ && fontModel_ && noodleFont_) {
            *clickColorText_ = GUIText(
                ColorName::toString(clickColor) + ", Element: " + std::to_string(element),
                0.5f, fontModel_, noodleFont_, glm::vec2(10.0f, 20.0f), clickColor,
                0.75f * static_cast<float>(DisplayManager::Width()), false);
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
