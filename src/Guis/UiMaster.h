//
// Created by Joseph Alai on 2/27/22.
//

#ifndef ENGINE_UIMASTER_H
#define ENGINE_UIMASTER_H


#include "GuiComponent.h"
#include "../RenderEngine/DisplayManager.h"
#include "Rect/Rendering/RectRenderer.h"
#include "Texture/Rendering/GuiRenderer.h"
#include "Text/Rendering/FontRenderer.h"
#include <map>

class UiMaster {
private:
    static UiConstraints *masterConstraints;
    static GuiComponent *masterContainer;

    // try to move everything GUI related into this renderer
    static Loader *loader;
    static GuiRenderer *guiRenderer;
    static RectRenderer *rectRenderer;
    static FontRenderer *fontRenderer;

    static std::map<int, Container *> groupMap;
    static int group;
    static std::map<int, int> tmpGroupMap;


public:
    static const Container *getGroupMap(int group);

    static void setGroup(int group, Container *container);

    static int getCurrentGroup();

    static int newGroup();

    /**
     * Initializes the master container.
     */
    static void
    initialize(Loader *loader, GuiRenderer *guiTextureRenderer, FontRenderer *fontRenderer, RectRenderer *rectRenderer);

    /**
     * @brief applies the constraints to the children. This is to keep the UI as groups.
     *
     * @param parentComponent must be the master PARENT parentComponent.
     */
    static void applyConstraints(GuiComponent *parentComponent);

    static void applyConstraints();

    static void createRenderQueue(GuiComponent *component);

    static void createRenderQueue();

    /**
     * @brief Gets the constraints of the master UiComponent (the container of all UI components).
     *
     * @return
     */
    static UiConstraints *getMasterConstraints();

    /**
     * @brief Gets the master component. The master component is the container which holds all other
     * Components & Containers.
     *
     * @return
     */
    static GuiComponent *getMasterComponent();

    static void addToLayerQueue(GuiComponent *component);

    static void render();

    static void cleanUp();

    static void printComponentPosition(Container *childComponent);

    static void  printComponentInformation(GuiComponent *parentComponent);

    static std::vector<Container *> renderOrder;

    // ------------------------------------------------------------------
    // Phase 1 — UI Input Consumption
    // ------------------------------------------------------------------

    /// Returns true when the mouse cursor is currently over any registered
    /// UiMaster component region.  InputDispatcher calls this before routing
    /// clicks to the 3D world to prevent accidental world interactions when
    /// the player is clicking UI elements.
    static bool isMouseOverUi();

    /// Register a screen-space pixel rectangle as a "UI region".
    /// InputDispatcher will treat clicks within any registered region as
    /// consumed (i.e. not passed to the 3D world).
    /// Coordinates are in window pixels (origin = top-left).
    static void registerUiRegion(float x, float y, float w, float h);

    /// Clear all registered UI regions (call once per frame, or when layout changes).
    static void clearUiRegions();

    // ------------------------------------------------------------------
    // Phase 2 — Context menu helpers (delegate to ContextMenu singleton)
    // ------------------------------------------------------------------

    /// Render the OSRS-style right-click context menu (if visible).
    /// Called from UISystem::update() each frame.
    static void renderContextMenu();

    // ------------------------------------------------------------------
    // Phase 3 — Chat box helper
    // ------------------------------------------------------------------

    /// Render the spatial chat box.
    static void renderChatBox();

    // ------------------------------------------------------------------
    // Phase 4 — Inventory grid helper
    // ------------------------------------------------------------------

    /// Render the inventory grid panel.
    static void renderInventory();

    // ------------------------------------------------------------------
    // Phase 5 — Skills panel helper
    // ------------------------------------------------------------------

    /// Render the skills panel.
    static void renderSkillsPanel();

    // ------------------------------------------------------------------
    // Phase 6 — Equipment panel helper
    // ------------------------------------------------------------------

    /// Render the equipment toggle panel.
    static void renderEquipmentPanel();

private:
    /// Registered 2-D UI regions (pixel rectangles) for mouse-over detection.
    /// Layout: {x, y, w, h} tuples stored flat.
    static std::vector<float> uiRegions_;
};


#endif //ENGINE_UIMASTER_H
