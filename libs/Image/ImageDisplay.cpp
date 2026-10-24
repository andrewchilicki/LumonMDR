#include "ImageDisplay.h"

#include "Image.h"

#include <GL/glew.h>
#include <unordered_map>
#include <iostream>
#include <optional>
#include <utility>

#define _CRT_SECURE_NO_WARNINGS
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

class ImageDisplayImpl : public ImageDisplay {
public:
    explicit ImageDisplayImpl(std::string assetDir) : assetDir(std::move(assetDir)) {}

    ~ImageDisplayImpl() override {
        // Clean up cached textures
        for (const auto& pair : imageCache) {
            glDeleteTextures(1, &pair.second.texture);
        }
    }

    void drawImGuiImage(const std::string& imagePath, float scale, std::optional<ImVec4> tint) final
    {
        auto filePath = assetDir + imagePath;
        if (auto image = getImageForFile(filePath)) {
            ImGui::Image((ImTextureID)(intptr_t)image->texture, ImVec2(image->width*scale, image->height*scale),
                        ImVec2(0,0), ImVec2(1,1), tint.value_or(ImVec4(1,1,1,1)));
        }
    }

    std::pair<int, int> getImageSize(const std::string &imagePath) final
    {
        auto filePath = assetDir + imagePath;
        if (auto image = getImageForFile(filePath)) {
            return std::make_pair(image->width, image->height);
        }
        return std::make_pair(0, 0);
    }

    std::optional<Image> getImageForFile(const std::string& filePath)
    {
        if (auto it = imageCache.find(filePath); it != imageCache.end()) {
            // Return from cache
            return it->second;
        }

        // Load as new into cache
        return loadImageFromFile(filePath);
    }

    std::optional<Image> loadImageFromFile(const std::string &filePath)
    {
        // Load texture from file
        FILE* f = fopen(filePath.c_str(), "rb");
        if (f == NULL) {
            std::cerr << "Failed to open file " << filePath << std::endl;
            return std::nullopt;
        }
        fseek(f, 0, SEEK_END);
        size_t file_size = (size_t)ftell(f);
        if (file_size == -1){
            std::cerr << "Failed to get file size of file " << filePath << std::endl;
            return std::nullopt;
        }
        fseek(f, 0, SEEK_SET);
        void* file_data = IM_ALLOC(file_size);
        fread(file_data, 1, file_size, f);

        int out_width = 0;
        int out_height = 0;
        GLuint out_texture = 0;
        bool ret = loadTextureFromMemory(file_data, file_size, &out_texture, &out_width, &out_height);
        IM_FREE(file_data);

        if (ret) {
            // Cache the loaded image
            auto newImage = Image{out_texture, out_width, out_height};
            imageCache.emplace(filePath, newImage);
            std::cout << "New image saved to cache: " << filePath << std::endl;
            return newImage;
        }

        return std::nullopt;
    }

    // From https://github.com/ocornut/imgui/wiki/Image-Loading-and-Displaying-Examples
    bool loadTextureFromMemory(const void* data, size_t data_size, GLuint* out_texture, int* out_width, int* out_height)
    {
        int image_width = 0;
        int image_height = 0;
        unsigned char* image_data = stbi_load_from_memory((const unsigned char*)data, (int)data_size, &image_width, &image_height, NULL, 4);
        if (image_data == NULL) {
            return false;
        }

        GLuint image_texture;
        glGenTextures(1, &image_texture);
        glBindTexture(GL_TEXTURE_2D, image_texture);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
        stbi_image_free(image_data);

        *out_texture = image_texture;
        *out_width = image_width;
        *out_height = image_height;

        return true;
    }

    std::string assetDir;
    std::unordered_map<std::string, Image> imageCache;
};

std::shared_ptr<ImageDisplay> createImageDisplay(const std::string& assetDir)
{
    return std::make_shared<ImageDisplayImpl>(assetDir);
}
