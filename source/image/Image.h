#pragma once
#include <vector>
#include <string>

// ----------------------------------------------------------------------------
// Image
// ----------------------------------------------------------------------------
// Container for representing a loaded image in memory.
struct Image {
    // Image properties
    std::size_t width{};
    std::size_t height{};
    std::size_t channels{};

    // File path
    std::string source_path;

    // Image pixel data
    std::vector<unsigned char> pixels;
};

// ----------------------------------------------------------------------------
// makeImageFromRawBuffer
// ----------------------------------------------------------------------------
// Create an image container for a successfully loaded image.
Image makeImageFromRawBuffer(
    std::size_t width,
    std::size_t height,
    std::size_t channels,
    std::string source_path,
    const unsigned char* raw_data
);