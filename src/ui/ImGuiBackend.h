#pragma once

struct SDL_Window;
typedef void* SDL_GLContext;

namespace fsvng {

class ImGuiBackend {
public:
    static void init(SDL_Window* window, SDL_GLContext glContext);
    static void shutdown();
    static void beginFrame();
    static void endFrame(SDL_Window* window);
};

} // namespace fsvng
