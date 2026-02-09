#include "animation/Morph.h"
#include "animation/Animation.h"
#include "core/PlatformUtils.h"

#include <cmath>

namespace fsvng {

MorphEngine& MorphEngine::instance() {
    static MorphEngine inst;
    return inst;
}

Morph* MorphEngine::lastStage(Morph* m) {
    while (m->next != nullptr) {
        m = m->next;
    }
    return m;
}

std::list<Morph*>::iterator MorphEngine::findByVar(double* var) {
    for (auto it = morphQueue_.begin(); it != morphQueue_.end(); ++it) {
        if ((*it)->var == var) {
            return it;
        }
    }
    return morphQueue_.end();
}

void MorphEngine::morphFull(double* var, MorphType type, double targetValue, double duration,
                            std::function<void(Morph*)> stepCb,
                            std::function<void(Morph*)> endCb,
                            void* data) {
    double tNow = PlatformUtils::getTime();

    // Create new morph record
    Morph* newMorph = new Morph();
    newMorph->type = type;
    newMorph->var = var;
    newMorph->startValue = *var;
    newMorph->endValue = targetValue;
    newMorph->tStart = tNow;
    newMorph->tEnd = tNow + duration;
    newMorph->stepCb = std::move(stepCb);
    newMorph->endCb = std::move(endCb);
    newMorph->data = data;
    newMorph->next = nullptr;

    // Check to see if the variable is already undergoing morphing
    auto it = findByVar(var);
    if (it == morphQueue_.end()) {
        // Variable is not being morphed
        // Make sure we're animating
        Animation::instance().requestRedraw();
        // Add new morph to queue
        morphQueue_.push_front(newMorph);
    } else {
        // Variable is already undergoing morphing. Append
        // new stage to the incumbent morph record(s)
        Morph* morph = *it;
        Morph* mlast = lastStage(morph);
        newMorph->tStart = mlast->tEnd;
        newMorph->tEnd = mlast->tEnd + duration;
        newMorph->startValue = mlast->endValue;
        mlast->next = newMorph;
    }
}

void MorphEngine::morph(double* var, MorphType type, double targetValue, double duration) {
    morphFull(var, type, targetValue, duration, nullptr, nullptr, nullptr);
}

void MorphEngine::morphFinish(double* var) {
    auto it = findByVar(var);
    if (it == morphQueue_.end()) {
        return;  // Variable is not being morphed
    }
    Morph* morph = *it;
    morph->tEnd = 0.0;
}

void MorphEngine::morphBreak(double* var) {
    auto it = findByVar(var);
    if (it == morphQueue_.end()) {
        return;  // Variable is not being morphed
    }

    // Remove morph record from queue
    Morph* morph = *it;
    morphQueue_.erase(it);

    // Free morph record, and any subsequent stages
    while (morph != nullptr) {
        Morph* mnext = morph->next;
        delete morph;
        morph = mnext;
    }
}

bool MorphEngine::iteration() {
    double tNow = PlatformUtils::getTime();
    bool stateChanged = false;

    // Perform update of all morphing variables
    auto it = morphQueue_.begin();
    while (it != morphQueue_.end()) {
        Morph* morph = *it;

        if (tNow >= morph->tEnd) {
            // Morph complete - assign end value
            *(morph->var) = morph->endValue;
            stateChanged = true;

            // Call end callback, if there is one
            if (morph->endCb) {
                morph->endCb(morph);
            }

            if (morph->next != nullptr) {
                // Drop in next stage
                *it = morph->next;
            } else {
                // Remove record from queue
                it = morphQueue_.erase(it);
            }
            delete morph;
            continue;
        }

        // Update variable value using appropriate remapping
        double percent = (tNow - morph->tStart) / (morph->tEnd - morph->tStart);

        switch (morph->type) {
            case MorphType::Linear:
                // No remapping
                break;

            case MorphType::Quadratic:
                // Parabolic curve
                percent = percent * percent;
                break;

            case MorphType::InvQuadratic:
                // Inverted parabolic curve
                percent = 1.0 - (1.0 - percent) * (1.0 - percent);
                break;

            case MorphType::Sigmoid:
                // Sigmoidal (S-like) remapping
                percent = 0.5 * (1.0 - std::cos(PI * percent));
                break;

            case MorphType::SigmoidAccel:
                // Sigmoidal, with acceleration
                percent = 0.5 * (1.0 - std::cos(PI * percent * percent));
                break;
        }

        // Assign new variable value
        *(morph->var) = morph->startValue + percent * (morph->endValue - morph->startValue);
        stateChanged = true;

        // Call step callback, if there is one
        if (morph->stepCb) {
            morph->stepCb(morph);
        }

        ++it;
    }

    return stateChanged;
}

} // namespace fsvng
