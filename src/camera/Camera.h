#pragma once

#include "core/Types.h"
#include "animation/Morph.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>

namespace fsvng {

class FsNode;

struct CameraState {
    double theta = 0.0;        // heading
    double phi = 0.0;          // elevation
    double distance = 1000.0;  // distance to target
    double fov = 60.0;         // field of view degrees
    double nearClip = 1.0;     // near clipping plane
    double farClip = 100000.0; // far clipping plane
    double panPart = 1.0;      // pan fraction [0,1]
    bool manualControl = false;
};

struct MapVCameraState : CameraState {
    XYZvec target{};
};

struct TreeVCameraState : CameraState {
    RTZvec target{};
};

// Constants
constexpr double NEAR_TO_DISTANCE_RATIO = 0.5;
constexpr double FAR_TO_NEAR_RATIO = 128.0;

class Camera {
public:
    static Camera& instance();

    // Per-scene camera state. Heap-stable (owned by a Scene) so that the
    // double* pointers captured by morph animations and the history vector
    // stay valid and unique per pane. Camera points at the active one.
    struct SceneState {
        // Camera state union (MapV vs TreeV share storage).
        union AnyCameraState {
            MapVCameraState mapv;
            TreeVCameraState treev;
            AnyCameraState() : mapv{} {}
        };
        AnyCameraState currentCamera_;
        AnyCameraState preBirdseyeCamera_;
        AnyCameraState savedCamera_;
        bool hasSavedState_ = false;
        FsvMode currentMode_ = FSV_NONE;
        bool moving_ = false;
        bool birdseyeActive_ = false;
        std::vector<FsNode*> history_;
        FsNode* currentNode_ = nullptr;
    };

    // Re-point Camera at a scene's state (nullptr restores the default).
    void setSceneState(SceneState* s) { s_ = s ? s : &defaultState_; }
    SceneState* sceneState() { return s_; }

    void init(FsvMode mode, bool initialView);

    // Point camera at node with morph animation
    void lookAt(FsNode* node);
    void lookAtFull(FsNode* node, MorphType mtype, double panTimeOverride);
    void lookAtPrevious();

    // Two-stage L-shaped pan (TreeV)
    void treevLpanLookAt(FsNode* node, double panTimeOverride);

    // Camera controls
    void dolly(double dk);
    void revolve(double dtheta, double dphi);
    void pan(double dx, double dy);
    void birdseyeView(bool goingUp);

    // Halt/finish camera movement
    void panFinish();
    void panBreak();

    bool isMoving() const { return s_->moving_; }
    bool isBirdseyeActive() const { return s_->birdseyeActive_; }

    // Get the view matrix for rendering
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix(float aspectRatio) const;

    // Access camera state
    CameraState& state() { return *reinterpret_cast<CameraState*>(&s_->currentCamera_); }
    const CameraState& state() const { return *reinterpret_cast<const CameraState*>(&s_->currentCamera_); }

    MapVCameraState& mapvState() { return s_->currentCamera_.mapv; }
    TreeVCameraState& treevState() { return s_->currentCamera_.treev; }

    const MapVCameraState& mapvState() const { return s_->currentCamera_.mapv; }
    const TreeVCameraState& treevState() const { return s_->currentCamera_.treev; }

    // Snapshot / restore the full camera state. Used to preserve the view
    // across a rescan (e.g. after a file operation or live refresh).
    void saveState();
    void restoreState();

    // Get the current mode (set during init)
    FsvMode currentMode() const { return s_->currentMode_; }

    // Get current node of interest
    FsNode* currentNode() const { return s_->currentNode_; }

private:
    Camera() = default;

    double fieldDiameter(double fov, double distance) const;
    double fieldDistance(double fov, double diameter) const;

    double mapvLookAt(FsNode* node, MorphType mtype, double panTimeOverride);
    double treevLookAt(FsNode* node, MorphType mtype, double panTimeOverride);

    void mapvGetCameraPosition(const CameraState* cam, const XYZvec* target, XYZvec* pos) const;
    void treevGetCameraPosition(const CameraState* cam, const RTZvec* target, RTZvec* pos) const;

    // Default per-scene state used when no Scene is active (single-pane and
    // tests). Scenes provide their own SceneState via setSceneState().
    SceneState defaultState_;
    SceneState* s_ = &defaultState_;

    // Pan timing constants
    static constexpr double MAPV_MIN_PAN_TIME = 0.5;
    static constexpr double MAPV_MAX_PAN_TIME = 4.0;
    static constexpr double TREEV_MIN_PAN_TIME = 1.0;
    static constexpr double TREEV_MAX_PAN_TIME = 4.0;
    static constexpr double TREEV_AVG_VELOCITY = 1024.0;
};

} // namespace fsvng
