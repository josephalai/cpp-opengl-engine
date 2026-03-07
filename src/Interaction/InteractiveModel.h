//
// Created by Joseph Alai on 7/30/21.
//

#ifndef ENGINE_INTERACTIVEMODEL_H
#define ENGINE_INTERACTIVEMODEL_H

#include <map>

class Entity;

class InteractiveModel {
    static std::map<int, Entity *> interactiveBox;
public:
    static Entity* getInteractiveBox(int index);

    static void setInteractiveBox(int index, Entity *entity);

    static void setInteractiveBox(Entity *entity);

    static int getIndexByInteractive(Entity *entity);
};


#endif //ENGINE_INTERACTIVEMODEL_H
