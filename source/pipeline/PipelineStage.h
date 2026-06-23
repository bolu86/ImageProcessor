#pragma once
#include <filesystem>
#include <string>
#include <vector>

// ----------------------------------------------------------------------------
// ProcessingError
// ----------------------------------------------------------------------------
struct ProcessingError
{
    ProcessingError(
        std::string stage, 
        std::string file_path, 
        std::string reason
    ) : stage(std::move(stage))
      , file_path(std::move(file_path))
      , reason(std::move(reason))
    {}

    std::string stage;
    std::string file_path;
    std::string reason;
};

// ----------------------------------------------------------------------------
// PipelineStage
// ----------------------------------------------------------------------------
// Abstract base class for derived pipeline-stage classes.
class PipelineStage
{
public:
    explicit PipelineStage(std::string stage_name)
        : stage_name_(std::move(stage_name))
    {}
    virtual ~PipelineStage() = default;

    // Call to run the stage's tasks.
    // Non-virtual: provides guaranteed try/catch and shutdown() wrapping
    // for all derived stages. Override runImpl() in derived classes, not this.
    void run() {
        try {
            runImpl();
        }
        catch (...) {
            errors_.emplace_back(
                getStageName(),
                "",
                "Unexpected exception, stage terminated early"
            );
        }

        // Signal shutdown, normally after all images have been processed 
        // but also if runImpl terminates in any way.
        shutdown();
    };

    // Extract errors, moving them out of the stage.
    // Called by Pipeline after join(), safe to move since thread is done.
    std::vector<ProcessingError> extractErrors() { return std::move(errors_); }

    std::size_t succeededCount() const { return succeeded_count_; }
    const std::string& getStageName() const { return stage_name_; }

    // This class does not allow copy or move.
    PipelineStage(const PipelineStage&) = delete;
    PipelineStage& operator=(const PipelineStage&) = delete;
    PipelineStage(PipelineStage&&) = delete;
    PipelineStage& operator=(PipelineStage&&) = delete;

protected:
    // Override in derived classes to implement the specific tasks to run.
    virtual void runImpl() = 0;

    // Called after runImpl() completes or throws.
    // Override in derived classes that have an output queue to shut down.
    virtual void shutdown() {};

    // The derived "stage" classes write to these directly during run().
    std::vector<ProcessingError> errors_;
    std::size_t succeeded_count_{ 0 };

private:
    std::string stage_name_;
};