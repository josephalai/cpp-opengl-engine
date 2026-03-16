//
// Created by Joseph Alai on 2/27/22.
//

#include "UiMaster.h"
#include "Texture/GuiTexture.h"
#include "Text/GUIText.h"
#include "Text/Rendering/TextMaster.h"
#include "../Toolbox/Logger/Log.h"
#include "../Util/CommonHeader.h"
#include "Constraints/UiPercentConstraint.h"
#include "Constraints/UiNormalizedConstraint.h"
#include "ContextMenu/ContextMenu.h"
#include "Chat/ChatBox.h"
#include "Inventory/InventoryGrid.h"
#include "Skills/SkillsPanel.h"
#include "../Input/InputMaster.h"

GuiComponent *UiMaster::masterContainer;
UiConstraints *UiMaster::masterConstraints;

GuiRenderer *UiMaster::guiRenderer;
RectRenderer *UiMaster::rectRenderer;
FontRenderer *UiMaster::fontRenderer;
Loader *UiMaster::loader;

std::map<int, Container *> UiMaster::groupMap = std::map<int, Container *>();
std::map<int, int> UiMaster::tmpGroupMap = std::map<int, int>();
int UiMaster::group = 0;


void UiMaster::applyConstraints() {
    applyConstraints(masterContainer);
}

/**
 * @brief This function is to be utilized for children only. Said, the original parent is the master parent, and has
 *        simply master attributes. Anything to be done to the master parent is NOT to be done or called in this fn.
 *
 * @param parentComponent
 */
void UiMaster::applyConstraints(GuiComponent *parentComponent) {
    // iterate through children of this parentComponent and apply the constraints to them.

    // traverse through children
    for (Container *childComponent : parentComponent->SortChildrenByLayer(true)) {

        childComponent->constraints->parentPosition =
                parentComponent->constraints->parentPosition + parentComponent->constraints->position;
        // std::cout << "Position of " << childComponent->getName() << " is " << childComponent->constraints->parentPosition.x << ", " << childComponent->constraints->parentPosition.y << std::endl ;
        // std::cout << "PARENT of ^^ : " << childComponent->getParent()->getName() << " is " << childComponent->getParent()->constraints->parentPosition.x << ", " << childComponent->getParent()->constraints->parentPosition.y << std::endl ;

        // check to see which type the parentComponent is: and perform the action based on that.
        switch (childComponent->getType()) {
            case Container::IMAGE: {
                auto *p = dynamic_cast<GuiTexture *>(childComponent);
                p->constraints->parentPosition =
                        parentComponent->constraints->parentPosition + parentComponent->constraints->position;

                break;
            }
            case Container::TEXT: {
                auto *p = dynamic_cast<GUIText *>(childComponent);
                p->constraints->parentPosition =
                        parentComponent->constraints->parentPosition + parentComponent->constraints->position;

                break;
            }
            case Container::COLORED_BOX: {
                auto *p = dynamic_cast<GuiRect *>(childComponent);
                p->constraints->parentPosition =
                        parentComponent->constraints->parentPosition + parentComponent->constraints->position;

                break;
            }
            case Container::CONTAINER: {
                auto *p = dynamic_cast<GuiComponent *>(childComponent);
                p->constraints->parentPosition =
                        parentComponent->constraints->parentPosition + parentComponent->constraints->position;

                break;
            }
        }

        auto *pComponent = dynamic_cast<GuiComponent *>(childComponent);
        if (pComponent != nullptr) {
            UiMaster::applyConstraints(pComponent);
        }
    }

}

/**
 * @brief This function is to be utilized for children only. Said, the original parent is the master parent, and has
 *        simply master attributes. Anything to be done to the master parent is NOT to be done or called in this fn.
 *
 * @param component
 */
void UiMaster::createRenderQueue(GuiComponent *component) {
    // iterate through children of this component and apply the constraints to them.
    int i = 0;
    for (Container *c : component->SortChildrenByLayer(true)) {
        UiMaster::addToLayerQueue(dynamic_cast<GuiComponent *>(c));
        UiMaster::createRenderQueue(dynamic_cast<GuiComponent *>(c));
    }
}

GuiComponent *UiMaster::getMasterComponent() {
    return masterContainer;
}

UiConstraints *UiMaster::getMasterConstraints() {
    return masterConstraints;
}

void UiMaster::initialize(Loader *loader, GuiRenderer *guiTextureRenderer, FontRenderer *fontRenderer,
                          RectRenderer *rectRenderer) {
    masterConstraints = new UiConstraints(new UiNormalizedConstraint(XAxis, 0.0f), new UiNormalizedConstraint(YAxis, 0.0f),
                                          static_cast<float>(DisplayManager::Width()),
                                          static_cast<float>(DisplayManager::Height()));
    masterContainer = new GuiComponent(Container::CONTAINER, masterConstraints);
    masterContainer->setName("master");
    // add the master container as the first to be rendered
    renderOrder.push_back(masterContainer);

    // this loader is used for the gui renderers
    UiMaster::loader = loader;
    UiMaster::guiRenderer = guiTextureRenderer;
    UiMaster::rectRenderer = rectRenderer;
    UiMaster::fontRenderer = fontRenderer;
}

std::vector<Container *> UiMaster::renderOrder = std::vector<Container *>();

/**
 * Adds this layer to the layer queue because it is in order.
 * @param component
 */
void UiMaster::addToLayerQueue(GuiComponent *component) {
    renderOrder.push_back(component);
}

// fixme: some components do not show up on some renders (randomly or so it seems).
// todo: Should we proceed to only not render if it is visible, in here? I think so. Added: 3/3/2022
/**
 * This is what was just added, thus we should test this by removing the render of the other components.
 */
void UiMaster::render() {
    int i = 0;

    std::sort(renderOrder.begin(), renderOrder.end(),
              [](Container * a, Container * b) -> bool
              { return a->getLayer() < b->getLayer(); } );

    for (Container *c: renderOrder) {
        // std::cout << "renderOrder: " << c->getName() << ", " << c->getLayer() << std::endl;
        {
            if (c->isHidden()) {
                // std::cout << c->getName() << " is hidden" << std::endl;
                continue;
            }

            // check to see which type the component is: and perform the action based on that.
            switch (c->getType()) {
                case Container::IMAGE: {
                    auto *p = dynamic_cast<GuiTexture *>(c);
                    guiRenderer->render(p);
                    break;
                }
                case Container::TEXT: {
                    auto *p = dynamic_cast<GUIText *>(c);
                    fontRenderer->render(p);
                    break;
                }
                case Container::COLORED_BOX: {
                    auto *p = dynamic_cast<GuiRect *>(c);
                    rectRenderer->render(p);
                    break;
                }
                case Container::CONTAINER: {
                    auto *p = dynamic_cast<GuiComponent *>(c);
                    break;
                }
            }
        }
    }
}

int UiMaster::newGroup() {
    group += 1;
    return group;
}

int UiMaster::getCurrentGroup() {
    return group;
}

void UiMaster::setGroup(int group, Container *container) {
    groupMap[group] = container;
}

const Container *UiMaster::getGroupMap(int group) {
    return groupMap[group];
}

void UiMaster::createRenderQueue() {
    createRenderQueue(getMasterComponent());
}

void UiMaster::cleanUp() {}

void UiMaster::printComponentPosition(Container *childComponent) {}

void UiMaster::printComponentInformation(GuiComponent *parentComponent) {}

// ---------------------------------------------------------------------------
// Phase 1 — UI region registration & mouse-over detection
// ---------------------------------------------------------------------------

std::vector<float> UiMaster::uiRegions_ = {};

void UiMaster::registerUiRegion(float x, float y, float w, float h) {
    uiRegions_.push_back(x);
    uiRegions_.push_back(y);
    uiRegions_.push_back(w);
    uiRegions_.push_back(h);
}

void UiMaster::clearUiRegions() {
    uiRegions_.clear();
}

bool UiMaster::isMouseOverUi() {
    double mx = InputMaster::mouseX;
    double my = InputMaster::mouseY;

    // Check every registered pixel rectangle.
    for (std::size_t i = 0; i + 3 < uiRegions_.size(); i += 4) {
        float rx = uiRegions_[i];
        float ry = uiRegions_[i + 1];
        float rw = uiRegions_[i + 2];
        float rh = uiRegions_[i + 3];
        if (mx >= rx && mx <= rx + rw &&
            my >= ry && my <= ry + rh) {
            return true;
        }
    }

    // Also consume clicks when the context menu is under the cursor.
    if (ContextMenu::instance().isMouseOver()) return true;

    return false;
}

// ---------------------------------------------------------------------------
// Phase 2 — Context menu render
// ---------------------------------------------------------------------------

void UiMaster::renderContextMenu() {
    ContextMenu::instance().render();
}

// ---------------------------------------------------------------------------
// Phase 3 — Chat box render
// ---------------------------------------------------------------------------

void UiMaster::renderChatBox() {
    ChatBox::instance().render();
}

// ---------------------------------------------------------------------------
// Phase 4 — Inventory render
// ---------------------------------------------------------------------------

void UiMaster::renderInventory() {
    InventoryGrid::instance().render();
}

// ---------------------------------------------------------------------------
// Phase 5 — Skills panel render
// ---------------------------------------------------------------------------

void UiMaster::renderSkillsPanel() {
    SkillsPanel::instance().render();
}
