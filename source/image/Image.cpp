#include "Image.h"

Image makeImageFromRawBuffer(
    std::size_t width,
    std::size_t height,
    std::size_t channels,
    std::string source_path,
    const unsigned char* raw_data
)
{
    // Calculate the byte count of the input data.
    const std::size_t byte_count = width * height * channels;

    // Instantiate an image container.
    Image image;
    image.width = width;
    image.height = height;
    image.channels = channels;
    image.source_path = std::move(source_path);
    image.pixels.assign(raw_data, raw_data + byte_count);

    return image;
}