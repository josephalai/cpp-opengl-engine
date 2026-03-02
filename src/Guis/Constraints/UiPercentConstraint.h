//
// Created by Joseph Alai on 3/12/22.
//

#ifndef ENGINE_UIPERCENTCONSTRAINT_H
#define ENGINE_UIPERCENTCONSTRAINT_H

#include "UiConstraint.h"
#include "Tools.h"

class UiPercentConstraint : public UiConstraint {
private:
    const ConstraintType constraintType = Relative;
    const PositionType positionType = Percent;

public:
    UiPercentConstraint(Axis axis, float value, ConstraintType constraintType = Relative)
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
        return value;
    }

    /**
     * @brief Returns the demical percentage value out of 1.
     *
     * @return
     */
    float getNormalizedValue() override {
        this->updateNormalizedValue();
        return normalizedValue;
    }

    /**
     * @brief Gets the Pixel value (not decimal).
     *
     * @return
     */
    int getPixelValue() override {
        return Tools::getPixelValueFromNormalizedCoord(axis, this->normalizedValue);
    }

    float getValue() {
        return value;
    }

    void updateNormalizedValue() override {
        this->normalizedValue = Tools::getNormalizedValueFromPercentageCoord(value);
        std::cout << "normalized value : " << normalizedValue << std::endl;
    }

    void addPixel(float pixel) override {
        this->value += (Tools::getDecimalValueFromPixelCoord(axis, pixel) * 100.0f);
        updateNormalizedValue();
    }

    void addPercent(float percent) override {
        this->value += percent;
        updateNormalizedValue();
    }

    void addNormalized(float pixel) override {
        this->value += (Tools::getDecimalValueFromNormalizedCoord(pixel) * 100.0f);
        updateNormalizedValue();
    }
};

#endif //ENGINE_UIPERCENTCONSTRAINT_H
