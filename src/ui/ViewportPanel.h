#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>
#include <imgui.h>

#include "renderer/ShaderProgram.h"

namespace fsvng {

class FsNode;

class ViewportPanel {
public:
    static ViewportPanel& instance();

    void draw();

    int getWidth() const { return width_; }
    int getHeight() const { return height_; }

private:
    ViewportPanel() = default;
    void createFBO(int width, int height);
    void handleInput();
    void handleKeyboard();

    // MapV text label overlay
    void drawMapVLabels(const glm::mat4& viewProj, ImVec2 imgPos, ImVec2 imgSize);
    void drawMapVLabelsRecursive(FsNode* dnode, const glm::mat4& viewProj,
                                  ImVec2 imgPos, ImVec2 imgSize,
                                  ImDrawList* drawList, double zBase, int depth);

    // TreeV text label overlay
    void drawTreeVLabels(const glm::mat4& viewProj, ImVec2 imgPos, ImVec2 imgSize);
    void drawTreeVLabelsRecursive(FsNode* dnode, const glm::mat4& viewProj,
                                   ImVec2 imgPos, ImVec2 imgSize,
                                   ImDrawList* drawList, int depth);

    // Viewport hit testing
    FsNode* hitTestMapV(float screenX, float screenY);
    FsNode* hitTestMapVRecursive(FsNode* dnode, float screenX, float screenY,
                                  double zBase, int depth);

    GLuint fbo_ = 0;
    GLuint colorTex_ = 0;
    GLuint depthRbo_ = 0;
    int width_ = 800;
    int height_ = 600;

    bool dragging_ = false;

    // Right-click context menu state
    bool rightMouseDown_ = false;
    ImVec2 rightClickPos_{0, 0};
    float rightClickDragDist_ = 0.0f;
    bool openContextMenu_ = false;
    FsNode* contextMenuNode_ = nullptr;

    // Cached rendering state for hit testing
    glm::mat4 cachedViewProj_{1.0f};
    ImVec2 imgPos_{0, 0};
    ImVec2 imgSize_{0, 0};
    bool hasScene_ = false;
};

} // namespace fsvng
