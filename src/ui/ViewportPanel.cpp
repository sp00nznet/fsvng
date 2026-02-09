#include "ui/ViewportPanel.h"

#include <imgui.h>
#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

#include "ui/MainWindow.h"
#include "renderer/Renderer.h"
#include "geometry/GeometryManager.h"
#include "camera/Camera.h"
#include "core/FsTree.h"
#include "core/FsNode.h"

namespace fsvng {

ViewportPanel& ViewportPanel::instance() {
    static ViewportPanel s;
    return s;
}

void ViewportPanel::createFBO(int width, int height) {
    // Delete existing resources if resizing
    if (fbo_ != 0) {
        glDeleteFramebuffers(1, &fbo_);
        glDeleteTextures(1, &colorTex_);
        glDeleteRenderbuffers(1, &depthRbo_);
        fbo_ = 0;
        colorTex_ = 0;
        depthRbo_ = 0;
    }

    width_ = width;
    height_ = height;

    if (width <= 0 || height <= 0) return;

    // Create color texture
    glGenTextures(1, &colorTex_);
    glBindTexture(GL_TEXTURE_2D, colorTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Create depth renderbuffer
    glGenRenderbuffers(1, &depthRbo_);
    glBindRenderbuffer(GL_RENDERBUFFER, depthRbo_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    // Create framebuffer
    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex_, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthRbo_);

    // Check completeness
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        // Framebuffer incomplete - will show a blank viewport
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ViewportPanel::handleInput() {
    if (!ImGui::IsWindowHovered()) return;

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 mousePos = ImGui::GetMousePos();

    // Left mouse drag - camera revolve
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        dragging_ = true;
        lastMouseX_ = mousePos.x;
        lastMouseY_ = mousePos.y;
    }
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        dragging_ = false;
    }
    if (dragging_ && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        float dx = mousePos.x - lastMouseX_;
        float dy = mousePos.y - lastMouseY_;
        lastMouseX_ = mousePos.x;
        lastMouseY_ = mousePos.y;

        Camera::instance().revolve(dx * 0.3, dy * 0.3);
    }

    // Right mouse drag - camera pan
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        rightDragging_ = true;
        lastMouseX_ = mousePos.x;
        lastMouseY_ = mousePos.y;
    }
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
        rightDragging_ = false;
    }
    if (rightDragging_ && ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
        float dx = mousePos.x - lastMouseX_;
        float dy = mousePos.y - lastMouseY_;
        lastMouseX_ = mousePos.x;
        lastMouseY_ = mousePos.y;

        Camera::instance().pan(dx, dy);
    }

    // Scroll wheel - camera dolly
    float wheel = io.MouseWheel;
    if (wheel != 0.0f) {
        Camera::instance().dolly(-wheel * 64.0);
    }
}

// Helper: compute absolute z base for a MapV node (sum of all ancestor heights)
static double mapvNodeZBase(FsNode* node) {
    double z = 0.0;
    FsNode* p = node->parent;
    while (p) {
        z += p->mapvGeom.height;
        p = p->parent;
    }
    return z;
}

// Project 3D world position to screen coordinates within the viewport image
static bool projectToScreen(const glm::mat4& viewProj, const glm::vec3& worldPos,
                            ImVec2 imgPos, ImVec2 imgSize, float& sx, float& sy) {
    glm::vec4 clip = viewProj * glm::vec4(worldPos, 1.0f);
    if (clip.w <= 0.0f) return false;

    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    if (ndc.x < -1.2f || ndc.x > 1.2f || ndc.y < -1.2f || ndc.y > 1.2f) return false;

    sx = imgPos.x + (ndc.x * 0.5f + 0.5f) * imgSize.x;
    sy = imgPos.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * imgSize.y;
    return true;
}

void ViewportPanel::drawMapVLabels(const glm::mat4& viewProj, ImVec2 imgPos, ImVec2 imgSize) {
    FsNode* rootDir = FsTree::instance().rootDir();
    if (!rootDir) return;

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    drawMapVLabelsRecursive(rootDir, viewProj, imgPos, imgSize, drawList, 0.0, 0);
}

void ViewportPanel::drawMapVLabelsRecursive(FsNode* dnode, const glm::mat4& viewProj,
                                             ImVec2 imgPos, ImVec2 imgSize,
                                             ImDrawList* drawList, double zBase, int depth) {
    if (depth > 4) return; // limit recursion depth to avoid clutter

    double childZBase = zBase + dnode->mapvGeom.height;

    for (auto& childPtr : dnode->children) {
        FsNode* node = childPtr.get();

        // World position: center of the top face
        glm::vec3 worldPos(
            static_cast<float>(node->mapvCenterX()),
            static_cast<float>(node->mapvCenterY()),
            static_cast<float>(childZBase + node->mapvGeom.height)
        );

        // Project center
        float cx, cy;
        if (!projectToScreen(viewProj, worldPos, imgPos, imgSize, cx, cy))
            continue;

        // Project two corners of the top face to estimate screen size
        glm::vec3 c0World(
            static_cast<float>(node->mapvGeom.c0.x),
            static_cast<float>(node->mapvGeom.c0.y),
            static_cast<float>(childZBase + node->mapvGeom.height)
        );
        glm::vec3 c1World(
            static_cast<float>(node->mapvGeom.c1.x),
            static_cast<float>(node->mapvGeom.c1.y),
            static_cast<float>(childZBase + node->mapvGeom.height)
        );

        float sx0, sy0, sx1, sy1;
        if (!projectToScreen(viewProj, c0World, imgPos, imgSize, sx0, sy0)) continue;
        if (!projectToScreen(viewProj, c1World, imgPos, imgSize, sx1, sy1)) continue;

        float screenW = std::abs(sx1 - sx0);
        float screenH = std::abs(sy1 - sy0);
        float screenSize = std::max(screenW, screenH);

        // Only draw label if node projection is large enough
        if (screenSize < 30.0f) continue;

        const char* name = node->name.c_str();
        ImVec2 textSize = ImGui::CalcTextSize(name);

        // Truncate text if wider than projected box
        float maxTextWidth = screenW * 0.9f;
        if (textSize.x > maxTextWidth && maxTextWidth > 20.0f) {
            // Draw with clipping
            float textX = cx - maxTextWidth * 0.5f;
            float textY = cy - textSize.y * 0.5f;
            drawList->PushClipRect(
                ImVec2(textX, textY),
                ImVec2(textX + maxTextWidth, textY + textSize.y),
                true
            );
            drawList->AddText(ImVec2(textX, textY), IM_COL32(255, 255, 255, 220), name);
            drawList->PopClipRect();
        } else if (textSize.x <= maxTextWidth || screenSize > 60.0f) {
            float textX = cx - textSize.x * 0.5f;
            float textY = cy - textSize.y * 0.5f;

            // Draw shadow for readability
            drawList->AddText(ImVec2(textX + 1.0f, textY + 1.0f), IM_COL32(0, 0, 0, 180), name);
            drawList->AddText(ImVec2(textX, textY), IM_COL32(255, 255, 255, 220), name);
        }

        // Recurse into expanded directories
        if (node->isDir() && !node->isCollapsed()) {
            drawMapVLabelsRecursive(node, viewProj, imgPos, imgSize, drawList,
                                     childZBase, depth + 1);
        }
    }
}

void ViewportPanel::draw() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("Viewport");
    ImGui::PopStyleVar();

    // Get available content region size
    ImVec2 availSize = ImGui::GetContentRegionAvail();
    int newWidth = static_cast<int>(availSize.x);
    int newHeight = static_cast<int>(availSize.y);

    // Ensure minimum size
    if (newWidth < 1) newWidth = 1;
    if (newHeight < 1) newHeight = 1;

    // Create or resize FBO if needed
    if (fbo_ == 0 || newWidth != width_ || newHeight != height_) {
        createFBO(newWidth, newHeight);
    }

    // Store view/projection for label overlay
    glm::mat4 cachedView(1.0f);
    glm::mat4 cachedProj(1.0f);
    bool hasScene = false;

    if (fbo_ != 0) {
        // Bind FBO and render the 3D scene into it
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glViewport(0, 0, width_, height_);
        glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Enable depth test for 3D rendering, disable culling for correct MapV display
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDisable(GL_CULL_FACE);

        // Render the scene if we have a tree loaded
        if (FsTree::instance().rootDir()) {
            hasScene = true;
            Camera& cam = Camera::instance();
            float aspect = (height_ > 0) ? static_cast<float>(width_) / static_cast<float>(height_) : 1.0f;
            cachedView = cam.getViewMatrix();
            cachedProj = cam.getProjectionMatrix(aspect);

            // Set up node shader common uniforms (lighting)
            ShaderProgram& nodeShader = Renderer::instance().getNodeShader();
            nodeShader.use();
            nodeShader.setMat4("uView", cachedView);
            nodeShader.setMat4("uProjection", cachedProj);
            nodeShader.setVec3("uLightPos", glm::vec3(0.0f, 10000.0f, 10000.0f));
            nodeShader.setVec3("uAmbient", glm::vec3(0.2f, 0.2f, 0.2f));
            nodeShader.setVec3("uDiffuse", glm::vec3(0.5f, 0.5f, 0.5f));
            nodeShader.setVec3("uViewPos", glm::vec3(0.0f, 500.0f, 1000.0f));
            nodeShader.setFloat("uHighlight", 0.0f);
            nodeShader.unuse();

            // Draw geometry
            GeometryManager::instance().draw(cachedView, cachedProj, true);
        }

        // Disable 3D state before returning to ImGui
        glDisable(GL_DEPTH_TEST);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Record image position before drawing it
        ImVec2 imgPos = ImGui::GetCursorScreenPos();

        // Display the FBO color texture as an ImGui image
        ImTextureID texId = (ImTextureID)(uintptr_t)(colorTex_);
        // Flip UV vertically because OpenGL textures are bottom-up
        ImGui::Image(texId, availSize, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));

        // Draw text labels overlay for MapV mode
        if (hasScene && MainWindow::instance().getMode() == FSV_MAPV) {
            glm::mat4 viewProj = cachedProj * cachedView;
            drawMapVLabels(viewProj, imgPos, availSize);
        }
    }

    // Handle mouse input for camera control
    handleInput();

    ImGui::End();
}

} // namespace fsvng
