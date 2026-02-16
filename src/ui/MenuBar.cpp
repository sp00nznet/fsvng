#include "ui/MenuBar.h"

#include <imgui.h>
#include <SDL.h>

#include "core/Types.h"
#include "ui/Dialogs.h"
#include "ui/MainWindow.h"
#include "ui/ThemeManager.h"
#include "app/Config.h"

namespace fsvng {

MenuBar& MenuBar::instance() {
    static MenuBar s;
    return s;
}

void MenuBar::draw() {
    if (ImGui::BeginMainMenuBar()) {
        drawFileMenu();
        drawVisMenu();
        drawColorsMenu();
        drawThemesMenu();
        drawHelpMenu();
        ImGui::EndMainMenuBar();
    }
}

void MenuBar::drawFileMenu() {
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Change Root...")) {
            Dialogs::instance().showChangeRoot();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Set Default Path...")) {
            Dialogs::instance().showSetDefaultPath();
        }
        bool hasDefault = !Config::instance().defaultPath.empty();
        if (ImGui::MenuItem("Rescan Default", nullptr, false, hasDefault)) {
            MainWindow::instance().requestScan(Config::instance().defaultPath);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Exit", "Alt+F4")) {
            SDL_Event quitEvent;
            quitEvent.type = SDL_QUIT;
            SDL_PushEvent(&quitEvent);
        }
        ImGui::EndMenu();
    }
}

void MenuBar::drawVisMenu() {
    MainWindow& mw = MainWindow::instance();
    FsvMode currentMode = mw.getMode();

    if (ImGui::BeginMenu("Vis")) {
        if (ImGui::RadioButton("MapV", currentMode == FSV_MAPV)) {
            mw.setMode(FSV_MAPV);
        }
        if (ImGui::RadioButton("TreeV", currentMode == FSV_TREEV)) {
            mw.setMode(FSV_TREEV);
        }
        if (ImGui::RadioButton("DiscV", currentMode == FSV_DISCV)) {
            mw.setMode(FSV_DISCV);
        }
        ImGui::EndMenu();
    }
}

void MenuBar::drawColorsMenu() {
    MainWindow& mw = MainWindow::instance();
    ColorMode currentColorMode = mw.getColorMode();

    if (ImGui::BeginMenu("Colors")) {
        if (ImGui::RadioButton("By Type", currentColorMode == COLOR_BY_NODETYPE)) {
            mw.setColorMode(COLOR_BY_NODETYPE);
        }
        if (ImGui::RadioButton("By Timestamp", currentColorMode == COLOR_BY_TIMESTAMP)) {
            mw.setColorMode(COLOR_BY_TIMESTAMP);
        }
        if (ImGui::RadioButton("By Wildcard", currentColorMode == COLOR_BY_WPATTERN)) {
            mw.setColorMode(COLOR_BY_WPATTERN);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Configure Colors...")) {
            Dialogs::instance().showColorConfig();
        }
        ImGui::EndMenu();
    }
}

void MenuBar::drawThemesMenu() {
    if (ImGui::BeginMenu("Themes")) {
        ThemeManager& tm = ThemeManager::instance();
        int current = tm.currentIndex();
        const auto& themes = tm.themes();
        for (int i = 0; i < static_cast<int>(themes.size()); ++i) {
            if (ImGui::RadioButton(themes[i].displayName.c_str(), current == i)) {
                tm.setThemeByIndex(i);
            }
        }
        ImGui::EndMenu();
    }
}

void MenuBar::drawHelpMenu() {
    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("About...")) {
            Dialogs::instance().showAbout();
        }
        ImGui::EndMenu();
    }
}

} // namespace fsvng
