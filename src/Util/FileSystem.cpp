//
// Created by Joseph Alai on 7/6/21.
//

#include "FileSystem.h"

#include <iostream>
#include <fstream>

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
    return HOME_PATH + "/src/Resources/Models/" + in + ".obj";
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

// ---------------------------------------------------------------------------
// VFS static member definitions
// ---------------------------------------------------------------------------

std::unordered_map<std::string, PakEntry> FileSystem::s_pakIndex_;
std::FILE*                                FileSystem::s_pakFile_ = nullptr;
std::mutex                                FileSystem::s_pakMutex_;

// ---------------------------------------------------------------------------
// VFS implementation
// ---------------------------------------------------------------------------

void FileSystem::initVFS() {
    std::string pakPath = HOME_PATH + "/data.pak";
    s_pakFile_ = std::fopen(pakPath.c_str(), "rb");
    if (!s_pakFile_) {
        std::cout << "[FileSystem] VFS: data.pak not found, using disk I/O\n";
        return;
    }

    // Read and validate magic number "PAK1"
    char magic[4];
    if (std::fread(magic, 1, 4, s_pakFile_) != 4 ||
        magic[0] != 'P' || magic[1] != 'A' || magic[2] != 'K' || magic[3] != '1') {
        std::cerr << "[FileSystem] VFS: invalid magic in data.pak, using disk I/O\n";
        std::fclose(s_pakFile_);
        s_pakFile_ = nullptr;
        return;
    }

    // Read file count
    uint32_t count = 0;
    if (std::fread(&count, 4, 1, s_pakFile_) != 1) {
        std::cerr << "[FileSystem] VFS: failed to read file count, using disk I/O\n";
        std::fclose(s_pakFile_);
        s_pakFile_ = nullptr;
        return;
    }

    // Read file table
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t pathLen = 0;
        if (std::fread(&pathLen, 4, 1, s_pakFile_) != 1) break;

        std::string path(pathLen, '\0');
        if (std::fread(&path[0], 1, pathLen, s_pakFile_) != pathLen) break;

        PakEntry entry{};
        if (std::fread(&entry.offset, 8, 1, s_pakFile_) != 1) break;
        if (std::fread(&entry.size,   8, 1, s_pakFile_) != 1) break;

        s_pakIndex_[path] = entry;
    }

    std::cout << "[FileSystem] VFS: loaded data.pak with " << count << " files\n";
}

void FileSystem::shutdownVFS() {
    std::lock_guard<std::mutex> lock(s_pakMutex_);
    if (s_pakFile_) {
        std::fclose(s_pakFile_);
        s_pakFile_ = nullptr;
    }
    s_pakIndex_.clear();
}

std::vector<uint8_t> FileSystem::readAllBytes(const std::string& path) {
    // If VFS is active, strip HOME_PATH prefix and look up in the pak index.
    // The entire VFS lookup + read is guarded by s_pakMutex_ to prevent a race
    // between readAllBytes (worker threads) and shutdownVFS (main thread).
    {
        std::lock_guard<std::mutex> lock(s_pakMutex_);
        if (s_pakFile_) {
            std::string vPath = path;
            if (vPath.size() > HOME_PATH.size() &&
                vPath.compare(0, HOME_PATH.size(), HOME_PATH) == 0) {
                vPath = vPath.substr(HOME_PATH.size());
                if (!vPath.empty() && vPath[0] == '/') {
                    vPath = vPath.substr(1);
                }
            }

            auto it = s_pakIndex_.find(vPath);
            if (it != s_pakIndex_.end()) {
                const uint64_t offset = it->second.offset;
                const uint64_t size   = it->second.size;
                std::vector<uint8_t> data(size);
#ifdef _WIN32
                _fseeki64(s_pakFile_, static_cast<__int64>(offset), SEEK_SET);
#else
                fseeko(s_pakFile_, static_cast<off_t>(offset), SEEK_SET);
#endif
                auto nread = std::fread(data.data(), 1, size, s_pakFile_);
                data.resize(nread);
                return data;
            }
        }
    }

    // Fall back to reading directly from disk
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return {};
    }
    return std::vector<uint8_t>(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    );
}

bool FileSystem::isVFSActive() {
    return s_pakFile_ != nullptr;
}
