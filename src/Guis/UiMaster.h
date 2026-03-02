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
};


#endif //ENGINE_UIMASTER_H
