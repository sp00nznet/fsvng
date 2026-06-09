#include "scene/SceneManager.h"
#include "scene/Scene.h"

#include "core/FsScanner.h"

namespace fsvng {

// ----------------------------------------------------------------------------
// Scene destructor: tear down per-scene threads / GL resources. Runs while the
// GL context is still alive because SceneManager::shutdown() frees scenes
// explicitly before context teardown.
// ----------------------------------------------------------------------------
Scene::~Scene() {
    // Stop the live-watch thread.
    main.watcher_.stop();
    // Join any in-flight scan thread.
    if (main.scanThread_.joinable()) {
        if (main.activeScanner_) {
            main.activeScanner_->cancelRequested.store(true);
        }
        main.scanThread_.join();
    }
    // Free the preview's GL texture.
    PreviewPanel::releaseState(preview);
}

SceneManager& SceneManager::instance() {
    static SceneManager s;
    return s;
}

void SceneManager::init() {
    if (scenes_.empty()) {
        addScene();
    }
    if (!active_ && !scenes_.empty()) {
        activate(scenes_.front().get());
    }
}

Scene* SceneManager::addScene() {
    if (sceneCount() >= kMaxScenes) {
        return nullptr;
    }
    scenes_.push_back(std::make_unique<Scene>());
    Scene* sc = scenes_.back().get();
    if (!active_) {
        activate(sc);
    }
    if (!focused_) {
        focused_ = sc;
    }
    return sc;
}

void SceneManager::activate(Scene* sc) {
    if (!sc) return;
    Camera::instance().setSceneState(&sc->camera);
    FsTree::instance().setSceneState(&sc->tree);
    GeometryManager::instance().setSceneState(&sc->geom);
    DirTreePanel::instance().setSceneState(&sc->dirTree);
    FileListPanel::instance().setSceneState(&sc->fileList);
    PreviewPanel::instance().setSceneState(&sc->preview);
    MainWindow::instance().setSceneState(&sc->main);
    active_ = sc;
}

int SceneManager::indexOf(Scene* sc) const {
    for (int i = 0; i < sceneCount(); ++i) {
        if (scenes_[i].get() == sc) return i;
    }
    return -1;
}

void SceneManager::resetSingletonsToDefault() {
    Camera::instance().setSceneState(nullptr);
    FsTree::instance().setSceneState(nullptr);
    GeometryManager::instance().setSceneState(nullptr);
    DirTreePanel::instance().setSceneState(nullptr);
    FileListPanel::instance().setSceneState(nullptr);
    PreviewPanel::instance().setSceneState(nullptr);
    MainWindow::instance().setSceneState(nullptr);
}

void SceneManager::shutdown() {
    // Detach singletons from the scenes we're about to destroy so nothing
    // dereferences a freed block during later static destruction.
    resetSingletonsToDefault();
    active_ = nullptr;
    focused_ = nullptr;
    scenes_.clear();  // runs Scene::~Scene (joins threads, frees textures)
}

} // namespace fsvng
