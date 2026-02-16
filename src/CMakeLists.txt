# Core library (no OpenGL/SDL/ImGui dependencies) - used by tests too
set(FSVNG_CORE_SOURCES
    core/FsNode.cpp
    core/FsTree.cpp
    core/FsScanner.cpp
    core/PlatformUtils.cpp
    animation/Morph.cpp
    animation/Animation.cpp
    animation/Scheduler.cpp
    color/ColorSystem.cpp
    color/Spectrum.cpp
)

add_library(fsvng_core STATIC ${FSVNG_CORE_SOURCES})
target_include_directories(fsvng_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(fsvng_core PUBLIC glm::glm nlohmann_json::nlohmann_json)

if(WIN32)
    target_link_libraries(fsvng_core PRIVATE advapi32)
endif()

# Main executable
set(FSVNG_SOURCES
    main.cpp
    app/App.cpp
    app/Config.cpp
    camera/Camera.cpp
    renderer/ShaderProgram.cpp
    renderer/MeshBuffer.cpp
    renderer/Renderer.cpp
    renderer/TextRenderer.cpp
    renderer/NodePicker.cpp
    renderer/SplashRenderer.cpp
    geometry/GeometryManager.cpp
    geometry/MapVLayout.cpp
    geometry/TreeVLayout.cpp
    geometry/CollapseExpand.cpp
    ui/ImGuiBackend.cpp
    ui/MainWindow.cpp
    ui/DirTreePanel.cpp
    ui/FileListPanel.cpp
    ui/ViewportPanel.cpp
    ui/MenuBar.cpp
    ui/Toolbar.cpp
    ui/StatusBar.cpp
    ui/Dialogs.cpp
    ui/ThemeManager.cpp
    ui/PulseEffect.cpp
)

add_executable(fsvng ${FSVNG_SOURCES})

target_link_libraries(fsvng PRIVATE
    fsvng_core
    SDL2::SDL2-static
    SDL2::SDL2main
    glad_gl_core_33
    imgui_lib
)

if(WIN32)
    target_link_libraries(fsvng PRIVATE opengl32)
elseif(APPLE)
    find_library(OPENGL_FRAMEWORK OpenGL)
    target_link_libraries(fsvng PRIVATE ${OPENGL_FRAMEWORK})
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(fsvng PRIVATE OpenGL::GL)
endif()

# Copy shaders to build directory
add_custom_command(TARGET fsvng POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/shaders
    $<TARGET_FILE_DIR:fsvng>/shaders
)
