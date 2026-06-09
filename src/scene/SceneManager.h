#pragma once

#include <memory>
#include <string>
#include <vector>

namespace fsvng {

struct Scene;

// ============================================================================
// SceneManager - owns up to 4 Scenes and tracks which is active/focused.
//
// activate(scene) re-points every per-scene singleton (Camera, FsTree,
// GeometryManager, the panels, MainWindow) at that Scene's state blocks, so
// the existing singleton-based code transparently operates on the chosen pane.
// ============================================================================
class SceneManager {
public:
    static constexpr int kMaxScenes = 4;

    static SceneManager& instance();

    // Ensure at least one scene exists and is active. Call once at startup.
    void init();

    // Make `sc`'s state blocks the active target of all singletons.
    void activate(Scene* sc);

    Scene* active() const { return active_; }
    Scene* focused() const { return focused_; }
    void setFocused(Scene* sc) { focused_ = sc; }

    int sceneCount() const { return static_cast<int>(scenes_.size()); }
    Scene* sceneAt(int i) const {
        return (i >= 0 && i < sceneCount()) ? scenes_[i].get() : nullptr;
    }
    int indexOf(Scene* sc) const;

    // Create a new scene (up to kMaxScenes). Returns nullptr if at capacity.
    Scene* addScene();

    // Stop threads/watchers and free GL resources while the GL context is
    // still alive, then drop all scenes. Call from App::shutdown().
    void shutdown();

private:
    SceneManager() = default;

    // Point all singletons back at their internal default state blocks.
    void resetSingletonsToDefault();

    std::vector<std::unique_ptr<Scene>> scenes_;
    Scene* active_ = nullptr;
    Scene* focused_ = nullptr;
};

} // namespace fsvng
