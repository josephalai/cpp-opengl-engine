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
};
#endif //ENGINE_FILESYSTEM_H
