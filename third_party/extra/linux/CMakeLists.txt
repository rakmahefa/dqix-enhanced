cmake_minimum_required(VERSION 3.20)
project(dqix_linux_ui LANGUAGES CXX)

include(CTest)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
find_program(BASH_EXECUTABLE bash REQUIRED)

add_library(dqix-linux-settings STATIC settings/settings_model.cpp)
target_include_directories(dqix-linux-settings PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/settings")
target_compile_features(dqix-linux-settings PUBLIC cxx_std_17)

add_executable(dqix-linux-ui-launcher ui/launcher_main.cpp)
target_compile_features(dqix-linux-ui-launcher PRIVATE cxx_std_17)
target_link_libraries(dqix-linux-ui-launcher PRIVATE dqix-linux-settings PkgConfig::SDL2)

if(BUILD_TESTING)
    add_executable(dqix-linux-settings-tests settings/settings_model_test.cpp)
    target_compile_features(dqix-linux-settings-tests PRIVATE cxx_std_17)
    target_link_libraries(dqix-linux-settings-tests PRIVATE dqix-linux-settings)
    add_test(NAME linux-settings-model-and-runtime-parity
        COMMAND "$<TARGET_FILE:dqix-linux-settings-tests>"
                "${CMAKE_CURRENT_SOURCE_DIR}/../../../runtime/frontend_sdl.c")
    add_test(NAME linux-ui-launcher-help COMMAND "$<TARGET_FILE:dqix-linux-ui-launcher>" --help)
    add_test(NAME linux-ui-sdl2-software-renderer-smoke
        COMMAND "${BASH_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/smoke-ui.sh"
                "$<TARGET_FILE:dqix-linux-ui-launcher>")
    set_tests_properties(linux-ui-sdl2-software-renderer-smoke PROPERTIES TIMEOUT 15)
endif()

message(STATUS "DQIX Linux launcher: SDL2 + SDL_Renderer; no external UI toolkit or Vulkan")
message(STATUS "DQIX in-game runtime bridge: not implemented")
