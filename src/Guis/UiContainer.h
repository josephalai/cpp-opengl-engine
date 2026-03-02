//
// Created by Joseph Alai on 2/25/22.
//

#ifndef ENGINE_UICONTAINER_H
#define ENGINE_UICONTAINER_H

#include <vector>
#include "Constraints/UiConstraints.h"
#include "Container.h"

class UiContainer : public Container {
protected:
    UiContainer *parent = nullptr;

    bool visible = true;
    bool clicked = false;
    bool mouseOver = false;
    bool displayed = true;
    bool removeFlag = false;
    bool reloadOnSizeChange = false;
    bool initialized = false;//true after adding to parent and having screen space and alpha calced
    int level = 0;
public:
    UiContainer(GuiType type) : Container(type, false) {}

    virtual void show() = 0;

    virtual void hide() = 0;

    virtual bool isDisplayed() = 0;

    // interactionable should probably be it's own interface:

    virtual bool isMouseOverChild() = 0;

    virtual bool isClicked() = 0;

    virtual bool isMouseOver() = 0;
};

#endif //ENGINE_UICONTAINER_H
