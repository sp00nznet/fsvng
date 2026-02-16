#pragma once

namespace fsvng {

class MenuBar {
public:
    static MenuBar& instance();
    void draw();

private:
    MenuBar() = default;
    void drawFileMenu();
    void drawVisMenu();
    void drawColorsMenu();
    void drawThemesMenu();
    void drawHelpMenu();
};

} // namespace fsvng
