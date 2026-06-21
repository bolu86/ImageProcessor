#pragma once
#include "Image.h"
#include <variant>
#include <string>

// Container to track load errors from stb_image.
struct LoadError {
    // The path of the file that failed to load.
    std::string path;

    // The stbi_failure_reason().
    std::string reason;
};

// Class to handle loading and saving of images.
namespace ImageIO {

    // Loads an image from disk. Returns std::nullopt if the file could not
    // be read or decoded (e.g. missing file, unsupported/corrupt format).
    // Ideally, this should use std::expected(C++23), but since this 
    // project is meant for C++17, std::variant is used instead.
    std::variant<Image, LoadError> load(const std::string& path);

    // Saves an image to disk as PNG. Returns true on success.
    bool savePng(const Image& image, const std::string& path);

}