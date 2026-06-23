#pragma once
#include "Image.h"
#include <optional>
#include <variant>
#include <string>

// TODO: Improve error handling in save, see if stbi offers any more info
//       when saving fails.

// Class to handle loading and saving of images.
namespace ImageIO {

    using ErrorMsg = std::string;

    // Loads an image from disk. Returns std::nullopt if the file could not
    // be read or decoded (e.g. missing file, unsupported/corrupt format).
    // Ideally, this should use std::expected(C++23), but since this 
    // project is meant for C++17, std::variant is used instead.
    std::variant<Image, ErrorMsg> load(
        const std::string& path
    );

    // Saves an image to disk as PNG. Returns nullopt on success, 
    // or an error on fail.
    std::optional<ErrorMsg> savePng(
        const Image& image, 
        const std::string& path
    );

}