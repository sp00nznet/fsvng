#include "camera/Camera.h"
#include "animation/Animation.h"
#include "animation/Morph.h"
#include "animation/Scheduler.h"
#include "core/FsNode.h"
#include "core/FsTree.h"
#include "core/Types.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace fsvng {

// Geometry constants ported from geometry.h
static constexpr double TREEV_LEAF_NODE_EDGE = 256.0;
static constexpr double TREEV_PLATFORM_SPACING_DEPTH = 2048.0;

Camera& Camera::instance() {
    static Camera inst;
    return inst;
}

double Camera::fieldDiameter(double fov, double distance) const {
    return 2.0 * distance * std::tan(rad(0.5 * fov));
}

double Camera::fieldDistance(double fov, double diameter) const {
    return diameter * (0.5 / std::tan(rad(0.5 * fov)));
}

// ============================================================================
// Pan callbacks
// ============================================================================

static void panStepCb(Morph* /*morph*/) {
    Animation::instance().requestRedraw();
}

static void panEndCb(Morph* /*morph*/) {
    Animation::instance().requestRedraw();
    Camera::instance().panBreak();  // Clean up any remaining morphs
    // Note: moving_ is set to false in lookAtFull's end callback below
}

// Internal end callback that also clears the moving flag
static void masterPanEndCb(Morph* /*morph*/) {
    Animation::instance().requestRedraw();
    // The Camera singleton's moving_ flag can't be set directly from
    // a free function. Instead we use a scheduled event approach below.
}

// ============================================================================
// Initialization
// ============================================================================

void Camera::init(FsvMode mode, bool initialView) {
    currentMode_ = mode;

    CameraState& cam = state();
    cam.fov = 60.0;
    cam.panPart = 1.0;
    cam.manualControl = false;

    FsNode* rootDir = FsTree::instance().rootDir();

    switch (mode) {
        case FSV_DISCV: {
            double rootRadius = 1000.0;  // Default; real value comes from geometry
            if (rootDir) {
                rootRadius = rootDir->discvGeom.radius;
                if (rootRadius < EPSILON) rootRadius = 1000.0;
            }
            double d = fieldDistance(cam.fov, 2.0 * rootRadius);
            if (initialView) {
                cam.distance = 2.0 * d;
            } else {
                cam.distance = 3.0 * d;
            }
            discvState().target.x = 0.0;
            discvState().target.y = 0.0;
            cam.nearClip = 0.9375 * cam.distance;
            cam.farClip = 1.0625 * cam.distance;
            break;
        }

        case FSV_MAPV: {
            double rootWidth = 1000.0;
            double rootHeight = 100.0;
            if (rootDir) {
                rootWidth = rootDir->mapvWidth();
                rootHeight = rootDir->mapvGeom.height;
                if (rootWidth < EPSILON) rootWidth = 1000.0;
                if (rootHeight < EPSILON) rootHeight = 100.0;
            }
            double d1 = fieldDistance(cam.fov, rootWidth);
            double d2 = rootHeight;
            double d = std::max(d1, d2);
            // Both views use an overhead angle for treemap
            cam.theta = 270.0;
            cam.phi = initialView ? 52.5 : 52.5;
            cam.distance = initialView ? 2.0 * d : 1.5 * d;
            mapvState().target.x = 0.0;
            mapvState().target.y = 0.0;
            mapvState().target.z = rootHeight * 0.5;
            cam.nearClip = NEAR_TO_DISTANCE_RATIO * cam.distance;
            cam.farClip = FAR_TO_NEAR_RATIO * cam.nearClip;
            break;
        }

        case FSV_TREEV: {
            double extRadius = 1000.0;
            if (rootDir) {
                extRadius = rootDir->treevGeom.platform.depth;
                if (extRadius < EPSILON) extRadius = 1000.0;
                // Include a generous view of the tree
                extRadius += 8192.0; // core radius
            }
            double d = fieldDistance(cam.fov, 2.0 * extRadius);
            // Start from a comfortable overhead view
            cam.theta = 0.0;
            cam.phi = initialView ? 75.0 : 60.0;
            cam.distance = initialView ? 4.0 * d : 2.0 * d;
            treevState().target.r = 0.0;
            treevState().target.theta = 90.0;
            treevState().target.z = 0.0;
            cam.nearClip = NEAR_TO_DISTANCE_RATIO * cam.distance;
            cam.farClip = FAR_TO_NEAR_RATIO * cam.nearClip;
            break;
        }

        default:
            break;
    }

    moving_ = false;
    birdseyeActive_ = false;
    history_.clear();
    currentNode_ = rootDir;
}

// ============================================================================
// Pan finish / break
// ============================================================================

void Camera::panFinish() {
    auto& me = MorphEngine::instance();
    CameraState& cam = state();

    me.morphFinish(&cam.theta);
    me.morphFinish(&cam.phi);
    me.morphFinish(&cam.distance);
    me.morphFinish(&cam.fov);
    me.morphFinish(&cam.nearClip);
    me.morphFinish(&cam.farClip);
    me.morphFinish(&cam.panPart);

    switch (currentMode_) {
        case FSV_DISCV:
            me.morphFinish(&discvState().target.x);
            me.morphFinish(&discvState().target.y);
            break;
        case FSV_MAPV:
            me.morphFinish(&mapvState().target.x);
            me.morphFinish(&mapvState().target.y);
            me.morphFinish(&mapvState().target.z);
            break;
        case FSV_TREEV:
            me.morphFinish(&treevState().target.r);
            me.morphFinish(&treevState().target.theta);
            me.morphFinish(&treevState().target.z);
            break;
        default:
            break;
    }
}

void Camera::panBreak() {
    auto& me = MorphEngine::instance();
    CameraState& cam = state();

    me.morphBreak(&cam.theta);
    me.morphBreak(&cam.phi);
    me.morphBreak(&cam.distance);
    me.morphBreak(&cam.fov);
    me.morphBreak(&cam.nearClip);
    me.morphBreak(&cam.farClip);
    me.morphBreak(&cam.panPart);

    switch (currentMode_) {
        case FSV_DISCV:
            me.morphBreak(&discvState().target.x);
            me.morphBreak(&discvState().target.y);
            break;
        case FSV_MAPV:
            me.morphBreak(&mapvState().target.x);
            me.morphBreak(&mapvState().target.y);
            me.morphBreak(&mapvState().target.z);
            break;
        case FSV_TREEV:
            me.morphBreak(&treevState().target.r);
            me.morphBreak(&treevState().target.theta);
            me.morphBreak(&treevState().target.z);
            break;
        default:
            break;
    }
}

// ============================================================================
// Mode-specific lookAt helpers
// ============================================================================

double Camera::discvLookAt(FsNode* node, MorphType mtype, double panTimeOverride) {
    auto& me = MorphEngine::instance();
    CameraState& cam = state();

    // Construct desired camera state
    double nodeRadius = node->discvGeom.radius;
    if (nodeRadius < EPSILON) nodeRadius = 100.0;

    double newDistance = 2.0 * fieldDistance(cam.fov, 2.0 * nodeRadius);
    double newNearClip = 0.9375 * newDistance;
    double newFarClip = 1.0625 * newDistance;

    double targetX = node->discvGeom.pos.x;
    double targetY = node->discvGeom.pos.y;

    // Duration of pan
    double panTime;
    if (panTimeOverride > 0.0) {
        panTime = panTimeOverride;
    } else {
        // TODO: write a proper pan_time function
        panTime = 2.0;
    }

    // Get the camera moving
    me.morph(&cam.distance, mtype, newDistance, panTime);
    me.morph(&cam.nearClip, mtype, newNearClip, panTime);
    me.morph(&cam.farClip, mtype, newFarClip, panTime);
    me.morph(&discvState().target.x, mtype, targetX, panTime);
    me.morph(&discvState().target.y, mtype, targetY, panTime);

    return panTime;
}

void Camera::mapvGetCameraPosition(const CameraState* cam, const XYZvec* target, XYZvec* pos) const {
    double sinTheta = std::sin(rad(cam->theta));
    double cosTheta = std::cos(rad(cam->theta));
    double sinPhi = std::sin(rad(cam->phi));
    double cosPhi = std::cos(rad(cam->phi));

    pos->x = target->x + cam->distance * cosTheta * cosPhi;
    pos->y = target->y + cam->distance * sinTheta * cosPhi;
    pos->z = target->z + cam->distance * sinPhi;
}

double Camera::mapvLookAt(FsNode* node, MorphType mtype, double panTimeOverride) {
    auto& me = MorphEngine::instance();
    CameraState& cam = state();

    FsNode* rootDir = FsTree::instance().rootDir();

    // Get target node geometry
    double nodeWidth = node->mapvWidth();
    double nodeDepth = node->mapvDepth();
    double nodeCenterX = node->mapvCenterX();
    double nodeCenterY = node->mapvCenterY();
    double nodeHeight = node->mapvGeom.height;

    // Target point
    XYZvec newTarget;
    newTarget.x = nodeCenterX;
    newTarget.y = nodeCenterY;
    newTarget.z = nodeHeight;  // Approximate z0 + height

    // Viewing angles
    double newTheta = 270.0;
    if (rootDir) {
        double rootWidth = rootDir->mapvWidth();
        if (rootWidth > EPSILON) {
            newTheta = 270.0 + 45.0 * newTarget.x / rootWidth;
        }
    }

    double newPhi;
    if (node == rootDir) {
        newPhi = 52.5;
    } else if (node->parent) {
        double parentDepth = node->parent->mapvDepth();
        double parentC0y = node->parent->mapvGeom.c0.y;
        if (parentDepth > EPSILON) {
            newPhi = 45.0 + 15.0 * (newTarget.y - parentC0y) / parentDepth;
        } else {
            newPhi = 45.0;
        }
    } else {
        newPhi = 45.0;
    }

    // Distance from target point
    double k = std::sqrt(nodeWidth * nodeDepth);
    double diameter = SQRT_2 * std::max(k, 0.5 * std::max(nodeWidth, nodeDepth));
    double dirMultiplier;
    if (node->isDir()) {
        diameter = std::max(diameter, nodeHeight);
        newTarget.z += 0.5 * nodeHeight;
        dirMultiplier = 1.25;
    } else {
        dirMultiplier = 2.0;
    }
    double newDistance = dirMultiplier * fieldDistance(cam.fov, diameter);

    // Clipping plane distances
    double newNearClip = NEAR_TO_DISTANCE_RATIO * newDistance;
    double newFarClip = FAR_TO_NEAR_RATIO * newNearClip;

    // Overall travel vector
    XYZvec cameraPos, newCamPos, delta;
    XYZvec currentTarget = mapvState().target;
    mapvGetCameraPosition(&cam, &currentTarget, &cameraPos);

    // Build a temporary camera state for the new position
    CameraState tempCam = cam;
    tempCam.theta = newTheta;
    tempCam.phi = newPhi;
    tempCam.distance = newDistance;
    mapvGetCameraPosition(&tempCam, &newTarget, &newCamPos);

    delta.x = newCamPos.x - cameraPos.x;
    delta.y = newCamPos.y - cameraPos.y;
    delta.z = newCamPos.z - cameraPos.z;

    // Determine pan time
    double panTime;
    if (panTimeOverride > 0.0) {
        panTime = panTimeOverride;
    } else {
        double rootDiag = 1000.0;
        if (rootDir) {
            double rw = rootDir->mapvWidth();
            double rd = rootDir->mapvDepth();
            rootDiag = std::sqrt(rw * rw + rd * rd);
            if (rootDiag < EPSILON) rootDiag = 1000.0;
        }
        k = std::sqrt(xyzLen(delta) / rootDiag);
        panTime = std::max(MAPV_MIN_PAN_TIME, std::min(1.0, k) * MAPV_MAX_PAN_TIME);
    }

    // Judge if camera should swing back during the pan
    double xyTravel = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    bool swingBack = xyTravel > (3.0 * std::max(cam.distance, newDistance));

    // Get the camera moving
    me.morph(&cam.theta, mtype, newTheta, panTime);
    me.morph(&cam.phi, mtype, newPhi, panTime);

    if (swingBack) {
        double apgDistance = 1.2 * std::max(newDistance, xyTravel);
        double apgNearClip = NEAR_TO_DISTANCE_RATIO * apgDistance;
        double apgFarClip = FAR_TO_NEAR_RATIO * apgNearClip;

        me.morph(&cam.distance, mtype, apgDistance, 0.5 * panTime);
        me.morph(&cam.distance, mtype, newDistance, 0.5 * panTime);
        me.morph(&cam.nearClip, mtype, apgNearClip, 0.5 * panTime);
        me.morph(&cam.nearClip, mtype, newNearClip, 0.5 * panTime);
        me.morph(&cam.farClip, mtype, apgFarClip, 0.5 * panTime);
        me.morph(&cam.farClip, mtype, newFarClip, 0.5 * panTime);
    } else {
        me.morph(&cam.distance, mtype, newDistance, panTime);
        me.morph(&cam.nearClip, mtype, newNearClip, panTime);
        me.morph(&cam.farClip, mtype, newFarClip, panTime);
    }

    me.morph(&mapvState().target.x, mtype, newTarget.x, panTime);
    me.morph(&mapvState().target.y, mtype, newTarget.y, panTime);
    me.morph(&mapvState().target.z, mtype, newTarget.z, panTime);

    return panTime;
}

void Camera::treevGetCameraPosition(const CameraState* cam, const RTZvec* target, RTZvec* pos) const {
    // Convert target from RTZ to XYZ
    double theta = target->theta;
    XYZvec targetXYZ;
    targetXYZ.x = target->r * std::cos(rad(theta));
    targetXYZ.y = target->r * std::sin(rad(theta));
    targetXYZ.z = target->z;

    // Absolute camera heading
    theta = target->theta + cam->theta - 180.0;
    double sinTheta = std::sin(rad(theta));
    double cosTheta = std::cos(rad(theta));
    double sinPhi = std::sin(rad(cam->phi));
    double cosPhi = std::cos(rad(cam->phi));

    // XYZ position
    XYZvec xyzPos;
    xyzPos.x = targetXYZ.x + cam->distance * cosTheta * cosPhi;
    xyzPos.y = targetXYZ.y + cam->distance * sinTheta * cosPhi;
    xyzPos.z = targetXYZ.z + cam->distance * sinPhi;

    // Convert position from XYZ to RTZ
    pos->r = std::sqrt(xyzPos.x * xyzPos.x + xyzPos.y * xyzPos.y);
    pos->theta = deg(std::atan2(xyzPos.y, xyzPos.x));
    pos->z = xyzPos.z;
}

double Camera::treevLookAt(FsNode* node, MorphType mtype, double panTimeOverride) {
    auto& me = MorphEngine::instance();
    CameraState& cam = state();

    // Construct desired camera state
    double newTargetR, newTargetTheta, newTargetZ;
    double newDistance, newNearClip, newFarClip;
    double newTheta, newPhi;

    bool isLeaf = !node->isDir();  // Simplified leaf check

    if (isLeaf) {
        // Leaf node
        FsNode* parent = node->parent;
        if (!parent) parent = node;

        double platformR0 = parent->treevGeom.platform.depth;  // Simplified
        double platformTheta = parent->treevGeom.platform.theta;

        newTargetR = platformR0 + node->treevGeom.leaf.distance;
        newTargetTheta = platformTheta + node->treevGeom.leaf.theta;
        newTargetZ = parent->treevGeom.platform.height +
                     (MAGIC_NUMBER - 1.0) * node->treevGeom.leaf.height;

        double topDist = 2.5 * fieldDistance(cam.fov, SQRT_2 * TREEV_LEAF_NODE_EDGE);
        newDistance = topDist + (2.0 - MAGIC_NUMBER) * node->treevGeom.leaf.height;

        newNearClip = NEAR_TO_DISTANCE_RATIO * topDist;
        newFarClip = FAR_TO_NEAR_RATIO * newNearClip;

        // Viewing angles
        double relTheta = newTargetTheta - platformTheta;
        double arcWidth = parent->treevGeom.platform.arc_width;
        if (std::abs(arcWidth) > EPSILON) {
            newTheta = -15.0 * relTheta / arcWidth;
        } else {
            newTheta = 0.0;
        }
        newPhi = 45.0;

        // Ensure camera is pitched high enough to see top and bottom
        double leafHeight = node->treevGeom.leaf.height;
        if (leafHeight > EPSILON) {
            double k = newDistance * std::sin(rad(0.25 * cam.fov)) /
                       ((2.0 - MAGIC_NUMBER) * leafHeight);
            if (k >= -1.0 && k <= 1.0) {
                double alpha = deg(std::asin(k)) - 0.25 * cam.fov;
                newPhi = std::max(newPhi, 90.0 - alpha);
            }
        }
    } else {
        // Directory node (platform)
        double platformR0 = 0.0;  // Simplified; real value from geometry
        double platformTheta = node->treevGeom.platform.theta;
        double platformDepth = node->treevGeom.platform.depth;

        newTargetR = platformR0 + 0.3 * platformDepth - 0.2 * TREEV_PLATFORM_SPACING_DEPTH;
        newTargetTheta = platformTheta;
        newTargetZ = node->treevGeom.platform.height;

        // Distance from target point
        double diameter = std::max(platformDepth + 0.5 * TREEV_PLATFORM_SPACING_DEPTH,
                                   0.25 * node->treevGeom.platform.height);
        newDistance = fieldDistance(cam.fov, diameter);

        newNearClip = NEAR_TO_DISTANCE_RATIO * newDistance;
        newFarClip = FAR_TO_NEAR_RATIO * newNearClip;

        // Viewing angles
        newTheta = -0.125 * (newTargetTheta - 90.0);
        newPhi = 30.0;
    }

    // Determine pan time
    double panTime;
    if (panTimeOverride > 0.0) {
        panTime = panTimeOverride;
    } else {
        RTZvec cameraPos, newCamPos;
        RTZvec currentTarget = treevState().target;
        treevGetCameraPosition(&cam, &currentTarget, &cameraPos);

        CameraState tempCam = cam;
        tempCam.theta = newTheta;
        tempCam.phi = newPhi;
        tempCam.distance = newDistance;
        RTZvec newTargetVec{newTargetR, newTargetTheta, newTargetZ};
        treevGetCameraPosition(&tempCam, &newTargetVec, &newCamPos);

        double dist = rtzDist(cameraPos, newCamPos);
        double k = dist / TREEV_AVG_VELOCITY;
        panTime = std::max(TREEV_MIN_PAN_TIME, std::min(k, TREEV_MAX_PAN_TIME));
    }

    // Get the camera moving
    me.morph(&cam.theta, mtype, newTheta, panTime);
    me.morph(&cam.phi, mtype, newPhi, panTime);
    me.morph(&cam.distance, mtype, newDistance, panTime);
    me.morph(&cam.nearClip, mtype, newNearClip, panTime);
    me.morph(&cam.farClip, mtype, newFarClip, panTime);
    me.morph(&treevState().target.r, mtype, newTargetR, panTime);
    me.morph(&treevState().target.theta, mtype, newTargetTheta, panTime);
    me.morph(&treevState().target.z, mtype, newTargetZ, panTime);

    return panTime;
}

// ============================================================================
// lookAt / lookAtFull / lookAtPrevious
// ============================================================================

void Camera::lookAt(FsNode* node) {
    lookAtFull(node, MorphType::Sigmoid, -1.0);
}

void Camera::lookAtFull(FsNode* node, MorphType mtype, double panTimeOverride) {
    if (!node) return;

    auto& me = MorphEngine::instance();

    // Leave bird's-eye view if active
    if (birdseyeActive_) {
        birdseyeActive_ = false;
    }

    // Halt any ongoing camera pan
    panBreak();

    double panTime = 0.0;

    switch (currentMode_) {
        case FSV_DISCV:
            panTime = discvLookAt(node, mtype, panTimeOverride);
            break;
        case FSV_MAPV:
            panTime = mapvLookAt(node, mtype, panTimeOverride);
            break;
        case FSV_TREEV:
            panTime = treevLookAt(node, mtype, panTimeOverride);
            break;
        default:
            return;
    }

    // Master morph: pan_part goes from 0 to 1 over the pan duration.
    // The step callback requests redraws, and the end callback clears
    // the moving flag.
    CameraState& cam = state();
    cam.panPart = 0.0;

    // Capture 'this' safely via the singleton
    me.morphFull(&cam.panPart, MorphType::Linear, 1.0, panTime,
        // Step callback
        [](Morph* /*m*/) {
            Animation::instance().requestRedraw();
        },
        // End callback
        [](Morph* /*m*/) {
            Animation::instance().requestRedraw();
            Camera::instance().moving_ = false;
        },
        node
    );

    // Update visited node history
    bool backtracking = false;
    if (!history_.empty()) {
        FsNode* prevNode = history_.front();
        if (prevNode == nullptr) {
            // Camera is backtracking
            history_.erase(history_.begin());
            backtracking = true;
        }
    }
    if (!backtracking && node != currentNode_ &&
        (history_.empty() || currentNode_ != history_.front())) {
        history_.insert(history_.begin(), currentNode_);
    }

    // New current node of interest
    currentNode_ = node;

    // Camera is under programmatic control now
    cam.manualControl = false;

    moving_ = true;
}

void Camera::lookAtPrevious() {
    // Can't backtrack if history is empty
    if (history_.empty()) return;

    // Get previously visited node
    FsNode* prevNode = history_.front();

    // Mark current history entry as null (backtracking marker)
    history_.front() = nullptr;
    lookAt(prevNode);
}

// ============================================================================
// TreeV L-pan
// ============================================================================

void Camera::treevLpanLookAt(FsNode* node, double panTimeOverride) {
    if (!node) return;

    auto& me = MorphEngine::instance();

    // Leave bird's-eye view if active
    if (birdseyeActive_) {
        birdseyeActive_ = false;
    }

    // Construct desired camera state (stage 1: angular alignment)
    double newTheta, newTargetR, newTargetTheta;
    bool isLeaf = !node->isDir();

    if (isLeaf && node->parent) {
        FsNode* parent = node->parent;
        double arcWidth = parent->treevGeom.platform.arc_width;
        double leafTheta = node->treevGeom.leaf.theta;
        if (std::abs(arcWidth) > EPSILON) {
            newTheta = -15.0 * leafTheta / arcWidth;
        } else {
            newTheta = 0.0;
        }
        newTargetR = node->treevGeom.leaf.distance;  // Simplified
        newTargetTheta = parent->treevGeom.platform.theta + leafTheta;
    } else {
        newTargetR = (2.0 - MAGIC_NUMBER) * node->treevGeom.platform.depth;
        newTargetTheta = node->treevGeom.platform.theta;
        newTheta = -0.125 * (newTargetTheta - 90.0);
    }

    // Duration of pan
    double panTime;
    if (panTimeOverride > 0.0) {
        panTime = panTimeOverride;
    } else {
        RTZvec cameraPos, newCamPos;
        RTZvec currentTarget = treevState().target;
        CameraState& cam = state();
        treevGetCameraPosition(&cam, &currentTarget, &cameraPos);

        CameraState tempCam = cam;
        tempCam.theta = newTheta;
        RTZvec newTargetVec{newTargetR, newTargetTheta, currentTarget.z};
        treevGetCameraPosition(&tempCam, &newTargetVec, &newCamPos);

        double dist = rtzDist(cameraPos, newCamPos);
        panTime = dist / TREEV_AVG_VELOCITY;
        panTime = std::max(TREEV_MIN_PAN_TIME, std::min(panTime, TREEV_MAX_PAN_TIME));
    }

    panBreak();

    CameraState& cam = state();

    // Stage 1: angular alignment
    me.morph(&cam.theta, MorphType::InvQuadratic, newTheta, panTime);
    me.morph(&treevState().target.r, MorphType::InvQuadratic, newTargetR, panTime);
    me.morph(&treevState().target.theta, MorphType::InvQuadratic, newTargetTheta, panTime);

    // Master morph for stage 1
    cam.panPart = 0.0;

    // Store panTime for use in stage 2
    double stage2PanTime = panTime;

    me.morphFull(&cam.panPart, MorphType::Linear, 1.0, panTime,
        // Step callback
        [](Morph* /*m*/) {
            Animation::instance().requestRedraw();
        },
        // End callback: schedule stage 2
        [node, stage2PanTime](Morph* /*m*/) {
            Animation::instance().requestRedraw();
            // Schedule stage 2 on the next frame
            Scheduler::instance().scheduleEvent(
                [](void* data) {
                    // Unpack: data points to a heap-allocated pair
                    auto* pair = static_cast<std::pair<FsNode*, double>*>(data);
                    Camera::instance().lookAtFull(pair->first, MorphType::Sigmoid, pair->second);
                    delete pair;
                },
                new std::pair<FsNode*, double>(node, stage2PanTime),
                1
            );
        },
        nullptr
    );

    cam.manualControl = false;
    moving_ = true;
}

// ============================================================================
// Camera controls
// ============================================================================

void Camera::dolly(double dk) {
    CameraState& cam = state();

    cam.distance += dk * cam.distance / 256.0;
    cam.distance = std::max(cam.distance, 16.0);
    cam.nearClip = NEAR_TO_DISTANCE_RATIO * cam.distance;
    cam.farClip = FAR_TO_NEAR_RATIO * cam.nearClip;

    cam.manualControl = true;
    Animation::instance().requestRedraw();
}

void Camera::revolve(double dtheta, double dphi) {
    CameraState& cam = state();

    cam.theta -= dtheta;
    cam.phi += dphi;

    // Keep angles within proper bounds
    while (cam.theta < 0.0)   cam.theta += 360.0;
    while (cam.theta > 360.0) cam.theta -= 360.0;
    cam.phi = std::max(1.0, std::min(cam.phi, 90.0));

    cam.manualControl = true;
    Animation::instance().requestRedraw();
}

void Camera::pan(double dx, double dy) {
    CameraState& cam = state();
    double scale = cam.distance / 800.0;

    switch (currentMode_) {
        case FSV_MAPV: {
            double sinTheta = std::sin(rad(cam.theta));
            double cosTheta = std::cos(rad(cam.theta));
            mapvState().target.x += (dx * sinTheta + dy * cosTheta) * scale;
            mapvState().target.y += (-dx * cosTheta + dy * sinTheta) * scale;
            break;
        }
        case FSV_DISCV: {
            discvState().target.x += dx * scale;
            discvState().target.y -= dy * scale;
            break;
        }
        case FSV_TREEV: {
            treevState().target.theta -= dx * 0.15;
            treevState().target.z += dy * scale;
            break;
        }
        default:
            break;
    }

    cam.manualControl = true;
    Animation::instance().requestRedraw();
}

void Camera::birdseyeView(bool goingUp) {
    auto& me = MorphEngine::instance();
    CameraState& cam = state();

    // Halt any ongoing camera pan
    panBreak();

    // Determine length of pan
    double panTime = 0.0;
    switch (currentMode_) {
        case FSV_DISCV: panTime = DISCV_MAX_PAN_TIME; break;
        case FSV_MAPV:  panTime = MAPV_MAX_PAN_TIME;  break;
        case FSV_TREEV: panTime = TREEV_MAX_PAN_TIME;  break;
        default: return;
    }

    if (goingUp) {
        // Save current camera state
        std::memcpy(&preBirdseyeCamera_, &currentCamera_, sizeof(AnyCameraState));

        // Build bird's-eye view
        double newPhi = 90.0;
        double newDistance = cam.distance;
        double newTheta = cam.theta;

        FsNode* rootDir = FsTree::instance().rootDir();

        switch (currentMode_) {
            case FSV_DISCV: {
                double rootRadius = 1000.0;
                if (rootDir) {
                    rootRadius = rootDir->discvGeom.radius;
                    if (rootRadius < EPSILON) rootRadius = 1000.0;
                }
                newDistance = 2.0 * fieldDistance(cam.fov, 2.0 * rootRadius);
                break;
            }
            case FSV_MAPV: {
                double rootWidth = 1000.0;
                if (rootDir) {
                    rootWidth = rootDir->mapvWidth();
                    if (rootWidth < EPSILON) rootWidth = 1000.0;
                }
                newTheta = 270.0;
                newDistance = fieldDistance(cam.fov, rootWidth);
                break;
            }
            case FSV_TREEV: {
                newTheta = 90.0 - treevState().target.theta;
                newDistance = 4.0 * cam.distance;  // Simplified
                break;
            }
            default:
                break;
        }

        double newNearClip = NEAR_TO_DISTANCE_RATIO * newDistance;
        double newFarClip = FAR_TO_NEAR_RATIO * newNearClip;

        me.morph(&cam.theta, MorphType::SigmoidAccel, newTheta, panTime);
        me.morph(&cam.phi, MorphType::SigmoidAccel, newPhi, panTime);
        me.morph(&cam.distance, MorphType::SigmoidAccel, newDistance, panTime);
        me.morph(&cam.nearClip, MorphType::SigmoidAccel, newNearClip, panTime);
        me.morph(&cam.farClip, MorphType::SigmoidAccel, newFarClip, panTime);

        birdseyeActive_ = true;
    } else {
        // Restore pre-bird's-eye-view camera state
        CameraState* preCam = reinterpret_cast<CameraState*>(&preBirdseyeCamera_);

        me.morph(&cam.theta, MorphType::Sigmoid, preCam->theta, panTime);
        me.morph(&cam.phi, MorphType::Sigmoid, preCam->phi, panTime);
        me.morph(&cam.distance, MorphType::Sigmoid, preCam->distance, panTime);
        me.morph(&cam.nearClip, MorphType::Sigmoid, preCam->nearClip, panTime);
        me.morph(&cam.farClip, MorphType::Sigmoid, preCam->farClip, panTime);

        switch (currentMode_) {
            case FSV_DISCV: {
                DiscVCameraState* pre = &preBirdseyeCamera_.discv;
                me.morph(&discvState().target.x, MorphType::Sigmoid, pre->target.x, panTime);
                me.morph(&discvState().target.y, MorphType::Sigmoid, pre->target.y, panTime);
                break;
            }
            case FSV_MAPV: {
                MapVCameraState* pre = &preBirdseyeCamera_.mapv;
                me.morph(&mapvState().target.x, MorphType::Sigmoid, pre->target.x, panTime);
                me.morph(&mapvState().target.y, MorphType::Sigmoid, pre->target.y, panTime);
                me.morph(&mapvState().target.z, MorphType::Sigmoid, pre->target.z, panTime);
                break;
            }
            case FSV_TREEV: {
                TreeVCameraState* pre = &preBirdseyeCamera_.treev;
                me.morph(&treevState().target.r, MorphType::Sigmoid, pre->target.r, panTime);
                me.morph(&treevState().target.theta, MorphType::Sigmoid, pre->target.theta, panTime);
                me.morph(&treevState().target.z, MorphType::Sigmoid, pre->target.z, panTime);
                break;
            }
            default:
                break;
        }

        birdseyeActive_ = false;
    }

    // Master morph
    cam.panPart = 0.0;
    me.morphFull(&cam.panPart, MorphType::Linear, 1.0, panTime,
        [](Morph* /*m*/) {
            Animation::instance().requestRedraw();
        },
        [](Morph* /*m*/) {
            Animation::instance().requestRedraw();
            Camera::instance().moving_ = false;
        },
        nullptr
    );

    moving_ = true;
}

// ============================================================================
// View/Projection matrices
// ============================================================================

glm::mat4 Camera::getViewMatrix() const {
    const CameraState& cam = state();
    glm::dvec3 eye, target, up;
    up = glm::dvec3(0.0, 0.0, 1.0);

    switch (currentMode_) {
        case FSV_DISCV: {
            // DiscV: orthographic-like top-down view
            const DiscVCameraState& dcam = discvState();
            target = glm::dvec3(dcam.target.x, dcam.target.y, 0.0);
            eye = glm::dvec3(dcam.target.x, dcam.target.y, cam.distance);
            break;
        }

        case FSV_MAPV: {
            // MapV: target is XYZ, camera orbits using theta/phi/distance spherical coords
            const MapVCameraState& mcam = mapvState();
            target = glm::dvec3(mcam.target.x, mcam.target.y, mcam.target.z);

            double sinTheta = std::sin(rad(cam.theta));
            double cosTheta = std::cos(rad(cam.theta));
            double sinPhi = std::sin(rad(cam.phi));
            double cosPhi = std::cos(rad(cam.phi));

            eye = glm::dvec3(
                mcam.target.x + cam.distance * cosTheta * cosPhi,
                mcam.target.y + cam.distance * sinTheta * cosPhi,
                mcam.target.z + cam.distance * sinPhi
            );
            break;
        }

        case FSV_TREEV: {
            // TreeV: target is RTZ (cylindrical), convert to XYZ, then orbit
            const TreeVCameraState& tcam = treevState();

            // Convert target from cylindrical to Cartesian
            double targetTheta = tcam.target.theta;
            target = glm::dvec3(
                tcam.target.r * std::cos(rad(targetTheta)),
                tcam.target.r * std::sin(rad(targetTheta)),
                tcam.target.z
            );

            // Absolute camera heading
            double absTheta = targetTheta + cam.theta - 180.0;
            double sinTheta = std::sin(rad(absTheta));
            double cosTheta = std::cos(rad(absTheta));
            double sinPhi = std::sin(rad(cam.phi));
            double cosPhi = std::cos(rad(cam.phi));

            eye = glm::dvec3(
                target.x + cam.distance * cosTheta * cosPhi,
                target.y + cam.distance * sinTheta * cosPhi,
                target.z + cam.distance * sinPhi
            );
            break;
        }

        default: {
            // Fallback: look along -Z
            target = glm::dvec3(0.0, 0.0, 0.0);
            eye = glm::dvec3(0.0, 0.0, cam.distance);
            break;
        }
    }

    return glm::mat4(glm::lookAt(eye, target, up));
}

glm::mat4 Camera::getProjectionMatrix(float aspectRatio) const {
    const CameraState& cam = state();
    float fovRadians = static_cast<float>(rad(cam.fov));
    float nearClip = static_cast<float>(cam.nearClip);
    float farClip = static_cast<float>(cam.farClip);

    if (nearClip < 0.01f) nearClip = 0.01f;
    if (farClip <= nearClip) farClip = nearClip + 1.0f;
    if (aspectRatio < 0.01f) aspectRatio = 1.0f;

    return glm::perspective(fovRadians, aspectRatio, nearClip, farClip);
}

} // namespace fsvng
