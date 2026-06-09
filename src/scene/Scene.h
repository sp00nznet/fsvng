#pragma once

#include "camera/Camera.h"
#include "core/FsTree.h"
#include "geometry/GeometryManager.h"
#include "ui/DirTreePanel.h"
#include "ui/FileListPanel.h"
#include "ui/PreviewPanel.h"
#include "ui/PulseEffect.h"
#include "ui/ViewportPanel.h"
#include "ui/MainWindow.h"

namespace fsvng {

// ============================================================================
// Scene - one pane's worth of state.
//
// Bundles the per-scene state blocks extracted from each singleton. The blocks
// are heap-stable (a Scene is never moved/copied), so morph double* pointers,
// FsNode* references, and the scan thread that capture into these blocks stay
// valid for the Scene's lifetime. SceneManager::activate() points each
// singleton at this Scene's blocks.
// ============================================================================
struct Scene {
    Camera::SceneState          camera;
    FsTree::SceneState          tree;
    GeometryManager::SceneState geom;
    DirTreePanel::SceneState    dirTree;
    FileListPanel::SceneState   fileList;
    PreviewPanel::SceneState    preview;
    PulseEffect::SceneState     pulse;
    MainWindow::SceneState      main;

    // Each pane owns its viewport (FBO + hit-test + input state).
    ViewportPanel viewport;

    // Root path this pane was opened on (for labels / re-scan).
    std::string rootPath;

    Scene() = default;
    ~Scene();

    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;
};

} // namespace fsvng
