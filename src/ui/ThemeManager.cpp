#include "ui/ThemeManager.h"
#include "color/ColorSystem.h"
#include "geometry/GeometryManager.h"
#include "core/FsTree.h"
#include "core/FsNode.h"

namespace fsvng {

ThemeManager& ThemeManager::instance() {
    static ThemeManager s;
    return s;
}

void ThemeManager::init() {
    buildThemes();
    applyImGuiStyle();
}

// Helper to copy Classic defaults into a theme's nodeTypeColors
static void copyClassicColors(RGBcolor (&dst)[NUM_NODE_TYPES]) {
    for (int i = 0; i < NUM_NODE_TYPES; ++i) {
        dst[i] = ColorSystem::defaultNodeTypeColors[i];
    }
}

void ThemeManager::buildThemes() {
    themes_.clear();

    // ---------------------------------------------------------------
    // 0: Classic (original fsv colors)
    //    Dir: gray, File: yellow, Symlink: white
    // ---------------------------------------------------------------
    {
        Theme t;
        t.id = "classic";
        t.displayName = "Classic";
        t.viewportBg = {0.08f, 0.08f, 0.10f};
        t.accentPrimary = {0.26f, 0.59f, 0.98f};
        t.textColor = {1.0f, 1.0f, 1.0f};
        t.labelColor = IM_COL32(255, 255, 255, 220);
        t.labelShadow = IM_COL32(0, 0, 0, 180);
        copyClassicColors(t.nodeTypeColors);
        themes_.push_back(t);
    }

    // ---------------------------------------------------------------
    // 1: Tron (dark + cyan/blue tones)
    //    Dir: steel blue, File: light cyan, Symlink: bright white-cyan
    // ---------------------------------------------------------------
    {
        Theme t;
        t.id = "tron";
        t.displayName = "Tron";
        t.viewportBg = {0.01f, 0.02f, 0.05f};
        t.accentPrimary = {0.0f, 0.9f, 1.0f};
        t.accentSecondary = {0.0f, 0.3f, 0.4f};
        t.textColor = {0.7f, 1.0f, 1.0f};
        t.labelColor = IM_COL32(0, 230, 255, 240);
        t.labelShadow = IM_COL32(0, 40, 60, 200);

        t.nodeTypeColors[NODE_METANODE]  = {0.0f,  0.0f,  0.0f };
        t.nodeTypeColors[NODE_DIRECTORY] = {0.35f, 0.50f, 0.60f};  // steel blue-gray
        t.nodeTypeColors[NODE_REGFILE]   = {0.55f, 0.90f, 1.00f};  // light cyan
        t.nodeTypeColors[NODE_SYMLINK]   = {0.85f, 1.00f, 1.00f};  // bright white-cyan
        t.nodeTypeColors[NODE_FIFO]      = {0.00f, 0.50f, 1.00f};  // electric blue
        t.nodeTypeColors[NODE_SOCKET]    = {0.00f, 0.80f, 0.60f};  // teal
        t.nodeTypeColors[NODE_CHARDEV]   = {0.00f, 1.00f, 1.00f};  // bright cyan
        t.nodeTypeColors[NODE_BLOCKDEV]  = {0.20f, 0.40f, 0.80f};  // dark blue
        t.nodeTypeColors[NODE_UNKNOWN]   = {1.00f, 0.20f, 0.10f};  // red-orange

        themes_.push_back(t);
    }

    // ---------------------------------------------------------------
    // 2: Matrix (black + green tones)
    //    Dir: dark olive, File: yellow-green, Symlink: bright green-white
    // ---------------------------------------------------------------
    {
        Theme t;
        t.id = "matrix";
        t.displayName = "Matrix";
        t.viewportBg = {0.0f, 0.0f, 0.0f};
        t.accentPrimary = {0.0f, 1.0f, 0.3f};
        t.accentSecondary = {0.0f, 0.3f, 0.1f};
        t.textColor = {0.6f, 1.0f, 0.7f};
        t.labelColor = IM_COL32(0, 255, 80, 240);
        t.labelShadow = IM_COL32(0, 30, 0, 200);

        t.nodeTypeColors[NODE_METANODE]  = {0.0f,  0.0f,  0.0f };
        t.nodeTypeColors[NODE_DIRECTORY] = {0.30f, 0.50f, 0.20f};  // dark olive
        t.nodeTypeColors[NODE_REGFILE]   = {0.70f, 1.00f, 0.40f};  // yellow-green
        t.nodeTypeColors[NODE_SYMLINK]   = {0.80f, 1.00f, 0.80f};  // bright green-white
        t.nodeTypeColors[NODE_FIFO]      = {0.20f, 0.90f, 0.00f};  // lime
        t.nodeTypeColors[NODE_SOCKET]    = {0.80f, 0.90f, 0.00f};  // yellow-green
        t.nodeTypeColors[NODE_CHARDEV]   = {0.00f, 1.00f, 0.50f};  // bright green
        t.nodeTypeColors[NODE_BLOCKDEV]  = {0.10f, 0.60f, 0.30f};  // dark green
        t.nodeTypeColors[NODE_UNKNOWN]   = {0.80f, 0.10f, 0.00f};  // dark red

        themes_.push_back(t);
    }

    // ---------------------------------------------------------------
    // 3: Synthwave (purple bg + pink/magenta tones)
    //    Dir: muted purple, File: warm pink, Symlink: bright lavender
    // ---------------------------------------------------------------
    {
        Theme t;
        t.id = "synthwave";
        t.displayName = "Synthwave";
        t.viewportBg = {0.05f, 0.01f, 0.10f};
        t.accentPrimary = {1.0f, 0.2f, 0.8f};
        t.accentSecondary = {0.4f, 0.05f, 0.3f};
        t.textColor = {1.0f, 0.8f, 1.0f};
        t.labelColor = IM_COL32(255, 60, 200, 240);
        t.labelShadow = IM_COL32(40, 0, 30, 200);

        t.nodeTypeColors[NODE_METANODE]  = {0.0f,  0.0f,  0.0f };
        t.nodeTypeColors[NODE_DIRECTORY] = {0.55f, 0.35f, 0.65f};  // muted purple
        t.nodeTypeColors[NODE_REGFILE]   = {1.00f, 0.60f, 0.85f};  // warm pink
        t.nodeTypeColors[NODE_SYMLINK]   = {0.90f, 0.80f, 1.00f};  // bright lavender
        t.nodeTypeColors[NODE_FIFO]      = {1.00f, 0.10f, 0.60f};  // hot pink
        t.nodeTypeColors[NODE_SOCKET]    = {1.00f, 0.40f, 0.30f};  // coral
        t.nodeTypeColors[NODE_CHARDEV]   = {0.70f, 0.20f, 1.00f};  // electric purple
        t.nodeTypeColors[NODE_BLOCKDEV]  = {0.30f, 0.20f, 0.80f};  // deep blue
        t.nodeTypeColors[NODE_UNKNOWN]   = {0.80f, 0.00f, 0.30f};  // dark magenta

        themes_.push_back(t);
    }

    // ---------------------------------------------------------------
    // 4: Ember (dark warm bg + orange/gold tones)
    //    Dir: warm brown, File: gold, Symlink: pale cream
    // ---------------------------------------------------------------
    {
        Theme t;
        t.id = "ember";
        t.displayName = "Ember";
        t.viewportBg = {0.03f, 0.01f, 0.0f};
        t.accentPrimary = {1.0f, 0.3f, 0.05f};
        t.accentSecondary = {0.4f, 0.1f, 0.0f};
        t.textColor = {1.0f, 0.9f, 0.7f};
        t.labelColor = IM_COL32(255, 180, 80, 240);
        t.labelShadow = IM_COL32(40, 10, 0, 200);

        t.nodeTypeColors[NODE_METANODE]  = {0.0f,  0.0f,  0.0f };
        t.nodeTypeColors[NODE_DIRECTORY] = {0.60f, 0.40f, 0.25f};  // warm brown
        t.nodeTypeColors[NODE_REGFILE]   = {1.00f, 0.85f, 0.40f};  // gold
        t.nodeTypeColors[NODE_SYMLINK]   = {1.00f, 0.95f, 0.80f};  // pale cream
        t.nodeTypeColors[NODE_FIFO]      = {1.00f, 0.50f, 0.00f};  // bright orange
        t.nodeTypeColors[NODE_SOCKET]    = {0.90f, 0.20f, 0.00f};  // deep red
        t.nodeTypeColors[NODE_CHARDEV]   = {1.00f, 0.70f, 0.00f};  // amber
        t.nodeTypeColors[NODE_BLOCKDEV]  = {0.70f, 0.30f, 0.10f};  // rust
        t.nodeTypeColors[NODE_UNKNOWN]   = {0.80f, 0.00f, 0.00f};  // dark red

        themes_.push_back(t);
    }
}

void ThemeManager::setThemeByIndex(int index) {
    if (index < 0 || index >= static_cast<int>(themes_.size())) return;
    currentIndex_ = index;
    applyImGuiStyle();
    applyNodeColors();
}

void ThemeManager::setThemeById(const std::string& id) {
    for (int i = 0; i < static_cast<int>(themes_.size()); ++i) {
        if (themes_[i].id == id) {
            setThemeByIndex(i);
            return;
        }
    }
}

void ThemeManager::applyNodeColors() const {
    const Theme& t = themes_[currentIndex_];

    // Update ColorSystem's node-type palette with this theme's colors
    ColorConfig cfg = ColorSystem::instance().getConfig();
    for (int i = 0; i < NUM_NODE_TYPES; ++i) {
        cfg.byNodetype.colors[i] = t.nodeTypeColors[i];
    }
    // Push new config and re-color the entire tree
    ColorSystem::instance().setConfig(cfg, ColorSystem::instance().getMode());

    // Force geometry rebuild so vertex colors update
    GeometryManager::instance().queueUncachedDraw();
}

void ThemeManager::applyImGuiStyle() const {
    const Theme& t = themes_[currentIndex_];

    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    ImVec4 accent(t.accentPrimary.x, t.accentPrimary.y, t.accentPrimary.z, 1.0f);
    ImVec4 accentDim(t.accentPrimary.x * 0.6f, t.accentPrimary.y * 0.6f, t.accentPrimary.z * 0.6f, 1.0f);
    ImVec4 accentHover(
        std::min(t.accentPrimary.x * 1.2f, 1.0f),
        std::min(t.accentPrimary.y * 1.2f, 1.0f),
        std::min(t.accentPrimary.z * 1.2f, 1.0f), 1.0f);
    ImVec4 text(t.textColor.x, t.textColor.y, t.textColor.z, 1.0f);
    ImVec4 bg(t.viewportBg.x * 1.5f, t.viewportBg.y * 1.5f, t.viewportBg.z * 1.5f, 1.0f);
    ImVec4 bgChild(t.viewportBg.x * 2.0f, t.viewportBg.y * 2.0f, t.viewportBg.z * 2.0f, 1.0f);

    colors[ImGuiCol_Text] = text;
    colors[ImGuiCol_TextDisabled] = ImVec4(text.x * 0.5f, text.y * 0.5f, text.z * 0.5f, 1.0f);
    colors[ImGuiCol_WindowBg] = bg;
    colors[ImGuiCol_ChildBg] = bgChild;
    colors[ImGuiCol_PopupBg] = ImVec4(bg.x * 0.8f, bg.y * 0.8f, bg.z * 0.8f, 0.95f);
    colors[ImGuiCol_Border] = ImVec4(accent.x * 0.3f, accent.y * 0.3f, accent.z * 0.3f, 0.5f);
    colors[ImGuiCol_FrameBg] = ImVec4(accent.x * 0.15f, accent.y * 0.15f, accent.z * 0.15f, 0.5f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(accent.x * 0.25f, accent.y * 0.25f, accent.z * 0.25f, 0.5f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(accent.x * 0.35f, accent.y * 0.35f, accent.z * 0.35f, 0.5f);
    colors[ImGuiCol_TitleBg] = bg;
    colors[ImGuiCol_TitleBgActive] = ImVec4(accent.x * 0.2f, accent.y * 0.2f, accent.z * 0.2f, 1.0f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(bg.x * 0.8f, bg.y * 0.8f, bg.z * 0.8f, 1.0f);
    colors[ImGuiCol_Header] = ImVec4(accent.x * 0.25f, accent.y * 0.25f, accent.z * 0.25f, 0.5f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(accent.x * 0.4f, accent.y * 0.4f, accent.z * 0.4f, 0.5f);
    colors[ImGuiCol_HeaderActive] = accentDim;
    colors[ImGuiCol_Button] = ImVec4(accent.x * 0.25f, accent.y * 0.25f, accent.z * 0.25f, 0.6f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(accent.x * 0.4f, accent.y * 0.4f, accent.z * 0.4f, 0.7f);
    colors[ImGuiCol_ButtonActive] = accent;
    colors[ImGuiCol_Tab] = accentDim;
    colors[ImGuiCol_TabHovered] = accentHover;
    colors[ImGuiCol_CheckMark] = accent;
    colors[ImGuiCol_SliderGrab] = accent;
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.2f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(accent.x * 0.3f, accent.y * 0.3f, accent.z * 0.3f, 0.5f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(accent.x * 0.4f, accent.y * 0.4f, accent.z * 0.4f, 0.6f);
    colors[ImGuiCol_ScrollbarGrabActive] = accent;
    colors[ImGuiCol_Separator] = ImVec4(accent.x * 0.3f, accent.y * 0.3f, accent.z * 0.3f, 0.5f);
    colors[ImGuiCol_DockingPreview] = accent;
}

} // namespace fsvng
