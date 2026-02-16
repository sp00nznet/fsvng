#include "ui/PulseEffect.h"
#include "ui/ThemeManager.h"
#include "core/FsTree.h"
#include "core/FsNode.h"

#include <algorithm>
#include <cstdlib>
#include <cmath>

namespace fsvng {

PulseEffect& PulseEffect::instance() {
    static PulseEffect s;
    return s;
}

void PulseEffect::reset() {
    // Clear glow on all previously lit nodes
    for (FsNode* n : litNodes_) {
        n->glowIntensity = 0.0f;
    }
    litNodes_.clear();
    activePulses_.clear();
    spawnTimer_ = 0.0f;
}

void PulseEffect::tick(float dt) {
    const Theme& theme = ThemeManager::instance().currentTheme();
    if (!theme.pulseEnabled) {
        if (!activePulses_.empty()) {
            reset();
        }
        return;
    }

    // Clear glow on previously lit nodes
    for (FsNode* n : litNodes_) {
        n->glowIntensity = 0.0f;
    }
    litNodes_.clear();

    // Advance existing pulses
    for (auto& pulse : activePulses_) {
        pulse.position += pulse.speed * dt;
    }

    // Remove finished pulses
    activePulses_.erase(
        std::remove_if(activePulses_.begin(), activePulses_.end(),
            [](const Pulse& p) {
                return p.position > static_cast<float>(p.path.size()) + p.fadeWidth;
            }),
        activePulses_.end());

    // Spawn new pulses
    spawnTimer_ += dt;
    if (spawnTimer_ >= theme.pulseSpawnInterval) {
        spawnTimer_ -= theme.pulseSpawnInterval;
        spawnPulse();
    }

    // Compute per-node glow from all active pulses
    for (const auto& pulse : activePulses_) {
        for (int i = 0; i < static_cast<int>(pulse.path.size()); ++i) {
            float dist = std::abs(pulse.position - static_cast<float>(i));
            if (dist < pulse.fadeWidth) {
                float glow = pulse.peakIntensity * (1.0f - dist / pulse.fadeWidth);
                FsNode* node = pulse.path[i];
                node->glowIntensity = std::max(node->glowIntensity, glow);
                litNodes_.insert(node);
            }
        }
    }
}

void PulseEffect::spawnPulse() {
    FsNode* rootDir = FsTree::instance().rootDir();
    if (!rootDir) return;

    const Theme& theme = ThemeManager::instance().currentTheme();

    std::vector<FsNode*> path;
    path.push_back(rootDir);
    collectPath(rootDir, path);

    if (path.size() < 2) return;

    Pulse p;
    p.path = std::move(path);
    p.position = 0.0f;
    p.speed = theme.pulseSpeed;
    p.peakIntensity = theme.pulsePeakIntensity;
    p.fadeWidth = theme.pulseFadeWidth;
    activePulses_.push_back(std::move(p));
}

void PulseEffect::collectPath(FsNode* node, std::vector<FsNode*>& path) {
    // Random walk from node to a leaf
    if (node->children.empty()) return;

    // Pick a random child
    int idx = std::rand() % static_cast<int>(node->children.size());
    FsNode* child = node->children[idx].get();
    path.push_back(child);

    if (child->isDir() && !child->children.empty()) {
        collectPath(child, path);
    }
}

} // namespace fsvng
