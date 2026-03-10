//
// Created by Joseph Alai on 7/1/21.
//

#ifndef ENGINE_FILESYSTEM_H
#define ENGINE_FILESYSTEM_H
#define PATH(IN)(HOME_PATH+IN)

#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

extern std::string HOME_PATH;

struct PakEntry {
    uint64_t offset;
    uint64_t size;
};

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
    static std::string BakedChunk(std::string in);

    // VFS API (Step 5.2)
    static void initVFS();
    static void shutdownVFS();
    static std::vector<uint8_t> readAllBytes(const std::string& path);
    static bool isVFSActive();

private:
    static std::unordered_map<std::string, PakEntry> s_pakIndex_;
    static std::FILE*                                 s_pakFile_;
    static std::mutex                                 s_pakMutex_;
};
#endif //ENGINE_FILESYSTEM_H
