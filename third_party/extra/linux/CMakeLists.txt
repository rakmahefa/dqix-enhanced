cmake_minimum_required(VERSION 3.20)

project(dqix_linux_ui_p0 LANGUAGES CXX)

include(CTest)
include(FetchContent)

find_package(SDL3 CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_program(BASH_EXECUTABLE bash REQUIRED)

# Immutable upstream commit for Dear ImGui v1.93.0.
# Keep the upstream example unmodified while proving the Linux toolchain.
set(DQIX_DEAR_IMGUI_COMMIT "c1aaac65e896a45f81f47a4d68f9959e0f86c859")

FetchContent_Declare(
    dear_imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        ${DQIX_DEAR_IMGUI_COMMIT}
    GIT_PROGRESS   TRUE
)

FetchContent_GetProperties(dear_imgui)
if(NOT dear_imgui_POPULATED)
    FetchContent_Populate(dear_imgui)
endif()

set(DEAR_IMGUI_DIR "${dear_imgui_SOURCE_DIR}")

add_library(dqix_dear_imgui STATIC
    "${DEAR_IMGUI_DIR}/imgui.cpp"
    "${DEAR_IMGUI_DIR}/imgui_draw.cpp"
    "${DEAR_IMGUI_DIR}/imgui_tables.cpp"
    "${DEAR_IMGUI_DIR}/imgui_widgets.cpp"
    "${DEAR_IMGUI_DIR}/imgui_demo.cpp"
    "${DEAR_IMGUI_DIR}/backends/imgui_impl_sdl3.cpp"
    "${DEAR_IMGUI_DIR}/backends/imgui_impl_vulkan.cpp"
)

target_include_directories(dqix_dear_imgui
    PUBLIC
        "${DEAR_IMGUI_DIR}"
        "${DEAR_IMGUI_DIR}/backends"
)

target_compile_features(dqix_dear_imgui PUBLIC cxx_std_17)
target_link_libraries(dqix_dear_imgui PUBLIC SDL3::SDL3 Vulkan::Vulkan)

# The settings layer stays independent from SDL/Vulkan so it can be tested without
# a window, a GPU, a ROM, or an initialized game runtime.
add_library(dqix-linux-settings STATIC
    settings/settings_model.cpp
)
target_include_directories(dqix-linux-settings PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/settings")
target_compile_features(dqix-linux-settings PUBLIC cxx_std_17)

if(BUILD_TESTING)
    add_executable(dqix-linux-settings-tests settings/settings_model_test.cpp)
    target_compile_features(dqix-linux-settings-tests PRIVATE cxx_std_17)
    target_link_libraries(dqix-linux-settings-tests PRIVATE dqix-linux-settings)
    add_test(
        NAME linux-settings-model-and-runtime-parity
        COMMAND "$<TARGET_FILE:dqix-linux-settings-tests>"
                "${CMAKE_CURRENT_SOURCE_DIR}/../../../runtime/frontend_sdl.c"
    )
endif()

# This is the official upstream example, used only as a technical probe.
# The actual DQIX launcher UI and engine bridge are intentionally not part of P0.
add_executable(dqix-linux-ui-prototype
    "${DEAR_IMGUI_DIR}/examples/example_sdl3_vulkan/main.cpp"
)
target_compile_features(dqix-linux-ui-prototype PRIVATE cxx_std_17)
target_link_libraries(dqix-linux-ui-prototype PRIVATE dqix_dear_imgui)

if(BUILD_TESTING)
    add_test(
        NAME linux-ui-sdl3-vulkan-imgui-smoke
        COMMAND "${BASH_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/smoke-ui.sh"
                "$<TARGET_FILE:dqix-linux-ui-prototype>"
    )
    set_tests_properties(linux-ui-sdl3-vulkan-imgui-smoke PROPERTIES
        SKIP_RETURN_CODE 77
        TIMEOUT 25
    )
endif()

message(STATUS "DQIX Linux P0 prototype: SDL3 + Dear ImGui + Vulkan")
message(STATUS "Dear ImGui pinned commit: ${DQIX_DEAR_IMGUI_COMMIT}")
message(STATUS "DQIX runtime integration: not attempted in this standalone probe")
