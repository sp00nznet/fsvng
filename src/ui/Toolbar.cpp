#include "ui/Toolbar.h"

#include <imgui.h>
#include <imgui_internal.h>

#include "core/Types.h"
#include "ui/MainWindow.h"
#include "camera/Camera.h"

namespace fsvng {

Toolbar& Toolbar::instance() {
    static Toolbar s;
    return s;
}

void Toolbar::draw() {
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    float height = ImGui::GetFrameHeightWithSpacing();

    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y));
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, height));
    ImGui::SetNextWindowViewport(viewport->ID);

    if (ImGui::Begin("##Toolbar", nullptr, flags)) {
        MainWindow& mw = MainWindow::instance();

        // Navigation buttons
        if (ImGui::Button("Back")) {
            mw.navigateBack();
        }
        ImGui::SameLine();
        if (ImGui::Button("CD Root")) {
            mw.navigateToRoot();
        }
        ImGui::SameLine();
        if (ImGui::Button("CD Up")) {
            mw.navigateUp();
        }
        ImGui::SameLine();
        if (ImGui::Button("Bird's Eye")) {
            Camera& cam = Camera::instance();
            cam.birdseyeView(!cam.isBirdseyeActive());
        }

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        // Vis mode selector
        FsvMode currentMode = mw.getMode();
        ImGui::Text("Mode:");
        ImGui::SameLine();
        if (ImGui::RadioButton("Map", currentMode == FSV_MAPV)) {
            mw.setMode(FSV_MAPV);
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Tree", currentMode == FSV_TREEV)) {
            mw.setMode(FSV_TREEV);
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Disc", currentMode == FSV_DISCV)) {
            mw.setMode(FSV_DISCV);
        }
    }
    ImGui::End();
}

} // namespace fsvng
