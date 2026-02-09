#pragma once

#include <vector>

namespace fsvng {

class Animation {
public:
    static Animation& instance();

    void init();

    // Called every frame from main loop
    void tick();

    // Request a redraw (replaces redraw())
    void requestRedraw();

    // Check if animation is active
    bool isActive() const { return active_; }

    float getFramerate() const { return framerate_; }
    bool needsRedraw() const { return needRedraw_; }
    void clearRedrawFlag() { needRedraw_ = false; }

private:
    Animation() = default;
    void framerateIteration(bool frameRendered);

    bool active_ = false;
    bool needRedraw_ = true;
    float framerate_ = 0.0f;

    // Framerate tracking
    double prevTime_ = -1.0;
    double sumFrametimes_ = 0.0;
    std::vector<double> frametimes_;
    int frameIndex_ = 0;

    static constexpr double FRAMERATE_AVERAGE_TIME = 4.0;
};

} // namespace fsvng
