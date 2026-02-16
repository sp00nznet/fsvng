#include "ui/ViewportPanel.h"

#include <imgui.h>
#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

#include "ui/MainWindow.h"
#include "ui/Dialogs.h"
#include "ui/ThemeManager.h"
#include "ui/PulseEffect.h"
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

    // Double-click: navigate to node under cursor
    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && hasScene_) {
        FsNode* hit = nullptr;
        FsvMode mode = MainWindow::instance().getMode();
        if (mode == FSV_MAPV) {
            hit = hitTestMapV(mousePos.x, mousePos.y);
        }
        if (hit) {
            MainWindow::instance().navigateTo(hit);
            dragging_ = false;
            return;
        }
    }

    // Left mouse drag - camera revolve (uses ImGui delta to avoid double-click interference)
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) &&
        !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        float dx = io.MouseDelta.x;
        float dy = io.MouseDelta.y;
        if (dx != 0.0f || dy != 0.0f) {
            Camera::instance().revolve(dx * 0.3, dy * 0.3);
        }
    }

    // Right mouse button - track click vs drag for context menu
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        rightMouseDown_ = true;
        rightClickPos_ = mousePos;
        rightClickDragDist_ = 0.0f;
    }
    if (rightMouseDown_) {
        float dx = mousePos.x - rightClickPos_.x;
        float dy = mousePos.y - rightClickPos_.y;
        rightClickDragDist_ = std::max(rightClickDragDist_,
                                        std::sqrt(dx * dx + dy * dy));
    }

    // Right mouse drag - camera pan (only after sufficient drag distance)
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Right) && rightClickDragDist_ > 4.0f) {
        float dx = io.MouseDelta.x;
        float dy = io.MouseDelta.y;
        if (dx != 0.0f || dy != 0.0f) {
            Camera::instance().pan(dx, dy);
        }
    }

    // Right mouse release with minimal drag - show context menu
    if (rightMouseDown_ && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
        rightMouseDown_ = false;
        if (rightClickDragDist_ <= 4.0f && hasScene_) {
            // Try to find node under cursor
            FsNode* hit = nullptr;
            FsvMode mode = MainWindow::instance().getMode();
            if (mode == FSV_MAPV) {
                hit = hitTestMapV(rightClickPos_.x, rightClickPos_.y);
            }
            // Fall back to currently selected node
            if (!hit) {
                hit = MainWindow::instance().getCurrentNode();
            }
            if (hit) {
                contextMenuNode_ = hit;
                openContextMenu_ = true;
            }
        }
    }

    // Scroll wheel - camera dolly
    float wheel = io.MouseWheel;
    if (wheel != 0.0f) {
        Camera::instance().dolly(-wheel * 64.0);
    }
}

void ViewportPanel::handleKeyboard() {
    if (!ImGui::IsWindowFocused()) return;

    // WASD / Arrow keys for doom-style camera movement (continuous while held)
    float dollySpeed = 16.0f;
    float panSpeed = 1.5f;

    if (ImGui::IsKeyDown(ImGuiKey_W) || ImGui::IsKeyDown(ImGuiKey_UpArrow)) {
        Camera::instance().dolly(-dollySpeed);  // Forward = dolly in
    }
    if (ImGui::IsKeyDown(ImGuiKey_S) || ImGui::IsKeyDown(ImGuiKey_DownArrow)) {
        Camera::instance().dolly(dollySpeed);   // Backward = dolly out
    }
    if (ImGui::IsKeyDown(ImGuiKey_A) || ImGui::IsKeyDown(ImGuiKey_LeftArrow)) {
        Camera::instance().pan(-panSpeed, 0.0);  // Strafe left
    }
    if (ImGui::IsKeyDown(ImGuiKey_D) || ImGui::IsKeyDown(ImGuiKey_RightArrow)) {
        Camera::instance().pan(panSpeed, 0.0);   // Strafe right
    }

    // Q/E for vertical movement
    if (ImGui::IsKeyDown(ImGuiKey_Q)) {
        Camera::instance().pan(0.0, -panSpeed);  // Move up
    }
    if (ImGui::IsKeyDown(ImGuiKey_E)) {
        Camera::instance().pan(0.0, panSpeed);   // Move down
    }

    // Enter: toggle expand/collapse current directory
    if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
        MainWindow::instance().toggleExpandCurrent();
    }

    // Backspace: navigate back in history
    if (ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
        MainWindow::instance().navigateBack();
    }

    // Escape: navigate up to parent directory
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        MainWindow::instance().navigateUp();
    }
}

// ============================================================================
// MapV hit testing
// ============================================================================

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

FsNode* ViewportPanel::hitTestMapV(float screenX, float screenY) {
    FsNode* rootDir = FsTree::instance().rootDir();
    if (!rootDir) return nullptr;

    return hitTestMapVRecursive(rootDir, screenX, screenY, 0.0, 0);
}

FsNode* ViewportPanel::hitTestMapVRecursive(FsNode* dnode, float mx, float my,
                                             double zBase, int depth) {
    if (depth > 10) return nullptr;

    double childZBase = zBase + dnode->mapvGeom.height;
    FsNode* bestHit = nullptr;

    for (auto& childPtr : dnode->children) {
        FsNode* node = childPtr.get();

        float z = static_cast<float>(childZBase + node->mapvGeom.height);

        // Project top face corners to screen space
        glm::vec3 c0World(static_cast<float>(node->mapvGeom.c0.x),
                          static_cast<float>(node->mapvGeom.c0.y), z);
        glm::vec3 c1World(static_cast<float>(node->mapvGeom.c1.x),
                          static_cast<float>(node->mapvGeom.c1.y), z);

        float sx0, sy0, sx1, sy1;
        bool c0ok = projectToScreen(cachedViewProj_, c0World, imgPos_, imgSize_, sx0, sy0);
        bool c1ok = projectToScreen(cachedViewProj_, c1World, imgPos_, imgSize_, sx1, sy1);

        if (c0ok && c1ok) {
            float minX = std::min(sx0, sx1);
            float maxX = std::max(sx0, sx1);
            float minY = std::min(sy0, sy1);
            float maxY = std::max(sy0, sy1);

            if (mx >= minX && mx <= maxX && my >= minY && my <= maxY) {
                bestHit = node;
            }
        }

        // Always recurse into expanded directories for deeper hits,
        // regardless of whether projection succeeded or mouse was inside bounds.
        // When zoomed deep, parent nodes may project outside viewport.
        if (node->isDir() && !node->isCollapsed()) {
            FsNode* deeper = hitTestMapVRecursive(node, mx, my,
                                                   childZBase, depth + 1);
            if (deeper) bestHit = deeper;
        }
    }

    return bestHit;
}

// ============================================================================
// MapV text label overlay
// ============================================================================

void ViewportPanel::drawMapVLabels(const glm::mat4& viewProj, ImVec2 imgPos, ImVec2 imgSize) {
    FsNode* rootDir = FsTree::instance().rootDir();
    if (!rootDir) return;

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    drawMapVLabelsRecursive(rootDir, viewProj, imgPos, imgSize, drawList, 0.0, 0);
}

void ViewportPanel::drawMapVLabelsRecursive(FsNode* dnode, const glm::mat4& viewProj,
                                             ImVec2 imgPos, ImVec2 imgSize,
                                             ImDrawList* drawList, double zBase, int depth) {
    if (depth > 20) return;

    double childZBase = zBase + dnode->mapvGeom.height;

    for (auto& childPtr : dnode->children) {
        FsNode* node = childPtr.get();
        bool isExpandedDir = node->isDir() && !node->isCollapsed();

        // World position: center of the top face
        glm::vec3 worldPos(
            static_cast<float>(node->mapvCenterX()),
            static_cast<float>(node->mapvCenterY()),
            static_cast<float>(childZBase + node->mapvGeom.height)
        );

        // Project center
        float cx, cy;
        bool centerVisible = projectToScreen(viewProj, worldPos, imgPos, imgSize, cx, cy);

        if (centerVisible) {
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
            bool c0ok = projectToScreen(viewProj, c0World, imgPos, imgSize, sx0, sy0);
            bool c1ok = projectToScreen(viewProj, c1World, imgPos, imgSize, sx1, sy1);

            if (c0ok && c1ok) {
                float screenW = std::abs(sx1 - sx0);
                float screenH = std::abs(sy1 - sy0);
                float screenSize = std::max(screenW, screenH);

                // Only draw label if node projection is large enough
                if (screenSize >= 30.0f) {
                    // For expanded directories, skip the parent label when children are visible
                    if (!isExpandedDir || screenSize < 150.0f) {
                        const char* name = node->name.c_str();
                        ImVec2 textSize = ImGui::CalcTextSize(name);

                        float maxTextWidth = screenW * 0.9f;
                        if (textSize.x > maxTextWidth && maxTextWidth > 20.0f) {
                            float textX = cx - maxTextWidth * 0.5f;
                            float textY = cy - textSize.y * 0.5f;
                            drawList->PushClipRect(
                                ImVec2(textX, textY),
                                ImVec2(textX + maxTextWidth, textY + textSize.y),
                                true
                            );
                            drawList->AddText(ImVec2(textX, textY), ThemeManager::instance().currentTheme().labelColor, name);
                            drawList->PopClipRect();
                        } else if (textSize.x <= maxTextWidth || screenSize > 60.0f) {
                            float textX = cx - textSize.x * 0.5f;
                            float textY = cy - textSize.y * 0.5f;
                            drawList->AddText(ImVec2(textX + 1.0f, textY + 1.0f), ThemeManager::instance().currentTheme().labelShadow, name);
                            drawList->AddText(ImVec2(textX, textY), ThemeManager::instance().currentTheme().labelColor, name);
                        }
                    }
                }
            }
        }

        // Always recurse into expanded directories regardless of projection.
        // When zoomed deep, intermediate dirs project outside viewport but
        // their children are visible and need labels.
        if (isExpandedDir) {
            drawMapVLabelsRecursive(node, viewProj, imgPos, imgSize, drawList,
                                     childZBase, depth + 1);
        }
    }
}

// ============================================================================
// TreeV text label overlay
// ============================================================================

void ViewportPanel::drawTreeVLabels(const glm::mat4& viewProj, ImVec2 imgPos, ImVec2 imgSize) {
    FsNode* rootDir = FsTree::instance().rootDir();
    if (!rootDir) return;

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    drawTreeVLabelsRecursive(rootDir, viewProj, imgPos, imgSize, drawList, 0);
}

// Helper: draw a label at a given screen position if the node projects large enough
static void drawTreeVLabel(ImDrawList* drawList, const glm::mat4& viewProj,
                           ImVec2 imgPos, ImVec2 imgSize,
                           const glm::vec3& worldPos, float edgeOffset,
                           const char* name) {
    float cx, cy;
    if (!projectToScreen(viewProj, worldPos, imgPos, imgSize, cx, cy)) return;

    glm::vec3 edgePos(worldPos.x + edgeOffset, worldPos.y, worldPos.z);
    float ex, ey;
    if (!projectToScreen(viewProj, edgePos, imgPos, imgSize, ex, ey)) return;

    float screenSize = std::abs(ex - cx) * 2.0f;
    if (screenSize < 20.0f) return;

    ImVec2 textSize = ImGui::CalcTextSize(name);
    const Theme& theme = ThemeManager::instance().currentTheme();

    float maxTextWidth = screenSize * 0.9f;
    if (textSize.x > maxTextWidth && maxTextWidth > 20.0f) {
        float textX = cx - maxTextWidth * 0.5f;
        float textY = cy - textSize.y * 0.5f;
        drawList->PushClipRect(
            ImVec2(textX, textY),
            ImVec2(textX + maxTextWidth, textY + textSize.y),
            true
        );
        drawList->AddText(ImVec2(textX, textY), theme.labelColor, name);
        drawList->PopClipRect();
    } else if (textSize.x <= maxTextWidth || screenSize > 40.0f) {
        float textX = cx - textSize.x * 0.5f;
        float textY = cy - textSize.y * 0.5f;
        drawList->AddText(ImVec2(textX + 1.0f, textY + 1.0f), theme.labelShadow, name);
        drawList->AddText(ImVec2(textX, textY), theme.labelColor, name);
    }
}

void ViewportPanel::drawTreeVLabelsRecursive(FsNode* dnode, const glm::mat4& viewProj,
                                              ImVec2 imgPos, ImVec2 imgSize,
                                              ImDrawList* drawList, int depth) {
    if (depth > 20) return;

    GeometryManager& gm = GeometryManager::instance();
    constexpr double PI = 3.14159265358979323846;

    // Get absolute cylindrical coords for this directory's platform
    double platformR0 = gm.treevPlatformR0(dnode);
    double platformTheta = gm.treevPlatformTheta(dnode);

    for (auto& childPtr : dnode->children) {
        FsNode* node = childPtr.get();
        bool isExpandedDir = node->isDir() && !node->isCollapsed();

        const char* name = node->name.c_str();

        if (!node->isDir()) {
            double r = platformR0 + node->treevGeom.leaf.distance;
            double theta = platformTheta + node->treevGeom.leaf.theta;
            double thetaRad = theta * PI / 180.0;
            float worldX = static_cast<float>(r * std::cos(thetaRad));
            float worldY = static_cast<float>(r * std::sin(thetaRad));
            double parentHeight = dnode->treevGeom.platform.height;
            float topZ = static_cast<float>(parentHeight + node->treevGeom.leaf.height);
            float baseZ = static_cast<float>(parentHeight);

            // Label on TOP of leaf
            drawTreeVLabel(drawList, viewProj, imgPos, imgSize,
                           glm::vec3(worldX, worldY, topZ), 128.0f, name);

            // Label on FRONT face (midpoint of front face height)
            float frontZ = (baseZ + topZ) * 0.5f;
            drawTreeVLabel(drawList, viewProj, imgPos, imgSize,
                           glm::vec3(worldX, worldY, frontZ), 128.0f, name);
        } else {
            double childR0 = gm.treevPlatformR0(node);
            double childTheta = gm.treevPlatformTheta(node);
            double r = childR0 + 0.5 * node->treevGeom.platform.depth;
            double thetaRad = childTheta * PI / 180.0;
            float worldX = static_cast<float>(r * std::cos(thetaRad));
            float worldY = static_cast<float>(r * std::sin(thetaRad));
            float topZ = static_cast<float>(node->treevGeom.platform.height);
            float edgeOffset = static_cast<float>(node->treevGeom.platform.depth * 0.5);

            // Label on top of platform
            drawTreeVLabel(drawList, viewProj, imgPos, imgSize,
                           glm::vec3(worldX, worldY, topZ), edgeOffset, name);
        }

        // Always recurse into expanded directories regardless of projection
        if (isExpandedDir) {
            drawTreeVLabelsRecursive(node, viewProj, imgPos, imgSize, drawList, depth + 1);
        }
    }
}

// ============================================================================
// Main draw
// ============================================================================

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

    // Store view/projection for label overlay and hit testing
    glm::mat4 cachedView(1.0f);
    glm::mat4 cachedProj(1.0f);
    hasScene_ = false;

    if (fbo_ != 0) {
        const Theme& theme = ThemeManager::instance().currentTheme();

        // Tick pulse effect
        float dt = ImGui::GetIO().DeltaTime;
        PulseEffect::instance().tick(dt);

        // Bind FBO and render the 3D scene into it
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glViewport(0, 0, width_, height_);
        glClearColor(theme.viewportBg.x, theme.viewportBg.y, theme.viewportBg.z, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Enable depth test for 3D rendering, disable culling for correct MapV display
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDisable(GL_CULL_FACE);

        // Render the scene if we have a tree loaded
        if (FsTree::instance().rootDir()) {
            hasScene_ = true;
            Camera& cam = Camera::instance();
            float aspect = (height_ > 0) ? static_cast<float>(width_) / static_cast<float>(height_) : 1.0f;
            cachedView = cam.getViewMatrix();
            cachedProj = cam.getProjectionMatrix(aspect);

            // Set up node shader common uniforms (lighting + glow from theme)
            ShaderProgram& nodeShader = Renderer::instance().getNodeShader();
            nodeShader.use();
            nodeShader.setMat4("uView", cachedView);
            nodeShader.setMat4("uProjection", cachedProj);
            nodeShader.setVec3("uLightPos", theme.lightPos);
            nodeShader.setVec3("uAmbient", theme.ambient);
            nodeShader.setVec3("uDiffuse", theme.diffuse);
            nodeShader.setVec3("uViewPos", glm::vec3(0.0f, 500.0f, 1000.0f));
            nodeShader.setFloat("uHighlight", 0.0f);
            // Glow/rim uniforms from theme
            nodeShader.setVec3("uGlowColor", theme.glowColor);
            nodeShader.setFloat("uGlowIntensity", theme.baseEmissive);
            nodeShader.setFloat("uRimIntensity", theme.rimIntensity);
            nodeShader.setFloat("uRimPower", theme.rimPower);
            nodeShader.unuse();

            // Draw geometry
            GeometryManager::instance().draw(cachedView, cachedProj, true);
        }

        // Disable 3D state before returning to ImGui
        glDisable(GL_DEPTH_TEST);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Record image position before drawing it
        imgPos_ = ImGui::GetCursorScreenPos();
        imgSize_ = availSize;

        // Cache view-projection matrix for hit testing
        cachedViewProj_ = cachedProj * cachedView;

        // Display the FBO color texture as an ImGui image
        ImTextureID texId = (ImTextureID)(uintptr_t)(colorTex_);
        // Flip UV vertically because OpenGL textures are bottom-up
        ImGui::Image(texId, availSize, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));

        // Draw text labels overlay
        if (hasScene_) {
            FsvMode mode = MainWindow::instance().getMode();
            glm::mat4 viewProj = cachedProj * cachedView;
            if (mode == FSV_MAPV) {
                drawMapVLabels(viewProj, imgPos_, imgSize_);
            } else if (mode == FSV_TREEV) {
                drawTreeVLabels(viewProj, imgPos_, imgSize_);
            }
        }
    }

    // Handle mouse input for camera control and clicking
    handleInput();

    // Handle keyboard navigation
    handleKeyboard();

    // Context menu popup (must be within the Viewport window's Begin/End)
    if (openContextMenu_) {
        ImGui::OpenPopup("##ViewportContextMenu");
        openContextMenu_ = false;
    }
    Dialogs::instance().drawContextMenuPopup("##ViewportContextMenu", contextMenuNode_);

    ImGui::End();
}

} // namespace fsvng
