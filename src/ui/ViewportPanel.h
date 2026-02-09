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

    // MapV text label overlay
    void drawMapVLabels(const glm::mat4& viewProj, ImVec2 imgPos, ImVec2 imgSize);
    void drawMapVLabelsRecursive(FsNode* dnode, const glm::mat4& viewProj,
                                  ImVec2 imgPos, ImVec2 imgSize,
                                  ImDrawList* drawList, double zBase, int depth);

    GLuint fbo_ = 0;
    GLuint colorTex_ = 0;
    GLuint depthRbo_ = 0;
    int width_ = 800;
    int height_ = 600;

    bool dragging_ = false;
    bool rightDragging_ = false;
    float lastMouseX_ = 0.0f;
    float lastMouseY_ = 0.0f;
};

} // namespace fsvng
