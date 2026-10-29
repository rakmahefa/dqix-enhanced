// DQIX Enhanced Linux launcher: SDL3 + Dear ImGui + Vulkan.
// Vulkan renders this launcher's UI only; the game's renderer is separate.

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

// Important note to the reader who wish to integrate imgui_impl_vulkan.cpp/.h in their own engine/app.
// - Common ImGui_ImplVulkan_XXX functions and structures are used to interface with imgui_impl_vulkan.cpp/.h.
//   You will use those if you want to use this rendering backend in your engine/app.
// - Helper ImGui_ImplVulkanH_XXX functions and structures are only used by this example (main.cpp) and by
//   the backend itself (imgui_impl_vulkan.cpp), but should PROBABLY NOT be used by your own engine/app code.
// Read comments in imgui_impl_vulkan.h.

// Adapted from Dear ImGui's official SDL3 + Vulkan example (MIT license).
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"
#include "../settings/settings_model.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// This example doesn't compile with Emscripten yet! Awaiting SDL3 support.
#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif

// Volk headers
#ifdef IMGUI_IMPL_VULKAN_USE_VOLK
#define VOLK_IMPLEMENTATION
#include <volk.h>
#endif

//#define APP_USE_UNLIMITED_FRAME_RATE
#ifdef _DEBUG
#define APP_USE_VULKAN_DEBUG_REPORT
static VkDebugReportCallbackEXT g_DebugReport = VK_NULL_HANDLE;
#endif

// Data
static VkAllocationCallbacks*   g_Allocator = nullptr;
static VkInstance               g_Instance = VK_NULL_HANDLE;
static VkPhysicalDevice         g_PhysicalDevice = VK_NULL_HANDLE;
static VkDevice                 g_Device = VK_NULL_HANDLE;
static uint32_t                 g_QueueFamily = (uint32_t)-1;
static VkQueue                  g_Queue = VK_NULL_HANDLE;
static VkPipelineCache          g_PipelineCache = VK_NULL_HANDLE;
static VkDescriptorPool         g_DescriptorPool = VK_NULL_HANDLE;

static ImGui_ImplVulkanH_Window g_MainWindowData;
static uint32_t                 g_MinImageCount = 2;
static bool                     g_SwapChainRebuild = false;

static std::string g_dqix_log_path;

static void launcher_log(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    va_list stderr_args;
    va_copy(stderr_args, args);
    std::vfprintf(stderr, format, stderr_args);
    va_end(stderr_args);
    std::fflush(stderr);

    if (!g_dqix_log_path.empty()) {
        if (FILE* file = std::fopen(g_dqix_log_path.c_str(), "a")) {
            va_list file_args;
            va_copy(file_args, args);
            std::vfprintf(file, format, file_args);
            std::fflush(file);
            std::fclose(file);
            va_end(file_args);
        }
    }
    va_end(args);
}

static void check_vk_result(VkResult err)
{
    if (err == VK_SUCCESS)
        return;
    launcher_log("[vulkan] Error: VkResult = %d\n", err);
    if (err < 0)
        std::exit(EXIT_FAILURE);
}

#ifdef APP_USE_VULKAN_DEBUG_REPORT
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_report(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData)
{
    (void)flags; (void)object; (void)location; (void)messageCode; (void)pUserData; (void)pLayerPrefix; // Unused arguments
    std::fprintf(stderr, "[vulkan] Debug report from ObjectType: %i\nMessage: %s\n\n", objectType, pMessage);
    return VK_FALSE;
}
#endif // APP_USE_VULKAN_DEBUG_REPORT

static bool IsExtensionAvailable(const ImVector<VkExtensionProperties>& properties, const char* extension)
{
    for (const VkExtensionProperties& p : properties)
        if (std::strcmp(p.extensionName, extension) == 0)
            return true;
    return false;
}

static void SetupVulkan(ImVector<const char*> instance_extensions)
{
    VkResult err;
#ifdef IMGUI_IMPL_VULKAN_USE_VOLK
    volkInitialize();
#endif

    // Create Vulkan Instance
    {
        VkInstanceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

        // Enumerate available extensions
        uint32_t properties_count;
        ImVector<VkExtensionProperties> properties;
        err = vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, nullptr);
        check_vk_result(err);
        properties.resize(properties_count);
        err = vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, properties.Data);
        check_vk_result(err);

        // Enable required extensions
        if (IsExtensionAvailable(properties, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
            instance_extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
        if (IsExtensionAvailable(properties, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME))
        {
            instance_extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
            create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        }
#endif

        // Enabling validation layers
#ifdef APP_USE_VULKAN_DEBUG_REPORT
        const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
        create_info.enabledLayerCount = 1;
        create_info.ppEnabledLayerNames = layers;
        instance_extensions.push_back("VK_EXT_debug_report");
#endif

        // Create Vulkan Instance
        create_info.enabledExtensionCount = (uint32_t)instance_extensions.Size;
        create_info.ppEnabledExtensionNames = instance_extensions.Data;
        err = vkCreateInstance(&create_info, g_Allocator, &g_Instance);
        check_vk_result(err);
#ifdef IMGUI_IMPL_VULKAN_USE_VOLK
        volkLoadInstance(g_Instance);
#endif

        // Setup the debug report callback
#ifdef APP_USE_VULKAN_DEBUG_REPORT
        auto f_vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(g_Instance, "vkCreateDebugReportCallbackEXT");
        if (f_vkCreateDebugReportCallbackEXT == nullptr) {
            launcher_log("[vulkan] VK_EXT_debug_report is enabled but vkCreateDebugReportCallbackEXT could not be loaded.\n");
            std::exit(EXIT_FAILURE);
        }
        VkDebugReportCallbackCreateInfoEXT debug_report_ci = {};
        debug_report_ci.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
        debug_report_ci.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
        debug_report_ci.pfnCallback = debug_report;
        debug_report_ci.pUserData = nullptr;
        err = f_vkCreateDebugReportCallbackEXT(g_Instance, &debug_report_ci, g_Allocator, &g_DebugReport);
        check_vk_result(err);
#endif
    }

    // Select Physical Device (GPU)
    g_PhysicalDevice = ImGui_ImplVulkanH_SelectPhysicalDevice(g_Instance);
    if (g_PhysicalDevice == VK_NULL_HANDLE) {
        launcher_log("[vulkan] No compatible physical device was found. Check the host Vulkan driver/device.\n");
        std::exit(EXIT_FAILURE);
    }

    // Select graphics queue family
    g_QueueFamily = ImGui_ImplVulkanH_SelectQueueFamilyIndex(g_PhysicalDevice);
    if (g_QueueFamily == (uint32_t)-1) {
        launcher_log("[vulkan] No graphics queue family is available on the selected Vulkan device.\n");
        std::exit(EXIT_FAILURE);
    }

    // Create Logical Device (with 1 queue)
    {
        ImVector<const char*> device_extensions;
        device_extensions.push_back("VK_KHR_swapchain");

        // Enumerate physical device extension
        uint32_t properties_count;
        ImVector<VkExtensionProperties> properties;
        err = vkEnumerateDeviceExtensionProperties(g_PhysicalDevice, nullptr, &properties_count, nullptr);
        check_vk_result(err);
        properties.resize(properties_count);
        err = vkEnumerateDeviceExtensionProperties(g_PhysicalDevice, nullptr, &properties_count, properties.Data);
        check_vk_result(err);
#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
        if (IsExtensionAvailable(properties, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME))
            device_extensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#endif

        const float queue_priority[] = { 1.0f };
        VkDeviceQueueCreateInfo queue_info[1] = {};
        queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info[0].queueFamilyIndex = g_QueueFamily;
        queue_info[0].queueCount = 1;
        queue_info[0].pQueuePriorities = queue_priority;
        VkDeviceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create_info.queueCreateInfoCount = sizeof(queue_info) / sizeof(queue_info[0]);
        create_info.pQueueCreateInfos = queue_info;
        create_info.enabledExtensionCount = (uint32_t)device_extensions.Size;
        create_info.ppEnabledExtensionNames = device_extensions.Data;
        err = vkCreateDevice(g_PhysicalDevice, &create_info, g_Allocator, &g_Device);
        check_vk_result(err);
        vkGetDeviceQueue(g_Device, g_QueueFamily, 0, &g_Queue);
    }

    // Create Descriptor Pool
    // If you wish to load e.g. additional textures you may need to alter pools sizes and maxSets.
    {
        VkDescriptorPoolSize pool_sizes[] =
        {
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE },
            { VK_DESCRIPTOR_TYPE_SAMPLER, IMGUI_IMPL_VULKAN_MINIMUM_SAMPLER_POOL_SIZE },
        };
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 0;
        for (VkDescriptorPoolSize& pool_size : pool_sizes)
            pool_info.maxSets += pool_size.descriptorCount;
        pool_info.poolSizeCount = (uint32_t)IM_COUNTOF(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;
        err = vkCreateDescriptorPool(g_Device, &pool_info, g_Allocator, &g_DescriptorPool);
        check_vk_result(err);
    }
}

// All the ImGui_ImplVulkanH_XXX structures/functions are optional helpers used by the demo.
// Your real engine/app may not use them.
static void SetupVulkanWindow(ImGui_ImplVulkanH_Window* wd, VkSurfaceKHR surface, int width, int height)
{
    // Check for WSI support
    VkBool32 res;
    const VkResult support_result = vkGetPhysicalDeviceSurfaceSupportKHR(g_PhysicalDevice, g_QueueFamily, surface, &res);
    check_vk_result(support_result);
    if (res != VK_TRUE)
    {
        launcher_log("[vulkan] Selected device cannot present to the SDL window surface (WSI unsupported).\n");
        std::exit(EXIT_FAILURE);
    }

    // Select Surface Format
    const VkFormat requestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
    const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    wd->Surface = surface;
    wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(g_PhysicalDevice, wd->Surface, requestSurfaceImageFormat, (size_t)IM_COUNTOF(requestSurfaceImageFormat), requestSurfaceColorSpace);

    // Select Present Mode
#ifdef APP_USE_UNLIMITED_FRAME_RATE
    VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR };
#else
    VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
#endif
    wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(g_PhysicalDevice, wd->Surface, &present_modes[0], IM_COUNTOF(present_modes));
    //std::printf("[vulkan] Selected PresentMode = %d\n", wd->PresentMode);

    // Create SwapChain, RenderPass, Framebuffer, etc.
    IM_ASSERT(g_MinImageCount >= 2);
    ImGui_ImplVulkanH_CreateOrResizeWindow(g_Instance, g_PhysicalDevice, g_Device, wd, g_QueueFamily, g_Allocator, width, height, g_MinImageCount, 0);
}

static void CleanupVulkan()
{
    vkDestroyDescriptorPool(g_Device, g_DescriptorPool, g_Allocator);

#ifdef APP_USE_VULKAN_DEBUG_REPORT
    // Remove the debug report callback
    auto f_vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(g_Instance, "vkDestroyDebugReportCallbackEXT");
    f_vkDestroyDebugReportCallbackEXT(g_Instance, g_DebugReport, g_Allocator);
#endif // APP_USE_VULKAN_DEBUG_REPORT

    vkDestroyDevice(g_Device, g_Allocator);
    vkDestroyInstance(g_Instance, g_Allocator);
}

static void CleanupVulkanWindow(ImGui_ImplVulkanH_Window* wd)
{
    ImGui_ImplVulkanH_DestroyWindow(g_Instance, g_Device, wd, g_Allocator);
    vkDestroySurfaceKHR(g_Instance, wd->Surface, g_Allocator);
}

static void FrameRender(ImGui_ImplVulkanH_Window* wd, ImDrawData* draw_data)
{
    VkSemaphore image_acquired_semaphore  = wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
    VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
    VkResult err = vkAcquireNextImageKHR(g_Device, wd->Swapchain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &wd->FrameIndex);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
        g_SwapChainRebuild = true;
    if (err == VK_ERROR_OUT_OF_DATE_KHR)
        return;
    if (err != VK_SUBOPTIMAL_KHR)
        check_vk_result(err);

    ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];
    {
        err = vkWaitForFences(g_Device, 1, &fd->Fence, VK_TRUE, UINT64_MAX);    // wait indefinitely instead of periodically checking
        check_vk_result(err);

        err = vkResetFences(g_Device, 1, &fd->Fence);
        check_vk_result(err);
    }
    {
        err = vkResetCommandPool(g_Device, fd->CommandPool, 0);
        check_vk_result(err);
        VkCommandBufferBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        err = vkBeginCommandBuffer(fd->CommandBuffer, &info);
        check_vk_result(err);
    }
    {
        VkRenderPassBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.renderPass = wd->RenderPass;
        info.framebuffer = fd->Framebuffer;
        info.renderArea.extent.width = wd->Width;
        info.renderArea.extent.height = wd->Height;
        info.clearValueCount = 1;
        info.pClearValues = &wd->ClearValue;
        vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
    }

    // Record dear imgui primitives into command buffer
    ImGui_ImplVulkan_RenderDrawData(draw_data, fd->CommandBuffer);

    // Submit command buffer
    vkCmdEndRenderPass(fd->CommandBuffer);
    {
        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores = &image_acquired_semaphore;
        info.pWaitDstStageMask = &wait_stage;
        info.commandBufferCount = 1;
        info.pCommandBuffers = &fd->CommandBuffer;
        info.signalSemaphoreCount = 1;
        info.pSignalSemaphores = &render_complete_semaphore;

        err = vkEndCommandBuffer(fd->CommandBuffer);
        check_vk_result(err);
        err = vkQueueSubmit(g_Queue, 1, &info, fd->Fence);
        check_vk_result(err);
    }
}

static void FramePresent(ImGui_ImplVulkanH_Window* wd)
{
    if (g_SwapChainRebuild)
        return;
    VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
    VkPresentInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &render_complete_semaphore;
    info.swapchainCount = 1;
    info.pSwapchains = &wd->Swapchain;
    info.pImageIndices = &wd->FrameIndex;
    VkResult err = vkQueuePresentKHR(g_Queue, &info);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
        g_SwapChainRebuild = true;
    if (err == VK_ERROR_OUT_OF_DATE_KHR)
        return;
    if (err != VK_SUBOPTIMAL_KHR)
        check_vk_result(err);
    wd->SemaphoreIndex = (wd->SemaphoreIndex + 1) % wd->SemaphoreCount; // Now we can use the next set of semaphores
}

namespace fs = std::filesystem;
using dqix::linux_ui::IniDocument;

namespace {

enum class Page { Game, Graphics, Display, Audio, Controls, Mods, Wireless, System };

struct LauncherState {
    fs::path game_dir;
    fs::path settings_path;
    std::array<char, 1024> game_dir_input{};
    std::array<char, 32> player_name{};
    std::array<char, 64> wfc_server{};
    IniDocument settings;
    Page page = Page::Game;
    std::string message;
    bool message_is_error = false;
    bool settings_load_ok = true;
    pid_t game_pid = -1;
    std::string child_result;
};

fs::path absolute_path(const fs::path& value) {
    std::error_code ec;
    fs::path result = fs::absolute(value, ec);
    return (ec ? value : result).lexically_normal();
}

void copy_to_buffer(char* buffer, std::size_t size, std::string_view value) {
    if (size == 0) return;
    const std::size_t length = std::min(size - 1, value.size());
    std::memcpy(buffer, value.data(), length);
    buffer[length] = '\0';
}

void refresh_text_fields(LauncherState& state) {
    copy_to_buffer(state.player_name.data(), state.player_name.size(),
                   state.settings.get("player_name").value_or(""));
    copy_to_buffer(state.wfc_server.data(), state.wfc_server.size(),
                   state.settings.get("wfc_server").value_or("builtin"));
}

bool load_settings(LauncherState& state) {
    state.settings_path = state.game_dir / "settings.ini";
    state.settings = IniDocument::parse("");
    state.settings_load_ok = true;
    std::error_code ec;
    const bool exists = fs::exists(state.settings_path, ec);
    if (ec) {
        state.message = "Cannot inspect settings path '" + state.settings_path.string() + "': " + ec.message();
        state.message_is_error = true;
        state.settings_load_ok = false;
        refresh_text_fields(state);
        return false;
    }
    if (!exists) {
        state.settings = IniDocument::parse("");
        state.message = "No settings.ini yet. Choose options now; the file will be created on the first save.";
        state.message_is_error = false;
        refresh_text_fields(state);
        return true;
    }
    std::string error;
    if (!state.settings.load_file(state.settings_path, error)) {
        state.message = error;
        state.message_is_error = true;
        state.settings_load_ok = false;
        refresh_text_fields(state);
        return false;
    }
    state.settings_load_ok = true;
    state.message = "Loaded existing settings.ini. Changes are saved atomically.";
    state.message_is_error = false;
    refresh_text_fields(state);
    return true;
}

bool parse_arguments(int argc, char** argv, LauncherState& state, std::string& error) {
    fs::path requested;
    bool has_requested = false;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--help" || arg == "-h") {
            std::printf("DQIX Enhanced Linux Launcher (preview)\n"
                        "Usage: %s [--game-dir PATH] [--help]\n"
                        "PATH must be the built game folder containing dqix and data/.\n",
                        argv[0]);
            return false;
        }
        if (arg == "--game-dir") {
            if (i + 1 >= argc) {
                error = "--game-dir requires a path";
                return false;
            }
            requested = argv[++i];
            has_requested = true;
            continue;
        }
        error = "unknown argument: " + std::string(arg);
        return false;
    }

    if (!has_requested) {
        if (const char* env = std::getenv("DQIX_GAME_DIR"); env && *env) {
            requested = env;
            has_requested = true;
        }
    }
    if (!has_requested) {
        std::error_code ec;
        const fs::path candidate = fs::current_path(ec) / "game";
        if (!ec && fs::is_directory(candidate, ec)) requested = candidate;
        else requested = fs::current_path(ec);
    }
    state.game_dir = absolute_path(requested);
    copy_to_buffer(state.game_dir_input.data(), state.game_dir_input.size(),
                   state.game_dir.string());
    return true;
}

bool commit_setting(LauncherState& state, std::string_view key, std::string_view value) {
    if (!state.settings_load_ok) {
        state.message = "Settings cannot be edited because the selected settings.ini could not be read.";
        state.message_is_error = true;
        return false;
    }
    const IniDocument previous = state.settings;
    std::string error;
    if (!state.settings.set_validated(key, value, error)) {
        state.message = error;
        state.message_is_error = true;
        return false;
    }

    std::error_code ec;
    fs::create_directories(state.game_dir, ec);
    if (ec) {
        state.settings = previous;
        state.message = "Cannot create game directory '" + state.game_dir.string() + "': " + ec.message();
        state.message_is_error = true;
        return false;
    }
    if (!state.settings.save_atomic(state.settings_path, error)) {
        state.settings = previous;
        state.message = error;
        state.message_is_error = true;
        if (key == "player_name" || key == "wfc_server") refresh_text_fields(state);
        return false;
    }

    state.message = "Saved " + std::string(key) + " to " + state.settings_path.string() +
                    ". Effective support depends on the game runtime.";
    state.message_is_error = false;
    if (key == "player_name" || key == "wfc_server") refresh_text_fields(state);
    return true;
}

std::string setting_value(const LauncherState& state, std::string_view key,
                          std::string_view fallback) {
    return state.settings.get(key).value_or(std::string(fallback));
}

bool boolean_setting(const LauncherState& state, std::string_view key, bool fallback = false) {
    const std::string value = setting_value(state, key, fallback ? "1" : "0");
    // The existing runtime uses atoi(value) != 0 for boolean keys; reflect that
    // when opening a pre-existing file, while validating all newly written values.
    char* end = nullptr;
    const long parsed = std::strtol(value.c_str(), &end, 10);
    return end != value.c_str() && *end == '\0' && parsed != 0;
}

int integer_setting(const LauncherState& state, std::string_view key, int fallback,
                    int minimum, int maximum) {
    const std::string value = setting_value(state, key, std::to_string(fallback));
    char* end = nullptr;
    const long parsed = std::strtol(value.c_str(), &end, 10);
    if (end == value.c_str() || *end != '\0' ||
        parsed < minimum || parsed > maximum) return fallback;
    return static_cast<int>(parsed);
}

void checkbox_setting(LauncherState& state, const char* label, const char* key,
                      bool fallback = false) {
    bool value = boolean_setting(state, key, fallback);
    if (ImGui::Checkbox(label, &value))
        commit_setting(state, key, value ? "1" : "0");
}

void slider_setting(LauncherState& state, const char* label, const char* key,
                    int fallback, int minimum, int maximum, const char* format = "%d") {
    int value = integer_setting(state, key, fallback, minimum, maximum);
    if (ImGui::SliderInt(label, &value, minimum, maximum, format))
        commit_setting(state, key, std::to_string(value));
}

void combo_setting(LauncherState& state, const char* label, const char* key,
                   std::string_view fallback, const char* const items[], int count) {
    const std::string value = setting_value(state, key, fallback);
    int selected = 0;
    bool found = false;
    for (int i = 0; i < count; ++i) {
        if (value == items[i]) { selected = i; found = true; break; }
    }
    const std::string preview = found ? value : "Unsupported: " + value;
    if (ImGui::BeginCombo(label, preview.c_str())) {
        for (int i = 0; i < count; ++i) {
            const bool is_selected = found && i == selected;
            if (ImGui::Selectable(items[i], is_selected))
                commit_setting(state, key, items[i]);
            if (is_selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
}

bool path_has_file(const fs::path& path) {
    std::error_code ec;
    return fs::is_regular_file(path, ec);
}

bool game_ready(const LauncherState& state) {
    const fs::path executable = state.game_dir / "dqix";
    const fs::path rom = state.game_dir / "data" / "dqix.nds";
    const fs::path code = state.game_dir / "data" / "code";
    std::error_code ec;
    return path_has_file(executable) && ::access(executable.c_str(), X_OK) == 0 &&
           path_has_file(rom) && fs::is_directory(code, ec);
}

void apply_game_directory(LauncherState& state) {
    const std::string value(state.game_dir_input.data());
    if (value.empty()) {
        state.message = "Enter a game directory path.";
        state.message_is_error = true;
        return;
    }
    state.game_dir = absolute_path(value);
    copy_to_buffer(state.game_dir_input.data(), state.game_dir_input.size(), state.game_dir.string());
    load_settings(state);
}

void launch_game(LauncherState& state) {
    if (state.game_pid > 0) {
        state.message = "The game is already running (PID " + std::to_string(state.game_pid) + ").";
        state.message_is_error = false;
        return;
    }
    if (!game_ready(state)) {
        state.message = "The game is not ready. Expected executable dqix, data/dqix.nds and data/code/ inside the selected folder.";
        state.message_is_error = true;
        return;
    }

    const std::string executable = (state.game_dir / "dqix").string();
    const pid_t pid = ::fork();
    if (pid < 0) {
        state.message = "Could not start game: " + std::string(std::strerror(errno));
        state.message_is_error = true;
        return;
    }
    if (pid == 0) {
        if (::chdir(state.game_dir.c_str()) != 0) _exit(126);
        char* const args[] = {
            const_cast<char*>(executable.c_str()),
            const_cast<char*>("data/dqix.nds"),
            const_cast<char*>("data/code"),
            const_cast<char*>("0"),
            const_cast<char*>("1"),
            const_cast<char*>("dqix.sav"),
            nullptr
        };
        ::execv(executable.c_str(), args);
        // After fork, avoid stdio and allocation before _exit; the parent reports
        // an exit code of 127 if exec could not start the game.
        _exit(127);
    }

    state.game_pid = pid;
    state.child_result.clear();
    state.message = "Game started (PID " + std::to_string(pid) + "). The launcher can remain open.";
    state.message_is_error = false;
}

void update_child_status(LauncherState& state) {
    if (state.game_pid <= 0) return;
    int status = 0;
    const pid_t result = ::waitpid(state.game_pid, &status, WNOHANG);
    if (result == 0) return;
    if (result < 0) {
        if (errno != EINTR) {
            state.message = "Could not check game process: " + std::string(std::strerror(errno));
            state.message_is_error = true;
            state.game_pid = -1;
        }
        return;
    }
    if (WIFEXITED(status))
        state.child_result = "Game exited with code " + std::to_string(WEXITSTATUS(status)) + ".";
    else if (WIFSIGNALED(status))
        state.child_result = "Game stopped by signal " + std::to_string(WTERMSIG(status)) + ".";
    else
        state.child_result = "Game process ended.";
    state.game_pid = -1;
    state.message = state.child_result;
    state.message_is_error = false;
}

void draw_page_title(const char* title, const char* description) {
    ImGui::TextUnformatted(title);
    ImGui::Separator();
    ImGui::TextWrapped("%s", description);
    ImGui::Spacing();
}

void draw_game_page(LauncherState& state) {
    draw_page_title("Game", "Choose a built game folder, configure options, then launch the native Linux executable.");
    ImGui::TextWrapped("Game directory: %s", state.game_dir.string().c_str());
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputText("Game folder path", state.game_dir_input.data(), state.game_dir_input.size());
    if (ImGui::Button("Use this folder")) apply_game_directory(state);
    ImGui::SameLine();
    if (ImGui::Button("Reload settings")) load_settings(state);

    ImGui::Spacing();
    const bool executable = path_has_file(state.game_dir / "dqix") &&
                            ::access((state.game_dir / "dqix").c_str(), X_OK) == 0;
    const bool rom = path_has_file(state.game_dir / "data" / "dqix.nds");
    std::error_code ec;
    const bool code = fs::is_directory(state.game_dir / "data" / "code", ec);
    ImGui::BulletText("Game executable: %s", executable ? "ready" : "missing");
    ImGui::BulletText("Local ROM copy: %s", rom ? "found in the selected game folder" : "not found");
    ImGui::BulletText("Generated game code: %s", code ? "found" : "not found");
    ImGui::Spacing();
    ImGui::TextWrapped("The launcher expects the same game folder layout produced by the current build script. It does not include a ROM or generated game.");
    static const char* const languages[] = {"english", "french", "german", "italian", "spanish"};
    combo_setting(state, "Language (European ROM)", "language", "english", languages, 5);
    checkbox_setting(state, "Unlock downloadable quests and guests", "downloadable_content");
    ImGui::TextDisabled("The USA version runs in English; the language selector applies to the European version.");
    ImGui::Spacing();
    if (state.game_pid > 0) {
        ImGui::Text("Game running (PID %ld)", static_cast<long>(state.game_pid));
    } else {
        ImGui::BeginDisabled(!game_ready(state));
        if (ImGui::Button("Play now", ImVec2(150.0f, 42.0f))) launch_game(state);
        ImGui::EndDisabled();
        if (!game_ready(state))
            ImGui::TextDisabled("Build the game first; the Play button becomes available when all required files exist.");
    }
}

void draw_graphics_page(LauncherState& state) {
    static const char* const modes[] = {"classic", "enhanced"};
    static const char* const scales[] = {"auto", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12"};
    static const char* const outlines[] = {"thin", "medium", "ds"};
    draw_page_title("Graphics", "Values are saved to settings.ini. The runtime can still report an option unavailable if its renderer does not support it.");
    combo_setting(state, "Gameplay mode (next launch)", "mode", "classic", modes, 2);
    combo_setting(state, "3D render scale", "render_scale", "auto", scales, 13);
    checkbox_setting(state, "Smooth textures", "smooth_textures");
    combo_setting(state, "Character outlines", "outlines", "thin", outlines, 3);
    checkbox_setting(state, "Soft shadows and glows", "soft_shadows", true);
    checkbox_setting(state, "Smooth shading", "smooth_shading");
    checkbox_setting(state, "HD text", "hd_text");
    checkbox_setting(state, "HD text shadows", "hd_text_shadows");
}

void draw_display_page(LauncherState& state) {
    static const char* const layouts[] = {"stacked", "side_by_side", "corner", "wide", "wide_dynamic", "two_windows"};
    static const char* const backlights[] = {"low", "medium", "high", "max"};
    draw_page_title("Screen & Display", "Some display settings may require restarting the game or may depend on its existing SDL2/OpenGL path.");
    slider_setting(state, "Window scale", "window_scale", 2, 1, 8, "%dx");
    combo_setting(state, "Layout", "layout", "stacked", layouts, 6);
    checkbox_setting(state, "Fullscreen", "fullscreen");
    checkbox_setting(state, "Integer pixel scaling", "integer_scaling");
    checkbox_setting(state, "DS Lite screen filter", "dslite_filter", true);
    checkbox_setting(state, "LCD colours", "lcd_colors", true);
    checkbox_setting(state, "Pixel grid", "pixel_grid", true);
    checkbox_setting(state, "Motion blur", "motion_blur", true);
    combo_setting(state, "Backlight", "backlight", "max", backlights, 4);
    checkbox_setting(state, "Show console shell", "console", true);
    checkbox_setting(state, "Real screen gap", "screen_gap", true);
}

void draw_audio_page(LauncherState& state) {
    draw_page_title("Audio & Music", "The game reads these values at startup. Music-pack files remain in the player's game folder.");
    slider_setting(state, "Game volume", "volume", 100, 0, 100, "%d%%");
    checkbox_setting(state, "Mute", "mute");
    checkbox_setting(state, "Replace music with music packs", "music_packs", true);
    checkbox_setting(state, "Show current song name", "music_song_names");
    slider_setting(state, "Music-pack volume", "music_pack_volume", 70, 0, 200, "%d%%");
}

void draw_controls_page() {
    draw_page_title("Controls", "Keyboard and controller remapping is not wired into this standalone launcher yet.");
    ImGui::TextWrapped("The current runtime owns SDL2 input dispatch and controls.ini. This page does not pretend to apply key changes. The next integration step must provide a supported Linux bridge while leaving the Windows menu and source files untouched.");
}

void draw_mods_page(LauncherState& state) {
    draw_page_title("Mods", "These toggles update settings.ini only. The game must have the corresponding assets available to show an effect.");
    checkbox_setting(state, "Texture packs", "texture_packs", true);
    checkbox_setting(state, "Upscale 3D textures", "upscale_textures");
    checkbox_setting(state, "Music packs", "music_packs", true);
    checkbox_setting(state, "Display song names", "music_song_names");
    std::error_code textures_error, music_error;
    const bool textures = fs::is_directory(state.game_dir / "textures", textures_error);
    const bool music = fs::is_directory(state.game_dir / "music", music_error);
    ImGui::BulletText("textures/ folder: %s", textures ? "detected" : "not detected");
    ImGui::BulletText("music/ folder: %s", music ? "detected" : "not detected");
}

void draw_wireless_page(LauncherState& state) {
    draw_page_title("Wireless & Online", "Network settings are saved for the existing runtime. Server reachability is not tested here.");
    checkbox_setting(state, "Enable wireless/network features", "wireless", true);
    ImGui::SetNextItemWidth(240.0f);
    if (ImGui::InputText("WFC server or DNS", state.wfc_server.data(), state.wfc_server.size())) {
        state.message = "Server value changed in the editor; click Save server to persist it.";
        state.message_is_error = false;
    }
    if (ImGui::Button("Save server")) commit_setting(state, "wfc_server", state.wfc_server.data());
    ImGui::SameLine();
    ImGui::TextDisabled("e.g. builtin or a DNS hostname/address");
    ImGui::SetNextItemWidth(240.0f);
    if (ImGui::InputText("Player name", state.player_name.data(), state.player_name.size())) {
        state.message = "Player name changed in the editor; click Save player name to persist it.";
        state.message_is_error = false;
    }
    if (ImGui::Button("Save player name")) commit_setting(state, "player_name", state.player_name.data());
    slider_setting(state, "Host port", "session_port", 27900, 1, 65535);
}

void draw_system_page(LauncherState& state, VkPhysicalDevice physical_device, float fps) {
    draw_page_title("System & Diagnostics", "Diagnostics shown here describe the launcher process. They do not imply gameplay Vulkan support.");
    ImGui::TextWrapped("Game folder: %s", state.game_dir.string().c_str());
    ImGui::TextWrapped("Settings file: %s", state.settings_path.string().c_str());
    ImGui::TextWrapped("Launcher log: %s", g_dqix_log_path.c_str());
    VkPhysicalDeviceProperties properties{};
    if (physical_device != VK_NULL_HANDLE) {
        vkGetPhysicalDeviceProperties(physical_device, &properties);
        ImGui::TextWrapped("UI GPU: %s", properties.deviceName);
        ImGui::Text("Vulkan API: %u.%u.%u",
                    VK_VERSION_MAJOR(properties.apiVersion),
                    VK_VERSION_MINOR(properties.apiVersion),
                    VK_VERSION_PATCH(properties.apiVersion));
    } else {
        ImGui::TextWrapped("UI GPU: unavailable");
    }
    ImGui::Text("UI frame rate: %.1f FPS", fps);
    ImGui::Separator();
    ImGui::TextWrapped("Gameplay currently uses the existing SDL2/OpenGL renderer with a software fallback. Vulkan here is for the launcher's user interface only.");
    ImGui::TextWrapped("The game settings path is still the runtime's current working directory plus settings.ini. Play starts dqix with the selected game folder as its working directory.");
}

void draw_launcher_ui(LauncherState& state, VkPhysicalDevice physical_device, float fps) {
    update_child_status(state);

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::Begin("DQIX Enhanced Linux Launcher", nullptr, flags);

    ImGui::BeginChild("sidebar", ImVec2(210.0f, -64.0f), true);
    ImGui::TextUnformatted("DQIX Enhanced");
    ImGui::TextDisabled("Linux launcher preview");
    ImGui::Separator();
    const std::pair<Page, const char*> pages[] = {
        {Page::Game, "Game"},
        {Page::Graphics, "Graphics"},
        {Page::Display, "Screen & Display"},
        {Page::Audio, "Audio & Music"},
        {Page::Controls, "Controls"},
        {Page::Mods, "Mods"},
        {Page::Wireless, "Wireless & Online"},
        {Page::System, "System"}
    };
    for (const auto& item : pages) {
        if (ImGui::Selectable(item.second, state.page == item.first, 0, ImVec2(-1.0f, 34.0f)))
            state.page = item.first;
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextWrapped("New Linux UI: SDL3 + Dear ImGui + Vulkan.");
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("main_content", ImVec2(0.0f, -64.0f), false);
    switch (state.page) {
    case Page::Game: draw_game_page(state); break;
    case Page::Graphics: draw_graphics_page(state); break;
    case Page::Display: draw_display_page(state); break;
    case Page::Audio: draw_audio_page(state); break;
    case Page::Controls: draw_controls_page(); break;
    case Page::Mods: draw_mods_page(state); break;
    case Page::Wireless: draw_wireless_page(state); break;
    case Page::System: draw_system_page(state, physical_device, fps); break;
    }
    ImGui::EndChild();
    ImGui::Separator();
    if (state.message_is_error) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.35f, 0.30f, 1.0f));
        ImGui::TextWrapped("%s", state.message.c_str());
        ImGui::PopStyleColor();
    } else {
        ImGui::TextWrapped("%s", state.message.c_str());
    }
    ImGui::End();
}

} // namespace

// Main code
int main(int argc, char** argv)
{
    LauncherState launcher;
    std::string argument_error;
    if (!parse_arguments(argc, argv, launcher, argument_error)) {
        if (!argument_error.empty()) std::fprintf(stderr, "DQIX Launcher: %s\n", argument_error.c_str());
        return argument_error.empty() ? 0 : 2;
    }
    g_dqix_log_path = (launcher.game_dir / "launcher.log").string();
    {
        std::error_code log_dir_error;
        fs::create_directories(launcher.game_dir, log_dir_error);
        launcher_log("[launcher] DQIX Enhanced Linux launcher preview starting.\n");
        launcher_log("[launcher] Game directory: %s\n", launcher.game_dir.string().c_str());
        launcher_log("[launcher] Log path: %s\n", g_dqix_log_path.c_str());
    }
    load_settings(launcher);
    if (launcher.message_is_error) launcher_log("[settings] %s\n", launcher.message.c_str());

    // Setup SDL
    // [If using SDL_MAIN_USE_CALLBACKS: all code below until the main loop starts would likely be your SDL_AppInit() function]
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        launcher_log("[sdl3] SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    // Create window with Vulkan graphics context
    float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    SDL_WindowFlags window_flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    SDL_Window* window = SDL_CreateWindow("DQIX Enhanced - Linux Launcher", (int)(1280 * main_scale), (int)(800 * main_scale), window_flags);
    if (window == nullptr)
    {
        launcher_log("[sdl3] SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    ImVector<const char*> extensions;
    {
        uint32_t sdl_extensions_count = 0;
        const char* const* sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&sdl_extensions_count);
        if (!sdl_extensions || sdl_extensions_count == 0) {
            launcher_log("[vulkan] SDL3 did not provide the Vulkan instance extensions required for a window surface: %s\n", SDL_GetError());
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }
        for (uint32_t n = 0; n < sdl_extensions_count; n++)
            extensions.push_back(sdl_extensions[n]);
    }
    SetupVulkan(extensions);

    // Create Window Surface
    VkSurfaceKHR surface;
    VkResult err;
    if (SDL_Vulkan_CreateSurface(window, g_Instance, g_Allocator, &surface) == 0)
    {
        launcher_log("[vulkan] Failed to create SDL window surface: %s\n", SDL_GetError());
        CleanupVulkan();
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Create Framebuffers
    int w, h;
    SDL_GetWindowSizeInPixels(window, &w, &h);
    ImGui_ImplVulkanH_Window* wd = &g_MainWindowData;
    SetupVulkanWindow(wd, surface, w, h);
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();
    //style.FontSizeBase = 16.0f;           // Default to 13.0f when unspecified, which allows crisp legacy font for non-4K screens.
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;

    // Setup scaling
    style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
    style.FontScaleDpi = main_scale;        // Set initial font scale. (in docking branch: using io.ConfigDpiScaleFonts=true automatically overrides this for every window depending on the current monitor)

    // Setup Platform/Renderer backends
    if (!ImGui_ImplSDL3_InitForVulkan(window)) {
        launcher_log("[imgui] Could not initialize Dear ImGui's SDL3 platform backend.\n");
        CleanupVulkanWindow(&g_MainWindowData);
        CleanupVulkan();
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    ImGui_ImplVulkan_InitInfo init_info = {};
    //init_info.ApiVersion = VK_API_VERSION_1_3;              // Pass in your value of VkApplicationInfo::apiVersion, otherwise will default to header version.
    init_info.Instance = g_Instance;
    init_info.PhysicalDevice = g_PhysicalDevice;
    init_info.Device = g_Device;
    init_info.QueueFamily = g_QueueFamily;
    init_info.Queue = g_Queue;
    init_info.PipelineCache = g_PipelineCache;
    init_info.DescriptorPool = g_DescriptorPool;
    init_info.MinImageCount = g_MinImageCount;
    init_info.ImageCount = wd->ImageCount;
    init_info.Allocator = g_Allocator;
    init_info.PipelineInfoMain.RenderPass = wd->RenderPass;
    init_info.PipelineInfoMain.Subpass = 0;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.CheckVkResultFn = check_vk_result;
    if (!ImGui_ImplVulkan_Init(&init_info)) {
        launcher_log("[imgui] Could not initialize Dear ImGui's Vulkan renderer backend.\n");
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        CleanupVulkanWindow(&g_MainWindowData);
        CleanupVulkan();
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Load Fonts
    // - If fonts are not explicitly loaded, Dear ImGui will select an embedded font: either AddFontDefaultVector() or AddFontDefaultBitmap().
    //   This selection is based on (style.FontSizeBase * style.FontScaleMain * style.FontScaleDpi) reaching a threshold.
    // - You can load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - If a file cannot be loaded, AddFont functions will return a nullptr. Please handle those errors in your code (e.g. use an assertion, display an error and quit).
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use FreeType for higher quality font rendering.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefaultVector();
    //io.Fonts->AddFontDefaultBitmap();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf");
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf");
    //IM_ASSERT(font != nullptr);

    // Launcher window clear color and state are kept separate from the game renderer.
    ImVec4 clear_color = ImVec4(0.035f, 0.045f, 0.065f, 1.00f);

    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        // [If using SDL_MAIN_USE_CALLBACKS: call ImGui_ImplSDL3_ProcessEvent() from your SDL_AppEvent() function]
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT)
                done = true;
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window))
                done = true;
        }

        // [If using SDL_MAIN_USE_CALLBACKS: all code below would likely be your SDL_AppIterate() function]
        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)
        {
            SDL_Delay(10);
            continue;
        }

        // Resize swap chain?
        int fb_width, fb_height;
        SDL_GetWindowSizeInPixels(window, &fb_width, &fb_height);
        if (fb_width > 0 && fb_height > 0 && (g_SwapChainRebuild || g_MainWindowData.Width != fb_width || g_MainWindowData.Height != fb_height))
        {
            ImGui_ImplVulkan_SetMinImageCount(g_MinImageCount);
            ImGui_ImplVulkanH_CreateOrResizeWindow(g_Instance, g_PhysicalDevice, g_Device, wd, g_QueueFamily, g_Allocator, fb_width, fb_height, g_MinImageCount, 0);
            g_MainWindowData.FrameIndex = 0;
            g_SwapChainRebuild = false;
        }

        // Start the Dear ImGui frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // Draw the native Linux launcher, not the upstream demo windows.
        draw_launcher_ui(launcher, g_PhysicalDevice, io.Framerate);

        // Rendering
        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();
        const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
        if (!is_minimized)
        {
            wd->ClearValue.color.float32[0] = clear_color.x * clear_color.w;
            wd->ClearValue.color.float32[1] = clear_color.y * clear_color.w;
            wd->ClearValue.color.float32[2] = clear_color.z * clear_color.w;
            wd->ClearValue.color.float32[3] = clear_color.w;
            FrameRender(wd, draw_data);
            FramePresent(wd);
        }
    }

    // Cleanup
    // [If using SDL_MAIN_USE_CALLBACKS: all code below would likely be your SDL_AppQuit() function]
    err = vkDeviceWaitIdle(g_Device);
    check_vk_result(err);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    CleanupVulkanWindow(&g_MainWindowData);
    CleanupVulkan();

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
