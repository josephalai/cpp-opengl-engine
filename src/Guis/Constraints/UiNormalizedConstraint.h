//
// Created by Joseph Alai on 3/13/22.
//

#ifndef ENGINE_UINORMALIZEDCONSTRAINT_H
#define ENGINE_UINORMALIZEDCONSTRAINT_H
#include "UiConstraint.h"
#include "Tools.h"

class UiNormalizedConstraint : public UiConstraint {
private:
    const ConstraintType constraintType = Relative;
    const PositionType positionType = Normalized;

public:
    UiNormalizedConstraint(Axis axis, float value, ConstraintType constraintType = Relative)
            : constraintType(constraintType) {
        this->axis = axis;
        this->value = value;
        this->normalizedValue = value;
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
        return Tools::getDecimalValueFromNormalizedCoord(value) * 100.0f;
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
        return Tools::getPixelValueFromNormalizedCoord(axis, value);
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
        this->normalizedValue = value;
    }

    void addPixel(float pixelVal) override {
        this->value += Tools::getNormalizedValueFromPixelCoord(axis, pixelVal);
    }

    void addPercent(float percentVal) override {
        this->value += Tools::getNormalizedValueFromPercentageCoord(percentVal);
    }

    void addNormalized(float normalizedVal) override {
        this->value += normalizedVal;
    }
};
#endif //ENGINE_UINORMALIZEDCONSTRAINT_H
