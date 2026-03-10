//
// Created by Joseph Alai on 7/12/21.
//

#include "HeightMap.h"

const float Heightmap::MAX_HEIGHT = 40.0f;
const float Heightmap::MIN_HEIGHT = -40.0f;
const float Heightmap::MAX_COLOR_VALUE = 256 * 256 * 256;

Heightmap::Heightmap(const std::string &fileName) {
    this->fileName = fileName;
    imageInfo = ImageInfo{.filename = fileName};
    if (fileName.empty()) {
        return; 
    }
    calculateMapFromImage();
    if (heightData.empty()) {
        return;
    }
}

void Heightmap::calculateMapFromImage() {
    stbi_set_flip_vertically_on_load(1);
    int bytesPerPixel;
    const auto imageData = stbi_load(fileName.c_str(), &imageInfo.width, &imageInfo.height, &bytesPerPixel, 0);
    if (imageData == nullptr) {

        // Return empty vector in case of failure
        std::cout << "Failed to load heightmap image " << fileName << "!" << std::endl;
        return;
    }

    std::vector<std::vector<float>> result(imageInfo.height, std::vector<float>(imageInfo.width));
    auto pixelPtr = &imageData[0];
    for (auto i = 0; i < imageInfo.height; i++) {
        for (auto j = 0; j < imageInfo.width; j++) {
            result[i][j] = static_cast<float>(*pixelPtr) / 255.0f;
            pixelPtr += bytesPerPixel;
        }
    }
    heightData = result;
    stbi_image_free(imageData);
    // Reset global flip flag so subsequent texture loads (entity textures etc.)
    // are not unintentionally flipped; those rely on the UV-flip in the OBJ loader.
    stbi_set_flip_vertically_on_load(0);
}

float Heightmap::getHeight(int x, int z) {
    if (x < 0 || z < 0 || x >= imageInfo.height || z >= imageInfo.height) {
        return 0;
    }
    return (heightData[x][z] - 0.5f) * 2 * MAX_HEIGHT;
}

ImageInfo Heightmap::getImageInfo() {
    return imageInfo;
}

std::vector<std::vector<float>> Heightmap::getHeightData() {
    return heightData;
}


