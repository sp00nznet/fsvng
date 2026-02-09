#include "ui/StatusBar.h"

#include <imgui.h>

namespace fsvng {

StatusBar& StatusBar::instance() {
    static StatusBar s;
    return s;
}

void StatusBar::setMessage(const std::string& left, const std::string& right) {
    leftMessage_ = left;
    rightMessage_ = right;
}

void StatusBar::draw() {
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    float height = ImGui::GetFrameHeightWithSpacing();

    float yPos = viewport->WorkPos.y + viewport->WorkSize.y - height;
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, yPos));
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, height));
    ImGui::SetNextWindowViewport(viewport->ID);

    if (ImGui::Begin("##StatusBar", nullptr, flags)) {
        // Left-aligned message
        ImGui::TextUnformatted(leftMessage_.c_str());

        // Right-aligned message
        if (!rightMessage_.empty()) {
            float rightWidth = ImGui::CalcTextSize(rightMessage_.c_str()).x;
            float availWidth = ImGui::GetContentRegionAvail().x;
            if (rightWidth < availWidth) {
                ImGui::SameLine(availWidth - rightWidth);
            }
            ImGui::TextUnformatted(rightMessage_.c_str());
        }
    }
    ImGui::End();
}

} // namespace fsvng
