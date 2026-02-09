include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
    GIT_SHALLOW TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Helper function - links against fsvng_core for access to all core types
function(add_fsvng_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE fsvng_core gtest gtest_main)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_fsvng_test(test_FsNode)
add_fsvng_test(test_FsScanner)
add_fsvng_test(test_Morph)
add_fsvng_test(test_MapVLayout)
add_fsvng_test(test_TreeVLayout)
add_fsvng_test(test_ColorSystem)
add_fsvng_test(test_Camera)
