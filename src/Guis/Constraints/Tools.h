//
// Created by Joseph Alai on 3/12/22.
//

#ifndef ENGINE_TOOLS_H
#define ENGINE_TOOLS_H

#include "../../RenderEngine/DisplayManager.h"

class Tools {
public:
    /**
     * @brief Converts the input decimal percentage [0...1] into pixels.
     *
     * @param normalizedValue
     * @return
     */
    static int getPixelValueFromNormalizedCoord(Axis axis, float normalizedValue) {
        // multiplies the percentage normalizedValue by the width (pixel)
        if (axis == XAxis) {
            return static_cast<int>(static_cast<float>(DisplayManager::Width()) * getDecimalValueFromNormalizedCoord(normalizedValue));
        }
        return static_cast<int>(static_cast<float>(DisplayManager::Height()) * getDecimalValueFromNormalizedCoord(normalizedValue));
    }
    /**
     * @brief Converts the input decimal percentage [0...1] into pixels.
     *
     * @param normalizedValue
     * @return
     */
    static int getPixelValueFromDecimalCoord(Axis axis, float decimalValue) {
        // multiplies the percentage normalizedValue by the width (pixel)
        if (axis == XAxis) {
            return static_cast<int>(decimalValue * static_cast<float>(DisplayManager::Width()));
        }
        return static_cast<int>(decimalValue * static_cast<float>(DisplayManager::Height()));

    }

    /**
     * @brief PixelVal / MaxPixelVal = x / 2.0f ... [-1 ... 1]
     *
     * @return
     */
    static float getNormalizedValueFromPixelCoord(Axis axis, float pixelValue) {
        return pixelValue;
        float percent = getDecimalValueFromPixelCoord(axis, pixelValue);
        if (axis == XAxis) {
            return 2.0f * percent - 1.0f;
        }
        return 2.0f * (percent) - 1.0f;
    }

    static float getDecimalValueFromPixelCoord(Axis axis, float pixelValue) {
        if (axis == XAxis) {
            return pixelValue / static_cast<float>(DisplayManager::Width());
        }
        return pixelValue / static_cast<float>(DisplayManager::Height());
    }

    /**
     * @brief Decimal Value... x / 100 = (value + 1.0f) / (1.0f + 1.0f)
     * @param value
     * @return
     */
    static float getDecimalValueFromNormalizedCoord(float value) {
        return (value + 1.0f) / 2.0f;
    }

    /**
     * @brief Gets normalized value from whole number percentage
     *        percentage / 100 = x / 2.0f
     *
     * @param percentageValue
     * @return
     */
    static float getNormalizedValueFromPercentageCoord(float percentageValue) {
        return ((percentageValue / 100.0f) * 2.0f) - 1.0f;
    }

    /**
     * @brief Gets normalized value from whole number percentage
     *        percentage / 100 = x / 2.0f
     *
     * @param percentageValue
     * @return
     */
    static float getNormalizedValueFromDecimalCoord(float percentageValue) {
        return (percentageValue * 2.0f) - 1.0f;
    }
};

#endif //ENGINE_TOOLS_H
