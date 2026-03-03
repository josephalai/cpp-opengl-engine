//
// Created by Joseph Alai on 7/4/21.
//

#ifndef ENGINE_UTILS_H
#define ENGINE_UTILS_H
class Utils {
public:
    static float randomFloat() {
        return static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
    }

    static int roll(int min, int max) {
        // x is in [0,1[
        double x = rand() / (static_cast<double>(RAND_MAX) + 1.0);

        // [0,1[ * (max - min) + min is in [min,max[
        int that = min + static_cast<int>( x * (max - min));

        return that;
    }

    static std::string shaderArray(std::string uniformName, int index, std::string attribute = "") {
        std::string shaderArrayName = uniformName + "[" + std::to_string(index) + "]";
        if (attribute != "") {
            shaderArrayName = shaderArrayName + "." + attribute;
        }
        return shaderArrayName;
    }
};

#endif //ENGINE_UTILS_H
