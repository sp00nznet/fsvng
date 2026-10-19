#include "app/App.h"

#include <SDL.h>
#include <glad/gl.h>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>

#include "ui/ImGuiBackend.h"
#include "ui/MainWindow.h"
#include "core/FsTree.h"
#include "core/FsScanner.h"
#include "animation/Animation.h"
#include "renderer/Renderer.h"
#include "color/ColorSystem.h"
#include "app/Config.h"
#include "ui/ThemeManager.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace fsvng {

App* App::instance_ = nullptr;

App::App() {
    instance_ = this;
}

App::~App() {
    instance_ = nullptr;
}

App& App::instance() {
    return *instance_;
}

bool App::init(int argc, char* argv[]) {
    // Parse command-line arguments
    if (argc > 1) {
        initialPath_ = argv[1];
    }

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return false;
    }

#ifdef __EMSCRIPTEN__
    // The browser: WebGL 2, which is OpenGL ES 3.0
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
    // Request OpenGL 3.3 Core
#ifdef __APPLE__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // macOS requires it for 3.3 core
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
#endif
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

    Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    window_ = SDL_CreateWindow(
        "fsvng - 3D File System Visualizer",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        windowWidth_, windowHeight_,
        windowFlags
    );
    if (!window_) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }

    glContext_ = SDL_GL_CreateContext(window_);
    if (!glContext_) {
        SDL_Log("SDL_GL_CreateContext failed: %s", SDL_GetError());
        return false;
    }
    SDL_GL_MakeCurrent(window_, glContext_);
    SDL_GL_SetSwapInterval(1); // vsync

    // Load OpenGL functions via glad
    int version = gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress);
    if (!version) {
        SDL_Log("gladLoadGL failed");
        return false;
    }

    // Load config
    Config::instance().load();

    // Initialize ImGui
    ImGuiBackend::init(window_, glContext_);

    // Initialize renderer
    Renderer::instance().init();

    // Initialize color system
    ColorSystem::instance().init();

    // Initialize theme system
    ThemeManager::instance().init();
    ThemeManager::instance().setThemeById(Config::instance().themeName);

    // Initialize animation system
    Animation::instance().init();

    // Set initial scan path: CLI arg > default path > current dir
#ifdef __EMSCRIPTEN__
    if (initialPath_.empty()) initialPath_ = "/fsvng";   // the folder bundled into the page
#endif
    if (initialPath_.empty()) {
        const std::string& dp = Config::instance().defaultPath;
        if (!dp.empty()) {
            initialPath_ = dp;
        } else {
            initialPath_ = ".";
        }
    }
    MainWindow::instance().setInitialPath(initialPath_);

    running_ = true;
    return true;
}

void App::frame() {
    processEvents();
    beginFrame();

    // Update animation
    Animation::instance().tick();

    // Draw the main UI
    MainWindow::instance().draw();

    endFrame();
}

void App::run() {
#ifdef __EMSCRIPTEN__
    // The browser owns the loop: one frame per animation frame, and run() never returns.
    emscripten_set_main_loop_arg([](void* app) { static_cast<App*>(app)->frame(); }, this, 0, true);
#else
    while (running_) {
        frame();
    }
#endif
}

void App::shutdown() {
    Config::instance().themeName = ThemeManager::instance().currentTheme().id;
    Config::instance().save();
    Renderer::instance().shutdown();
    ImGuiBackend::shutdown();

    if (glContext_) {
        SDL_GL_DeleteContext(glContext_);
        glContext_ = nullptr;
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Quit();
}

void App::processEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);
        if (event.type == SDL_QUIT) {
            running_ = false;
        }
        if (event.type == SDL_WINDOWEVENT &&
            event.window.event == SDL_WINDOWEVENT_CLOSE &&
            event.window.windowID == SDL_GetWindowID(window_)) {
            running_ = false;
        }
        if (event.type == SDL_WINDOWEVENT &&
            event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
            windowWidth_ = event.window.data1;
            windowHeight_ = event.window.data2;
        }
    }
}

void App::beginFrame() {
    ImGuiBackend::beginFrame();
}

void App::endFrame() {
    ImGuiBackend::endFrame(window_);
}

} // namespace fsvng
