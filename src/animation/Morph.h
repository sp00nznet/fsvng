#pragma once

#include <functional>
#include <list>
#include <cmath>
#include "core/Types.h"

namespace fsvng {

enum class MorphType {
    Linear,
    Quadratic,
    InvQuadratic,
    Sigmoid,
    SigmoidAccel
};

class Morph {
public:
    MorphType type;
    double* var;
    double startValue;
    double endValue;
    double tStart;
    double tEnd;
    std::function<void(Morph*)> stepCb;
    std::function<void(Morph*)> endCb;
    void* data = nullptr;
    Morph* next = nullptr;  // for chaining
};

class MorphEngine {
public:
    static MorphEngine& instance();

    // Full morph with callbacks
    void morphFull(double* var, MorphType type, double targetValue, double duration,
                   std::function<void(Morph*)> stepCb = nullptr,
                   std::function<void(Morph*)> endCb = nullptr,
                   void* data = nullptr);

    // Simple morph (no callbacks)
    void morph(double* var, MorphType type, double targetValue, double duration);

    // Force morph to finish (jump to end value, call end callback)
    void morphFinish(double* var);

    // Cancel morph (no callbacks called)
    void morphBreak(double* var);

    // Update all morphs. Returns true if any state changed.
    bool iteration();

    // Check if any morphs are active
    bool isActive() const { return !morphQueue_.empty(); }

private:
    MorphEngine() = default;
    Morph* lastStage(Morph* m);
    std::list<Morph*>::iterator findByVar(double* var);

    std::list<Morph*> morphQueue_;
};

} // namespace fsvng
