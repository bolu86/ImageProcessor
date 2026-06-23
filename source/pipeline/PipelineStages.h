#pragma once
#include <filesystem>
#include <vector>

// Local includes
#include "../image/Image.h"
#include "PipelineStage.h"
#include "PipelineQueue.h"

// ----------------------------------------------------------------------------
// LoadStage
// ----------------------------------------------------------------------------
class LoadStage : public PipelineStage
{
public:
    LoadStage(
        std::vector<std::filesystem::path> input_paths,
        PipelineQueue<Image>& output
    );
    void runImpl() override;
    void shutdown() override { output_.shutdown(); };

private:
    std::vector<std::filesystem::path> input_paths_;
    PipelineQueue<Image>& output_;
};

// ----------------------------------------------------------------------------
// BlurStage
// ----------------------------------------------------------------------------
class BlurStage : public PipelineStage
{
public:
    BlurStage(
        PipelineQueue<Image>& input,
        PipelineQueue<Image>& output
    );
    void runImpl() override;
    void shutdown() override { output_.shutdown(); };

private:
    PipelineQueue<Image>& input_;
    PipelineQueue<Image>& output_;
 };

// ----------------------------------------------------------------------------
// HistogramEqualizationStage
// ----------------------------------------------------------------------------
class HistogramEqualizationStage : public PipelineStage
{
public:
    HistogramEqualizationStage(
        PipelineQueue<Image>& input,
        PipelineQueue<Image>& output
    );
    void runImpl() override;
    void shutdown() override { output_.shutdown(); };

private:
    PipelineQueue<Image>& input_;
    PipelineQueue<Image>& output_;
};

// ----------------------------------------------------------------------------
// SaveStage
// ----------------------------------------------------------------------------
class SaveStage : public PipelineStage
{
public:
    SaveStage(
        PipelineQueue<Image>& input,
        std::filesystem::path output_directory_path
    );
    void runImpl() override;

private:
    PipelineQueue<Image>& input_;
    std::filesystem::path output_directory_path_;
};