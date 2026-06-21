// Headers required to compile the implementation of stb_image
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
// Silence deprecation warnings from stb_image
#define _CRT_SECURE_NO_WARNINGS

#include "../third_party/stb_image.h"
#include "../third_party/stb_image_write.h"

#include "ImageIO.h"

namespace ImageIO {

    // load
    // ----------------------------------------------------------------------------
    std::variant<Image, LoadError> load(const std::string& path)
    {
        int width{}, height{};
        [[maybe_unused]] int channels_in_file{};

        // Force 3 channels (RGB), discarding any alpha channel, so every
        // loaded image has a consistent, known channel count downstream.
        int desired_channels{3};

        // Read raw image data into memory.
        unsigned char* raw_data = stbi_load(
            path.c_str(), 
            &width, 
            &height, 
            &channels_in_file,
            desired_channels
        );

        // Handle load errors.
        if (raw_data == nullptr) {
            return LoadError{
                path,
                stbi_failure_reason()
            };
        }

        Image image = makeImageFromRawBuffer(
            static_cast<std::size_t>(width),
            static_cast<std::size_t>(height),
            static_cast<std::size_t>(desired_channels),
            path,
            raw_data
        );

        // Raw buffer's lifetime ends here.
        stbi_image_free(raw_data);

        return image;
    }

    // savePng
    // ----------------------------------------------------------------------------
    bool savePng(const Image& image, const std::string& path)
    {
        const int stride_in_bytes = image.width * image.channels;

        int result = stbi_write_png(
            path.c_str(),
            image.width,
            image.height,
            image.channels,
            image.pixels.data(),
            stride_in_bytes
        );

        return result != 0;
    }

}