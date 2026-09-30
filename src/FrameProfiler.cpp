#include "FrameProfiler.h"

#include <algorithm>
#include <cmath>

namespace {
    constexpr std::array<const char *, 6> kStageNames = {"Frame", "Build", "Upload", "Acquire", "Record", "Submit"};
    constexpr std::array<const char *, 4> kCounterNames = {"Vertices drawn", "Chunks drawn", "Chunks remeshed", "Bytes uploaded"};

    double TicksToMilliseconds(const Uint64 ticks) {
        return static_cast<double>(ticks) * 1000.0 / static_cast<double>(SDL_GetPerformanceFrequency());
    }
}

FrameProfiler::StageScope::StageScope(FrameProfiler &profiler, const Stage stage)
    : profiler(profiler), stage(stage), startTicks(SDL_GetPerformanceCounter()) {
}

FrameProfiler::StageScope::~StageScope() {
    profiler.AddStageTicks(stage, SDL_GetPerformanceCounter() - startTicks);
}

FrameProfiler::FrameProfiler(const size_t windowFrames)
    : windowFrames(windowFrames) {
    window.reserve(windowFrames);
}

void FrameProfiler::BeginFrame() {
    if (hasOpenFrame) {
        AddStageTicks(Stage::Frame, SDL_GetPerformanceCounter() - lastBeginTicks);
        window.push_back(currentFrame);
        currentFrame = FrameSample{};

        if (window.size() >= windowFrames) {
            Report();
            window.clear();
        }
    }

    lastBeginTicks = SDL_GetPerformanceCounter();
    hasOpenFrame = true;
}

void FrameProfiler::AddCount(const Counter counter, const Uint64 amount) {
    currentFrame.counters[static_cast<size_t>(counter)] += amount;
}

void FrameProfiler::StartWindow(const size_t windowFrames, const char *modeLabel) {
    this->windowFrames = windowFrames;
    this->modeLabel = modeLabel;
    window.clear();
    window.reserve(windowFrames);
    currentFrame = FrameSample{};
    hasOpenFrame = false;
}

void FrameProfiler::Report() const {
    if (window.empty()) return;

    const size_t frameCount = window.size();
    SDL_Log("[Profiler] mode=%s frames=%zu (window %zu)", modeLabel.c_str(), frameCount, windowFrames);

    std::vector<double> values(frameCount);
    for (size_t stage = 0; stage < kStageCount; stage += 1) {
        double sum = 0.0;
        for (size_t i = 0; i < frameCount; i += 1) {
            values[i] = window[i].stageMs[stage];
            sum += values[i];
        }
        std::ranges::sort(values);
        const size_t p95Index = static_cast<size_t>(std::ceil(0.95 * static_cast<double>(frameCount))) - 1;
        SDL_Log("[Profiler] %-8s avg %8.3f ms  p95 %8.3f ms  max %8.3f ms", kStageNames[stage],
                sum / static_cast<double>(frameCount), values[std::min(p95Index, frameCount - 1)], values.back());
    }

    for (size_t counter = 0; counter < kCounterCount; counter += 1) {
        Uint64 sum = 0;
        for (const auto &sample: window) sum += sample.counters[counter];
        SDL_Log("[Profiler] %-16s avg %14.1f  sum %llu", kCounterNames[counter],
                static_cast<double>(sum) / static_cast<double>(frameCount), static_cast<unsigned long long>(sum));
    }
}

void FrameProfiler::AddStageTicks(const Stage stage, const Uint64 ticks) {
    currentFrame.stageMs[static_cast<size_t>(stage)] += TicksToMilliseconds(ticks);
}
