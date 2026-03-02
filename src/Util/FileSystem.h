//
// Created by Joseph Alai on 7/1/21.
//

#ifndef ENGINE_FILESYSTEM_H
#define ENGINE_FILESYSTEM_H
#define PATH(IN)(HOME_PATH+IN)

#include <cstdlib>
#include <string>

extern std::string HOME_PATH;
class FileSystem {
public:
    static std::string Path(std::string in);
    static std::string Path(char *in);
    static std::string Font(std::string in);
    static std::string Model(std::string in);
    static std::string Texture(std::string in);
    static std::string TerrainTexture(std::string in);
    static std::string Skybox(std::string in);
    static std::string Scene(std::string in);

};
#endif //ENGINE_FILESYSTEM_H
