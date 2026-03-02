//
// Created by Joseph Alai on 2/26/22.
//

#ifndef ENGINE_CONTAINER_H
#define ENGINE_CONTAINER_H

#include <vector>
#include "Constraints/UiConstraints.h"

class Container {
public:
    enum GuiType {
        TEXT,
        IMAGE,
        COLORED_BOX,
        CONTAINER
    };
public:
    Container *parent = nullptr;
    GuiType type;
    std::string name;
    uint groupId;
    uint childId;
    std::vector<Container *> children;
    std::vector<Container *> childrenToAdd;
    std::vector<Container *> childrenToRemove;
    UiConstraints *constraints;
public:

protected:
    bool sterile = false;

    int layer = -1;
    bool hidden = false;
public:
    /**
     * @brief Initializes a basic container, which is similar to a linked list. Specifies what type of container it is,
     *        and if it is capable of having children.
     * @param type
     * @param sterile
     */
    explicit Container(GuiType type, bool sterile, std::string name = "component") : type(type), sterile(sterile),
                                                                                     name(name), constraints(new UiConstraints()) {
        childrenToRemove = std::vector<Container *>();
        children = std::vector<Container *>();
        childrenToAdd = std::vector<Container *>();
    }

    explicit Container(int layer) {
        this->layer = layer;
    }
    virtual ~Container() {}
    const std::string &getName() const {
        return name;
    }

    /**
     * @brief set the name of this component for identification purposes.
     *
     * @param name
     */
    void setName(const std::string &name) {
        Container::name = name;
    }

    /**
     * @brief Gets the component type: TEXT, IMAGE, COLORED_SQUARE, CONTAINER
     * @return
     */
    GuiType getType() const {
        return type;
    }

    /**
     * @brief Gets the parent of this component.
     * @return
     */
    Container *getParent() { return parent; }

    /**
     * @brief Sets the parent to this component.
     * @param parent
     */
    void setParent(Container *parent) { this->parent = parent; }

    /**
     * @brief Adds a child component to this container.
     *
     * @param child
     * @param constraints
     */
    void addChild(Container *child, UiConstraints *constraints) {
        child->setParent(this);
        child->constraints = constraints;
        childrenToAdd.push_back(child);
    }

    /**
     * @brief adds and removes children components from this container.
     *        Then sorts the children by layer id.
     */
    void initialize() {
        std::vector<Container *>::iterator it;
        for (it = children.begin(); it < children.end(); it++) {
            for (Container *c:childrenToRemove) {
                if (*it == c) {
                    children.erase(it);
                }
            }
            (*(it))->initialize();
        }
        for (Container *c:childrenToAdd) {
            children.push_back(c);
            c->initialize();
        }
        childrenToAdd.clear();
        std::cout << "added: " << children.size() << std::endl;
//        SortChildrenByLayer(true);
    }

    /**
     * @brief Gets the constraints (size & position) of the component to be applied to itself and its' children.
     *
     * // todo: Should a constrained size smaller than the component itself either truncate or shrink the component?
     *
     * @return
     */
    UiConstraints *getConstraints() {
        return constraints;
    }

    /**
     * @brief Gets the constraints (size & position) of the component to be applied to itself and its' children.
     *
     * // todo: Should a constrained size smaller than the component itself either truncate or shrink the component?
     *
     * @return
     */
    void setConstraints(UiConstraints *constraints) {
        this->constraints = constraints;
    }

    /**
     * @brief Queues up this child to be removed from this container.
     *
     * @param child
     */
    void removeChild(Container *child) { childrenToRemove.push_back(child); }

    /**
     * @brief Removes from the parent.
     */
    void remove() { this->parent->removeChild(this); }

    /**
     * @brief Gets the children that are in queue to be added to the component.
     *
     * @return
     */
    std::vector<Container *> getChildrenToAdd() { return childrenToAdd; }

    /**
     * @brief Gets the children that are in queue to be added to the component.
     *
     * @return
     */
    std::vector<Container *> getChildren() { return children; }

    bool isSterile() {
        return sterile;
    }

    /**
     * @brief Sets if this component is capable of having children, or sub-components.
     * @param sterile
     */
    void setSterile(bool sterile = true) {
        this->sterile = sterile;
    }

    /**
     * @brief Delete everything within this component.
     */
    void deleteMe() {
        for (Container *component : children) {
            component->removeOldComponents();
        }
        for (Container *component : childrenToAdd) {
            component->removeOldComponents();
        }
        for (Container *component : childrenToRemove) {
            component->removeOldComponents();
        }
    }

    /**
     * @brief removes my sub-components, and then removes me from my parent.
     */
    void removeOldComponents() {
        int index = 0;
        while (index < childrenToRemove.size()) {
            Container *component = childrenToRemove.at(index++);
            children.erase(children.begin() + index);
            component->deleteMe();
        }
        childrenToRemove.clear();
    }

    /**
     * @brief gets the layer order. The lower the number, the sooner the render. Higher numbers appear on top of lower
     *        numbers.
     *
     * @return
     */
    int getLayer() const {
        return layer;
    }

    /**
     * @brief Sets the layer of the item
     * @details The layer is technically the order in which the components are rendered, from back to front.
     *          Meaning, if something is layered 0, that component is rendered BEHIND something that is layered 1.
     *
     * @param layer
     */
    void setLayer(int layer) {
        Container::layer = layer;
    }


    static bool sortByName(Container *A, Container *B) //function to sort fruits by names
    {
        return (A->name < B->name);
    }

    bool operator < (const Container &a) const{
        return layer <a.layer;
    }

//    /**
//     * @brief Comparatory "greater than" function, for sort.
//     * @param other
//     *
//     * @return
//     */
//    bool operator>(const Container &other) const {
//        return layer > other.layer;
//    }

    /**
     * @brief Sorts the children by layer.
     *
     * @param recursively
     * @return
     */
    std::vector<Container *> SortChildrenByLayer(bool recursively = false) {
        std::vector<Container *> children_copy = this->children;
        std::sort(children_copy.begin(), children_copy.end());
        if (recursively) {
            for (Container *c : this->children) {
                c->SortChildrenByLayer(true);
            }
        }
        return children_copy;
    }

    bool isHidden() const {
        return hidden;
    }

    /**
     * @brief Sets whether or not this component is hidden, and whether or not to apply it to the children as well.
     *
     * @param hidden
     */
    void setHidden(bool hidden, bool inheritance = true) {
        Container::hidden = hidden;
        if (!inheritance) {
            return;
        }

        for (Container *c : this->children) {
            c->setHidden(hidden, inheritance);
        }
    }

};

#endif //ENGINE_CONTAINER_H
