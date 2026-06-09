#include "ui/Toolbar.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <vector>

#include "core/Types.h"
#include "core/FsTree.h"
#include "core/FsNode.h"
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
    // Two rows: controls on top, breadcrumb/address bar below.
    float height = ImGui::GetFrameHeightWithSpacing() * 2.0f + 8.0f;

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
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        // Search / filter: highlights matching nodes in the 3D scene.
        ImGui::Text("Search:");
        ImGui::SameLine();
        static char searchBuf[256] = "";
        ImGui::SetNextItemWidth(180.0f);
        if (ImGui::InputTextWithHint("##search", "name or *.ext", searchBuf, sizeof(searchBuf))) {
            mw.setSearchQuery(searchBuf);
        }
        if (searchBuf[0] != '\0') {
            ImGui::SameLine();
            ImGui::TextDisabled("%d match%s", mw.getSearchMatchCount(),
                                mw.getSearchMatchCount() == 1 ? "" : "es");
            ImGui::SameLine();
            if (ImGui::SmallButton("Clear")) {
                searchBuf[0] = '\0';
                mw.setSearchQuery("");
            }
        }

        // ---- Row 2: breadcrumb + address bar ------------------------------
        ImGui::Separator();

        // Breadcrumb: clickable path segments from the scanned root down to
        // the current node. Clicking a segment navigates to that ancestor.
        FsNode* cur = mw.getCurrentNode();
        if (!cur) {
            cur = FsTree::instance().rootDir();
        }
        std::vector<FsNode*> chain;
        for (FsNode* n = cur; n && !n->isMetanode(); n = n->parent) {
            chain.push_back(n);
        }
        ImGui::Text("Path:");
        ImGui::SameLine();
        for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
            FsNode* n = *it;
            const char* label = n->name.empty() ? "/" : n->name.c_str();
            ImGui::PushID(n);
            if (ImGui::SmallButton(label)) {
                mw.navigateTo(n);
            }
            ImGui::PopID();
            if (std::next(it) != chain.rend()) {
                ImGui::SameLine(0.0f, 2.0f);
                ImGui::TextUnformatted("/");
                ImGui::SameLine(0.0f, 2.0f);
            }
        }

        // Address bar: type or paste a path and press Enter to scan it.
        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();
        ImGui::Text("Go:");
        ImGui::SameLine();
        static char addrBuf[1024] = "";
        ImGui::SetNextItemWidth(280.0f);
        if (ImGui::InputTextWithHint("##address", "/path/to/scan",
                                     addrBuf, sizeof(addrBuf),
                                     ImGuiInputTextFlags_EnterReturnsTrue)) {
            if (addrBuf[0] != '\0') {
                mw.requestScan(addrBuf);
            }
        }

        // Refresh affordance — highlighted when the watcher reports changes.
        ImGui::SameLine();
        bool pending = mw.hasPendingChanges();
        if (pending) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.70f, 0.45f, 0.10f, 1.0f));
        }
        if (ImGui::Button(pending ? "Refresh *" : "Refresh")) {
            mw.rescanCurrentRoot();
        }
        if (pending) {
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Filesystem changed since last scan");
            }
        }
    }
    ImGui::End();
}

} // namespace fsvng
