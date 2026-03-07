//
// Created by Joseph Alai on 7/30/21.
//

#include "../BoundingBox/BoundingBoxIndex.h"
#include "../Entities/Entity.h"
#include <iostream>
#include "InteractiveModel.h"

std::map<int, Entity *> InteractiveModel::interactiveBox;

Entity *InteractiveModel::getInteractiveBox(int index) {
    auto it = interactiveBox.find(index);
    if (it == interactiveBox.end()) {
        return nullptr;
    }
    return it->second;
}

void InteractiveModel::setInteractiveBox(int index, Entity *entity) {
    auto it = interactiveBox.find(index);
    if (it != interactiveBox.end()) {
        std::cout << "Error: Please add Alpha attribute to color picking. Already found Entity object at index: "
                  << std::to_string(index) << std::endl;
    }
    InteractiveModel::interactiveBox[index] = entity;
}

void InteractiveModel::setInteractiveBox(Entity *entity) {
    setInteractiveBox(BoundingBoxIndex::getIndexByColor(entity->getBoundingBox()->getBoxColor()), entity);
}

int InteractiveModel::getIndexByInteractive(Entity *entity) {
    return BoundingBoxIndex::getIndexByColor(entity->getBoundingBox()->getBoxColor());
}