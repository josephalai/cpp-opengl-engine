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

std::string FileSystem::Model(std::string in) {
    return HOME_PATH + "/src/Resources/Tutorial/" + in + ".obj";
}

std::string FileSystem::Texture(std::string in) {
    return HOME_PATH + "/src/Resources/Tutorial/" + in + ".png";
}

std::string FileSystem::TerrainTexture(std::string in) {
    return HOME_PATH + "/src/Resources/Tutorial/MultiTextureTerrain/" + in + ".png";
}