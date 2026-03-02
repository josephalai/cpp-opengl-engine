//
// Created by Joseph Alai on 3/12/22.
//

#ifndef ENGINE_UIPIXELCONSTRAINT_H
#define ENGINE_UIPIXELCONSTRAINT_H

#include "UiConstraint.h"
#include "Tools.h"

class UiPixelConstraint : public UiConstraint {
private:
    const ConstraintType constraintType = Relative;
    const PositionType positionType = Pixel;
public:
    UiPixelConstraint(Axis axis, int value, ConstraintType constraintType = Relative)
            : constraintType(constraintType) {
        this->axis = axis;
        this->value = value;

        this->updateNormalizedValue();
    }

    /**
     * @brief Gets the constraint type: either Relative or Absolute
     *
     * @return
     */
    ConstraintType getConstraintType() const override {
        return constraintType;
    }

    /**
     * @brief Gets the position type: either Pixel or Percentage
     *
     * @return
     */
    PositionType getPositionType() const override {
        return positionType;
    }

    /**
     * @brief Returns the percentage value (not decimal).
     *
     * @return
     */
    float getPercentValue() override {
        return Tools::getDecimalValueFromNormalizedCoord(value) * 100;
    }

    /**
     * @brief Returns the demical percentage value out of 1.
     *
     * @return
     */
    float getNormalizedValue() override {
        updateNormalizedValue();
        return this->normalizedValue;
    }

    /**
     * @brief Gets the Pixel value (not decimal).
     *
     * @return
     */
    int getPixelValue() override {
        return value;
    }

    float getValue() {
        return value;
    }

    /**
     * @brief Updates the default (normalized) value so it can easily be rendered on the screen
     *        without any further math outside of this class. This should be called every single
     *        time the positioning is updated.
     */
    void updateNormalizedValue() override {
        this->normalizedValue = Tools::getNormalizedValueFromPixelCoord(axis, value);
        std::cout << "value: " << this->normalizedValue << "out of " << DisplayManager::Width() << std::endl;
    }

    void addPixel(float pixelVal) override {
        this->value += pixelVal;
        updateNormalizedValue();
    }

    void addPercent(float percentVal) override {
        this->value += Tools::getPixelValueFromDecimalCoord(axis, percentVal / 100.0f);
        updateNormalizedValue();
    }

    void addNormalized(float normalVal) override {
        this->value += Tools::getPixelValueFromNormalizedCoord(axis, normalVal);
        updateNormalizedValue();
    }
};

#endif //ENGINE_UIPIXELCONSTRAINT_H
