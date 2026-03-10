//
// Created by Joseph Alai on 7/6/21.
//

#include "TextureLoader.h"
#include "../Util/FileSystem.h"

#define STB_IMAGE_IMPLEMENTATION

#include "../Libraries/images/stb_image.h"
#include "png.h"

/**
 * @brief loads a image file as a texture to opengl, and stores the
 *        textureId.
 * @param filename
 * @param type
 */
TextureLoader::TextureLoader(std::string filename, ImageType type) {
    this->textureId = 0;
    switch (type) {
        case JPG:
            this->loadJpgTexture(filename.c_str());
            break;
        case PNG:
            this->loadPngTexture(filename.c_str());
            break;
        default:
            printf("there is a problem, with loading the texture");
            return;
    }
}

/**
 * @brief opens and stores a texture to the buffer.
 * @param file_name
 */
void TextureLoader::loadJpgTexture(const char *file_name) {
    glGenTextures(1, &this->textureId);
    glBindTexture(GL_TEXTURE_2D, this->textureId);
    // set the texture wrapping/filtering options (on the currently bound texture object)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // load and generate the texture
    int width, height, nrChannels;
    auto bytes = FileSystem::readAllBytes(std::string(file_name));
    unsigned char *data = nullptr;
    if (!bytes.empty()) {
        data = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()),
                                     &width, &height, &nrChannels, 0);
    }
    if (data) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    } else {
        printf("Failed to load texture\n");
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);
}

/**
 * @brief load a texture as a png and apply it to openGL
 *
 * @param file_name
 */
void TextureLoader::loadPngTexture(const char *file_name) {
    int w;
    int h;
    int comp;
    auto bytes = FileSystem::readAllBytes(std::string(file_name));
    unsigned char *image = nullptr;
    if (!bytes.empty()) {
        image = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()),
                                      &w, &h, &comp, STBI_rgb_alpha);
    }

    if (image == nullptr) {
        printf("Failed to load texture %s\n", file_name);
        return;
    }

    glGenTextures(1, &this->textureId);

    glBindTexture(GL_TEXTURE_2D, this->textureId);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, -1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    if (comp == 3) glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
    else if (comp == 4) glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);

    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(image);
}