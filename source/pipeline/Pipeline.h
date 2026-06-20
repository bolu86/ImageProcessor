#pragma once

// ----------------------------------------------------------------------------
// Pipeline
// ----------------------------------------------------------------------------
class Pipeline {
public:
    Pipeline(std::vector<std::string> inputPaths,
        std::string outputDirectory,
        std::size_t queueCapacity);

    void run();   // blocks until all images have flowed through and been saved

private:
    PipelineQueue<Image> loadToBlur_;
    PipelineQueue<Image> blurToHistogram_;
    PipelineQueue<Image> histogramToSave_;

    LoadStage loadStage_;
    BlurStage blurStage_;
    HistogramEqualizationStage histogramStage_;
    SaveStage saveStage_;
};