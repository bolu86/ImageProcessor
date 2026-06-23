#include "PipelineStages.h"
#include "../image/ImageIO.h"

// LoadStage::LoadStage
// ----------------------------------------------------------------------------
LoadStage::LoadStage(
    std::vector<std::filesystem::path> input_paths,
    PipelineQueue<Image>& output
) : PipelineStage("LoadStage")
  , input_paths_(std::move(input_paths))
  , output_(output)
{
    // Input checks
    if (input_paths_.empty()) {
        throw std::invalid_argument(
            "Pipeline: input_paths must contain at least one path"
        );
    }
}

// LoadStage::run
// ----------------------------------------------------------------------------
void LoadStage::runImpl()
{
    for (const auto& path : input_paths_) {
        // Load an image.
        auto new_image = ImageIO::load(path);

        // Log any errors. The pipeline continues with the next 
        // image rather than propagating the failure downstream.
        if (std::holds_alternative<ImageIO::ErrorMsg>(new_image)) {
            errors_.emplace_back(
                getStageName(),
                path.string(),
                std::get<ImageIO::ErrorMsg>(new_image)
            );
            continue;
        }

        // Extract the image and put it into the output queue.
        // If the output queue was shut down for some reason, 
        // there is no point continuing so just break.
        if (!output_.push(std::move(std::get<Image>(new_image))))
            break;

        // Track success.
        ++succeeded_count_;
    }
}

// BlurStage::BlurStage
// ----------------------------------------------------------------------------
BlurStage::BlurStage(
    PipelineQueue<Image>& input,
    PipelineQueue<Image>& output
) : PipelineStage("BlurStage")
  , input_(input)
  , output_(output)
{}

// BlurStage::run
// ----------------------------------------------------------------------------
void BlurStage::runImpl()
{        
    while(true) {
        // Get a new image.
        auto new_image = input_.pop();
        if (!new_image.has_value()) 
            break;

        try {
            // Perform blur.
            Image processed_image = /*Blur function call with*/new_image.value();
            if (!output_.push(std::move(processed_image)))
                break;

            // Track success.
            ++succeeded_count_;
        }
        catch (const std::exception& e) {
            // Log any errors. The pipeline continues with the next 
            // image rather than propagating the failure downstream.
            errors_.emplace_back(
                getStageName(),
                new_image->source_path.string(),
                e.what()
            );
        }
    }
}

// HistogramEqualizationStage::HistogramEqualizationStage
// ----------------------------------------------------------------------------
HistogramEqualizationStage::HistogramEqualizationStage(
    PipelineQueue<Image>& input,
    PipelineQueue<Image>& output
) : PipelineStage("HistogramEqualizationStage")
  , input_(input)
  , output_(output)
{}

// HistogramEqualizationStage::run
// ----------------------------------------------------------------------------
void HistogramEqualizationStage::runImpl()
{
    while (true) {
        // Get a new image.
        auto new_image = input_.pop();
        if (!new_image.has_value())
            break;

        try {
            // Perform histogram equalization.
            Image processed_image = /*histogram equalization function call with*/new_image.value();
            if (!output_.push(std::move(processed_image)))
                break;

            // Track success.
            ++succeeded_count_;
        }
        catch (const std::exception& e) {
            // Log any errors. The pipeline continues with the next 
            // image rather than propagating the failure downstream.
            errors_.emplace_back(
                getStageName(),
                new_image->source_path.string(),
                e.what()
            );
        }
    }
}

// SaveStage::SaveStage
// Constructor
// ----------------------------------------------------------------------------
SaveStage::SaveStage(
    PipelineQueue<Image>& input,
    std::filesystem::path output_directory_path
) : PipelineStage("SaveStage")
  , input_(input)
  , output_directory_path_(std::move(output_directory_path))
{
    // Input checks
    if (std::error_code ec; !std::filesystem::exists(output_directory_path_, ec)) {
        throw std::invalid_argument(
            "Pipeline: output_path does not exist"
        );
    }
}

// SaveStage::run
// ----------------------------------------------------------------------------
void SaveStage::runImpl()
{
    while (true) {
        // Get a new image.
        auto new_image = input_.pop();
        if (!new_image.has_value())
            break;

        // Save the image.
        auto output_file = output_directory_path_ 
            / new_image->source_path.filename();
        auto result = ImageIO::savePng(
            new_image.value(),
            output_file
        );

        // If saving failed, the result is an error message 
        // (it is nullopt on success).
        if (result.has_value()) {
            // Log any errors.
            errors_.emplace_back(
                    getStageName(),
                    new_image->source_path.string(),
                    result.value()
            );
        }
        else {
            // Track success.
            ++succeeded_count_;
        }
    }
}