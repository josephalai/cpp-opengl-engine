//
// Created by Joseph Alai on 7/6/21.
//

#include "FileSystem.h"

std::string HOME_PATH =
#ifdef RESOURCE_ROOT
    RESOURCE_ROOT
#else
    // Fallback: resolve the current working directory at runtime.
    // POSIX only (Linux / macOS); on Windows _fullpath is used instead.
    []() -> std::string {
#ifdef _WIN32
        char buf[4096];
        char* raw = _fullpath(buf, ".", 4096);
        return raw ? raw : ".";
#else
        char* raw = realpath(".", nullptr);
        std::string path = raw ? raw : ".";
        free(raw);
        return path;
#endif
    }()
#endif
    ;

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

std::string FileSystem::BakedChunk(std::string in) {
    return HOME_PATH + "/src/Resources/baked_chunks/" + in;
}
