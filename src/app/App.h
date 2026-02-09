#pragma once

#include <string>

struct SDL_Window;
typedef void* SDL_GLContext;

namespace fsvng {

class App {
public:
    App();
    ~App();

    bool init(int argc, char* argv[]);
    void run();
    void shutdown();

    SDL_Window* getWindow() const { return window_; }
    int getWindowWidth() const { return windowWidth_; }
    int getWindowHeight() const { return windowHeight_; }

    static App& instance();

private:
    void processEvents();
    void beginFrame();
    void endFrame();

    SDL_Window* window_ = nullptr;
    SDL_GLContext glContext_ = nullptr;
    bool running_ = false;
    int windowWidth_ = 1280;
    int windowHeight_ = 800;
    std::string initialPath_;

    static App* instance_;
};

} // namespace fsvng
