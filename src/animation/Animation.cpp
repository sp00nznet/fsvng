#include "animation/Animation.h"
#include "animation/Morph.h"
#include "animation/Scheduler.h"
#include "core/PlatformUtils.h"

#include <cstring>

namespace fsvng {

Animation& Animation::instance() {
    static Animation inst;
    return inst;
}

void Animation::init() {
    active_ = false;
    needRedraw_ = true;
    framerate_ = 0.0f;
    prevTime_ = -1.0;
    sumFrametimes_ = 0.0;
    frametimes_.clear();
    frametimes_.push_back(0.0);
    frameIndex_ = 0;
}

void Animation::framerateIteration(bool frameRendered) {
    if (frametimes_.empty()) {
        // First-time initialization
        frametimes_.push_back(0.0);
        return;
    }

    if (!frameRendered) {
        // Entering steady state (STOP_TIMING)
        prevTime_ = -1.0;
        return;
    }

    // FRAME_RENDERED
    double tNow = PlatformUtils::getTime();

    if (prevTime_ < 0.0) {
        // First frame after steady state
        prevTime_ = tNow;
        return;
    }

    // Time for last rendered frame
    double deltaT = tNow - prevTime_;
    prevTime_ = tNow;

    int numFrametimes = static_cast<int>(frametimes_.size());

    // Update frametime buffer and sum
    sumFrametimes_ -= frametimes_[frameIndex_];
    frametimes_[frameIndex_] = deltaT;
    sumFrametimes_ += deltaT;

    // Update framerate
    double averageFrametime = sumFrametimes_ / static_cast<double>(numFrametimes);
    if (averageFrametime > 0.0) {
        framerate_ = static_cast<float>(1.0 / averageFrametime);
    }

    // Check that the frametime buffer isn't too small
    if (sumFrametimes_ < FRAMERATE_AVERAGE_TIME) {
        // Insert a new element into the frametime buffer, right
        // after the current element, with value equal to that
        // of the oldest element
        numFrametimes++;
        frametimes_.resize(numFrametimes);
        if (frameIndex_ < (numFrametimes - 2)) {
            std::memmove(&frametimes_[frameIndex_ + 2], &frametimes_[frameIndex_ + 1],
                         (numFrametimes - frameIndex_ - 2) * sizeof(double));
        } else {
            frametimes_[frameIndex_ + 1] = frametimes_[0];
        }
        sumFrametimes_ += frametimes_[frameIndex_ + 1];
    }

    // Check that the frametime buffer isn't too big
    if ((sumFrametimes_ > (FRAMERATE_AVERAGE_TIME + 1.0)) && (numFrametimes > 4)) {
        // Remove the oldest element from the frametime array
        if (frameIndex_ < (numFrametimes - 1)) {
            sumFrametimes_ -= frametimes_[frameIndex_ + 1];
            std::memmove(&frametimes_[frameIndex_ + 1], &frametimes_[frameIndex_ + 2],
                         (numFrametimes - frameIndex_ - 2) * sizeof(double));
        } else {
            sumFrametimes_ -= frametimes_[0];
            std::memmove(&frametimes_[0], &frametimes_[1],
                         (numFrametimes - 1) * sizeof(double));
        }
        numFrametimes--;
        frametimes_.resize(numFrametimes);
    }

    frameIndex_ = (frameIndex_ + 1) % numFrametimes;
}

void Animation::tick() {
    bool stateChanged = false;
    bool scheventsPending = false;

    // Update morphing variables
    stateChanged = MorphEngine::instance().iteration();

    if (needRedraw_) {
        // The actual rendering is done in the main loop (App::run).
        // Here we just update framerate and run scheduled events.

        // Update framerate
        framerateIteration(true);

        // Execute scheduled events
        scheventsPending = Scheduler::instance().iteration();

        if (!scheventsPending) {
            needRedraw_ = false;
        }
    }

    if (!stateChanged && !scheventsPending) {
        // Entering steady state
        framerateIteration(false);
        active_ = false;
    }
}

void Animation::requestRedraw() {
    active_ = true;
    needRedraw_ = true;
}

} // namespace fsvng
