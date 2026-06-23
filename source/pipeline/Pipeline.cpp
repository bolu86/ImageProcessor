#include "Pipeline.h"

Pipeline::Pipeline(
    std::vector<std::filesystem::path> input_paths,
    std::filesystem::path output_path,
    std::size_t queue_capacity
) : load_to_blur_(queue_capacity)
  , blur_to_histogram_(queue_capacity)
  , histogram_to_save_(queue_capacity)
{ 
    // Input validation is delegated to the individual stages (LoadStage,
    // SaveStage), which throw std::invalid_argument on invalid inputs.
    // This avoids duplicating validation logic at the Pipeline level.

    // Create the work stages.
    stages_.emplace_back(
        std::make_unique<LoadStage>(
            std::move(input_paths),
            load_to_blur_
        )
    );
    stages_.emplace_back(
        std::make_unique<BlurStage>(
            load_to_blur_,
            blur_to_histogram_
        )
    );
    stages_.emplace_back(
        std::make_unique<HistogramEqualizationStage>(
            blur_to_histogram_,
            histogram_to_save_
        )
    );
    stages_.emplace_back(
        std::make_unique<SaveStage>(
            histogram_to_save_,
            std::move(output_path)
        )
    );
}

void Pipeline::run()
{
    if (!threads_.empty())
        throw std::logic_error("Pipeline::run() can only be called once");

    // Add the stages to worker threads.
    for (auto& stage : stages_)
        threads_.emplace_back(&PipelineStage::run, stage.get());

    // Wait for all threads to finish and join.
    for (auto& t : threads_)
        t.join();

    // All threads finished, safe to read each stage's local results.
    collectSummary();
}

void Pipeline::collectSummary() {
    // All threads finished, safe to read each stage's local results
    for (auto& stage : stages_) {
        total_succeeded_count_ += stage->succeededCount();
        auto stage_errors = stage->extractErrors();
        all_errors_.insert(
            all_errors_.end(),
            std::make_move_iterator(stage_errors.begin()),
            std::make_move_iterator(stage_errors.end())
        );
    }
}