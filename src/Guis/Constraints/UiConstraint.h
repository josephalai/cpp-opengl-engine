//
// Created by Joseph Alai on 3/12/22.
//

#ifndef ENGINE_UICONSTRAINT_H
#define ENGINE_UICONSTRAINT_H

enum Axis {
    XAxis,
    YAxis
};

enum ConstraintType {
    Absolute,
    Relative
};

enum PositionType {
    Percent,
    Pixel,
    Normalized
};


class UiConstraint {
protected:
    float value = 0.0f;
    float normalizedValue = 0.0f;
    Axis axis;
public:
    virtual ConstraintType getConstraintType() const = 0;

    virtual PositionType getPositionType() const = 0;

    virtual float getPercentValue() = 0;

    virtual float getNormalizedValue() = 0;

    virtual void updateNormalizedValue() = 0;

    virtual void addPixel(float pixelVal) = 0;

    virtual void addPercent(float percentVal) = 0;

    virtual void addNormalized(float normalizedVal) = 0;

    virtual int getPixelValue() = 0;

    Axis getAxis() const;
};


#endif //ENGINE_UICONSTRAINT_H
