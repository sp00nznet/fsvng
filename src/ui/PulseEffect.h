#pragma once

#include <vector>
#include <unordered_set>

namespace fsvng {

class FsNode;

class PulseEffect {
public:
    static PulseEffect& instance();

    struct Pulse {
        std::vector<FsNode*> path;   // root-to-leaf node path
        float position = 0.0f;       // current position along path
        float speed = 3.0f;          // nodes per second
        float peakIntensity = 0.6f;
        float fadeWidth = 2.0f;
    };

    // Per-scene pulse animation state. Owned by a Scene.
    struct SceneState {
        std::vector<Pulse> activePulses_;
        std::unordered_set<FsNode*> litNodes_;
        float spawnTimer_ = 0.0f;
    };
    void setSceneState(SceneState* s) { s_ = s ? s : &defaultState_; }
    SceneState* sceneState() { return s_; }

    void tick(float dt);
    void reset();

private:
    PulseEffect() = default;

    void spawnPulse();
    void collectPath(FsNode* node, std::vector<FsNode*>& path);

    SceneState defaultState_;
    SceneState* s_ = &defaultState_;
};

} // namespace fsvng
