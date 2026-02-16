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
    void setHighlightNode(FsNode* node) { highlightNode_ = node; }
    FsNode* getHighlightNode() const { return highlightNode_; }

    // MapV helpers
    double mapvNodeZ0(FsNode* node) const;
    double mapvMaxExpandedHeight(FsNode* dnode) const;

    // TreeV helpers
    bool treevIsLeaf(FsNode* node) const;
    double treevPlatformR0(FsNode* dnode) const;
    double treevPlatformTheta(FsNode* dnode) const;
    double treevMaxLeafHeight(FsNode* dnode) const;
    void treevGetExtents(FsNode* dnode, RTvec* extC0, RTvec* extC1) const;

    FsvMode currentMode() const { return mode_; }
    MatrixStack& modelStack() { return modelStack_; }

    // Signal that geometry needs uncached redraw
    void queueUncachedDraw();

    // Draw stage management
    int lowDrawStage() const { return lowDrawStage_; }
    int highDrawStage() const { return highDrawStage_; }
    void advanceLowDrawStage() { if (lowDrawStage_ <= 1) ++lowDrawStage_; }
    void advanceHighDrawStage() { if (highDrawStage_ <= 1) ++highDrawStage_; }

private:
    GeometryManager() = default;

    void treevGetExtentsRecursive(FsNode* dnode, RTvec* c0, RTvec* c1,
                                  double r0, double theta) const;

    FsvMode mode_ = FSV_NONE;
    MatrixStack modelStack_;
    FsNode* highlightNode_ = nullptr;

    // TreeV state
    double treevCoreRadius_ = 8192.0;

    // Draw stage tracking (replaces display list caching)
    // Stage 0: Full recursive draw, some geometry rebuilt
    // Stage 1: Full recursive draw, no rebuild, capture
    // Stage 2: Fast draw using cached data
    int lowDrawStage_ = 0;
    int highDrawStage_ = 0;

    friend class TreeVLayout;
};

} // namespace fsvng
