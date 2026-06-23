#pragma once
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// Local includes
#include "../image/Image.h"
#include "PipelineQueue.h"
#include "PipelineStages.h"


// ----------------------------------------------------------------------------
// Pipeline
// ----------------------------------------------------------------------------
class Pipeline {
public:
    // Constructor
    Pipeline(
        std::vector<std::filesystem::path> input_paths,
        std::filesystem::path output_path,
        std::size_t queue_capacity
    );

    // Run all stages.
    void run();

    // Getters
    std::size_t totalSucceededCount() const { return total_succeeded_count_; }
    std::size_t totalFailedCount() const { return all_errors_.size(); }
    const std::vector<ProcessingError>& errors() const { return all_errors_; }

private:
    // Collect result metrics.
    void collectSummary();

    // Processing queues
    PipelineQueue<Image> load_to_blur_;
    PipelineQueue<Image> blur_to_histogram_;
    PipelineQueue<Image> histogram_to_save_;

    // Stages
    std::vector<std::unique_ptr<PipelineStage>> stages_;

    // Worker threads
    std::vector<std::thread> threads_;

    // Result metrics
    std::size_t total_succeeded_count_{}; // kind of useless?
    std::vector<ProcessingError> all_errors_;
};