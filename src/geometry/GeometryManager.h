#pragma once

#include "core/Types.h"
#include <vector>
#include <glm/glm.hpp>

namespace fsvng {

class FsNode;
class MeshBuffer;

// Explicit matrix stack replacing glPushMatrix/glPopMatrix
class MatrixStack {
public:
    void push() { stack_.push_back(current_); }
    void pop() { if (!stack_.empty()) { current_ = stack_.back(); stack_.pop_back(); } }
    void loadIdentity() { current_ = glm::mat4(1.0f); }
    void translate(float x, float y, float z);
    void rotate(float angle, float x, float y, float z);
    void scale(float x, float y, float z);
    const glm::mat4& top() const { return current_; }
    void set(const glm::mat4& m) { current_ = m; }
private:
    glm::mat4 current_{1.0f};
    std::vector<glm::mat4> stack_;
};

class GeometryManager {
public:
    static GeometryManager& instance();

    // Per-scene geometry state. Owned by a Scene so each pane has its own
    // view mode, highlight, and draw-stage tracking. The transient
    // modelStack_ stays global scratch (one pane's draw pass completes before
    // the next begins).
    struct SceneState {
        FsvMode mode_ = FSV_NONE;
        FsNode* highlightNode_ = nullptr;
        double treevCoreRadius_ = 8192.0;
        int lowDrawStage_ = 0;
        int highDrawStage_ = 0;
    };

    void setSceneState(SceneState* s) { s_ = s ? s : &defaultState_; }
    SceneState* sceneState() { return s_; }

    // Set the TreeV core radius (written by TreeVLayout each layout pass).
    void setTreevCoreRadius(double r) { s_->treevCoreRadius_ = r; }

    void init(FsvMode mode);
    void draw(const glm::mat4& view, const glm::mat4& projection, bool highDetail);
    void drawForPicking(const glm::mat4& view, const glm::mat4& projection);

    // Queue a directory for geometry rebuild
    void queueRebuild(FsNode* dnode);

    // Hook for camera pan completion
    void cameraPanFinished();

    // Hook for collapse/expand
    void colexpInitiated(FsNode* dnode);
    void colexpInProgress(FsNode* dnode);

    // Free all geometry
    void freeAll();

    // Check if node should highlight
    bool shouldHighlight(FsNode* node) const;
    void setHighlightNode(FsNode* node) { s_->highlightNode_ = node; }
    FsNode* getHighlightNode() const { return s_->highlightNode_; }

    // MapV helpers
    double mapvNodeZ0(FsNode* node) const;
    double mapvMaxExpandedHeight(FsNode* dnode) const;

    // TreeV helpers
    bool treevIsLeaf(FsNode* node) const;
    double treevPlatformR0(FsNode* dnode) const;
    double treevPlatformTheta(FsNode* dnode) const;
    double treevMaxLeafHeight(FsNode* dnode) const;
    void treevGetExtents(FsNode* dnode, RTvec* extC0, RTvec* extC1) const;

    FsvMode currentMode() const { return s_->mode_; }
    MatrixStack& modelStack() { return modelStack_; }

    // Signal that geometry needs uncached redraw
    void queueUncachedDraw();

    // Draw stage management
    int lowDrawStage() const { return s_->lowDrawStage_; }
    int highDrawStage() const { return s_->highDrawStage_; }
    void advanceLowDrawStage() { if (s_->lowDrawStage_ <= 1) ++s_->lowDrawStage_; }
    void advanceHighDrawStage() { if (s_->highDrawStage_ <= 1) ++s_->highDrawStage_; }

private:
    GeometryManager() = default;

    void treevGetExtentsRecursive(FsNode* dnode, RTvec* c0, RTvec* c1,
                                  double r0, double theta) const;

    // Transient render scratch (global; one draw pass at a time).
    MatrixStack modelStack_;

    SceneState defaultState_;
    SceneState* s_ = &defaultState_;
};

} // namespace fsvng
