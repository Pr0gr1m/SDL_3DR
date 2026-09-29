#ifndef SDL1_FRAMEPROFILER_H
#define SDL1_FRAMEPROFILER_H
#include <array>
#include <cstddef>
#include <string>
#include <vector>
#include <SDL3/SDL.h>

/**
 *@class FrameProfiler
 *@brief Collects per-stage frame timings and per-frame counters over a window of frames and logs a report
**/
class FrameProfiler {
public:
    enum class Stage {
        Frame,
        Build,
        Upload,
        Acquire,
        Record,
        Submit,
        Count
    };

    enum class Counter {
        VerticesDrawn,
        ChunksDrawn,
        ChunksRemeshed,
        BytesUploaded,
        Count
    };

    /**
     *@class StageScope
     *@brief Adds the time between its construction and destruction to a stage of the current frame
    **/
    class StageScope {
    public:
        StageScope(FrameProfiler &profiler, Stage stage);

        ~StageScope();

        StageScope(const StageScope &) = delete;

        StageScope &operator=(const StageScope &) = delete;

    private:
        FrameProfiler &profiler;
        Stage stage;
        Uint64 startTicks;
    };

    explicit FrameProfiler(size_t windowFrames);

    ///Closes the previous frame (Frame stage is the period between two calls) and logs a report when the window is full
    void BeginFrame();

    void AddCount(Counter counter, Uint64 amount);

    ///Starts a new window of the given length and discards recorded frames
    void StartWindow(size_t windowFrames, const char *modeLabel);

    ///Logs the recorded window, does nothing when no frame was recorded
    void Report() const;

private:
    static constexpr size_t kStageCount = static_cast<size_t>(Stage::Count);
    static constexpr size_t kCounterCount = static_cast<size_t>(Counter::Count);

    struct FrameSample {
        std::array<double, kStageCount> stageMs{};
        std::array<Uint64, kCounterCount> counters{};
    };

    void AddStageTicks(Stage stage, Uint64 ticks);

    std::vector<FrameSample> window;
    FrameSample currentFrame;
    Uint64 lastBeginTicks = 0;
    bool hasOpenFrame = false;
    size_t windowFrames;
    std::string modeLabel;
};

#endif //SDL1_FRAMEPROFILER_H
