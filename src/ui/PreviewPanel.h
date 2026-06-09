#pragma once

#include <string>

namespace fsvng {

class FsNode;

// ============================================================================
// PreviewPanel - shows a quick preview of the currently selected file:
// text content for text files, a thumbnail for images, and basic info
// otherwise. Content is loaded lazily when the selection changes.
// ============================================================================
class PreviewPanel {
public:
    static PreviewPanel& instance();

    void draw();

private:
    PreviewPanel() = default;

    enum class Kind { None, Text, Image, Binary, TooLarge, Unreadable };

public:
    // Per-scene preview state. Owned by a Scene; panel points at the active
    // one. texture_ is a GL resource freed when the owning Scene is destroyed
    // (Scene calls releaseState), never on activation.
    struct SceneState {
        FsNode* currentNode_ = nullptr;
        Kind kind_ = Kind::None;
        std::string text_;        // text preview (possibly truncated)
        bool truncated_ = false;
        unsigned int texture_ = 0; // GL texture id for image preview (0 = none)
        int imgW_ = 0;
        int imgH_ = 0;
        std::string info_;        // human-readable status / metadata line
    };
    void setSceneState(SceneState* s) { s_ = s ? s : &defaultState_; }
    SceneState* sceneState() { return s_; }
    // Free the GL texture held by a scene's state (call on scene close).
    static void releaseState(SceneState& st);

private:
    void loadFor(FsNode* node);
    void clear();

    SceneState defaultState_;
    SceneState* s_ = &defaultState_;
};

} // namespace fsvng
