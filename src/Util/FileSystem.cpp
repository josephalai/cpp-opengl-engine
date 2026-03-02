//
// Created by Joseph Alai on 7/6/21.
//

#include "FileSystem.h"

std::string HOME_PATH = realpath(".", NULL);

std::string FileSystem::Path(std::string in) {
    return HOME_PATH + in;
}

std::string FileSystem::Path(char *in) {
    return HOME_PATH + in;
}

std::string FileSystem::Font(std::string in) {
    return HOME_PATH + "/src/Resources/Tutorial/Fonts/" + in + ".ttf";
}

std::string FileSystem::Model(std::string in) {
    return HOME_PATH + "/src/Resources/Tutorial/" + in + ".obj";
}

std::string FileSystem::Texture(std::string in) {
    return HOME_PATH + "/src/Resources/Tutorial/" + in + ".png";
}

std::string FileSystem::TerrainTexture(std::string in) {
    return HOME_PATH + "/src/Resources/Tutorial/MultiTextureTerrain/" + in + ".png";
}

std::string FileSystem::Skybox(std::string in) {
    return HOME_PATH + "/src/Resources/Tutorial/skybox/" + in + ".png";
}

std::string FileSystem::Scene(std::string in) {
    return HOME_PATH + "/src/Resources/" + in;
}
