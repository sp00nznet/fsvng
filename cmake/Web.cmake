# The browser build (Emscripten): SDL2 is Emscripten's port, and OpenGL is WebGL 2 (OpenGL ES 3).
# These stand in for the targets FetchDependencies.cmake makes on the desktop.
add_library(SDL2::SDL2-static INTERFACE IMPORTED)
set_target_properties(SDL2::SDL2-static PROPERTIES
    INTERFACE_COMPILE_OPTIONS "-sUSE_SDL=2"
    INTERFACE_LINK_OPTIONS "-sUSE_SDL=2")
add_library(SDL2::SDL2main INTERFACE IMPORTED)

# glad: a stand-in header that includes GLES3 (WebGL 2 is loaded already, nothing to look up).
add_library(glad_gl_core_33 INTERFACE IMPORTED)
set_target_properties(glad_gl_core_33 PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/web")
