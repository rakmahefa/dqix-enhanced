#include <SDL.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_sdlrenderer2.h>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <signal.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

namespace fs = std::filesystem;

namespace {

constexpr int kWidth = 1280;
constexpr int kHeight = 820;
const ImVec4 kWindow(9.0f / 255.0f, 13.0f / 255.0f, 36.0f / 255.0f, 0.96f);
const ImVec4 kSolid(12.0f / 255.0f, 17.0f / 255.0f, 46.0f / 255.0f, 1.0f);
const ImVec4 kBorder(242.0f / 255.0f, 243.0f / 255.0f, 252.0f / 255.0f, 0.94f);
const ImVec4 kMuted(158.0f / 255.0f, 168.0f / 255.0f, 204.0f / 255.0f, 1.0f);
const ImVec4 kGold(244.0f / 255.0f, 203.0f / 255.0f, 82.0f / 255.0f, 1.0f);
const ImVec4 kSky(118.0f / 255.0f, 160.0f / 255.0f, 1.0f, 1.0f);
const ImVec4 kGood(112.0f / 255.0f, 222.0f / 255.0f, 142.0f / 255.0f, 1.0f);
const ImVec4 kWarn(246.0f / 255.0f, 182.0f / 255.0f, 64.0f / 255.0f, 1.0f);
const ImVec4 kBad(238.0f / 255.0f, 96.0f / 255.0f, 96.0f / 255.0f, 1.0f);

enum class Page : int { Rom = 0, Tools, Options, Build, Play };

struct Tool {
    std::string name;
    std::string purpose;
    bool ready = false;
    bool included = false;
    std::string detail;
};

std::string trim(std::string value) {
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r' ||
                              value.back() == ' ' || value.back() == '\t'))
        value.pop_back();
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    return value.substr(first);
}

std::string readPipe(const std::string &command) {
    FILE *pipe = popen(command.c_str(), "r");
    if (!pipe) return {};
    std::string result;
    std::array<char, 512> buffer{};
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe))
        result += buffer.data();
    pclose(pipe);
    return trim(result);
}

bool executableOnPath(const std::string &name) {
    const char *pathEnv = std::getenv("PATH");
    if (!pathEnv) return false;
    std::string path(pathEnv);
    size_t start = 0;
    while (start <= path.size()) {
        const size_t end = path.find(':', start);
        const std::string dir = path.substr(start, end == std::string::npos ? end : end - start);
        if (!dir.empty() && access((fs::path(dir) / name).c_str(), X_OK) == 0) return true;
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return false;
}

fs::path executablePath() {
    std::array<char, 4096> buffer{};
    const ssize_t size = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (size <= 0) return fs::current_path() / "dqix-launcher";
    buffer[static_cast<size_t>(size)] = '\0';
    return fs::path(buffer.data());
}

fs::path findSourceRoot(const fs::path &exeDir, const fs::path &payload) {
    const std::array<fs::path, 3> candidates = {
        payload / "source",
        exeDir / "source",
        fs::current_path()
    };
    for (const auto &candidate : candidates) {
        if (fs::exists(candidate / "Makefile") && fs::exists(candidate / "recomp/build.py") &&
            fs::exists(candidate / "runtime/version.h"))
            return fs::weakly_canonical(candidate);
    }
    fs::path current = exeDir;
    for (int i = 0; i < 7 && !current.empty(); ++i) {
        if (fs::exists(current / "Makefile") && fs::exists(current / "recomp/build.py") &&
            fs::exists(current / "runtime/version.h"))
            return fs::weakly_canonical(current);
        if (current == current.root_path()) break;
        current = current.parent_path();
    }
    return {};
}

std::string chooseFile() {
    // Dialog tools are optional. Drag-and-drop and the editable path remain available.
    return readPipe("zenity --file-selection --title='Choose your Dragon Quest IX ROM' "
                    "--file-filter='Nintendo DS ROM (*.nds) | *.nds' 2>/dev/null "
                    "|| kdialog --getopenfilename \"$HOME\" '*.nds' 2>/dev/null");
}

std::string chooseDirectory() {
    return readPipe("zenity --file-selection --directory --title='Choose a build workspace' 2>/dev/null "
                    "|| kdialog --getexistingdirectory \"$HOME\" 2>/dev/null");
}

std::string distroId() {
    std::ifstream file("/etc/os-release");
    for (std::string line; std::getline(file, line);) {
        if (line.rfind("ID=", 0) == 0) return trim(line.substr(3)).empty() ? "" : trim(line.substr(3));
    }
    return {};
}

std::string installCommand() {
    const std::string distro = distroId();
    if (distro == "debian" || distro == "ubuntu" || distro == "linuxmint" ||
        distro == "pop" || distro == "elementary")
        return "apt-get update && apt-get install -y build-essential make libgl-dev python3 python3-yaml libsdl2-dev";
    if (distro == "fedora" || distro == "rhel" || distro == "rocky" || distro == "almalinux")
        return "dnf install -y gcc-c++ make mesa-libGL-devel python3 python3-pyyaml SDL2-devel";
    if (distro == "arch" || distro == "manjaro" || distro == "endeavouros")
        return "pacman -S --needed --noconfirm base-devel mesa python python-yaml sdl2";
    return {};
}

std::string defaultWorkspace() {
    const char *xdg = std::getenv("XDG_DATA_HOME");
    const char *home = std::getenv("HOME");
    fs::path root = xdg && *xdg ? fs::path(xdg) : fs::path(home ? home : ".") / ".local/share";
    return (root / "dqix-enhanced" / "workspace").string();
}

std::string pageName(Page page) {
    switch (page) {
    case Page::Rom: return "Your ROM";
    case Page::Tools: return "Tools";
    case Page::Options: return "Options";
    case Page::Build: return "Build";
    case Page::Play: return "Play";
    }
    return {};
}

bool gameArtifactsReady(const fs::path &workspace) {
    const fs::path game = workspace / "game";
    const fs::path executable = game / "dqix";
    std::error_code ec;
    if (!fs::is_regular_file(executable, ec) || ec || access(executable.c_str(), X_OK) != 0)
        return false;
    if (!fs::is_regular_file(game / "dqixcore.so", ec) || ec) return false;
    if (!fs::is_regular_file(game / "play.sh", ec) || ec) return false;
    if (!fs::is_regular_file(game / "data/dqix.nds", ec) || ec) return false;
    if (!fs::is_regular_file(game / "data/code/arm9/arm9.bin", ec) || ec) return false;
    return fs::is_regular_file(game / "data/code/arm7/arm7.bin", ec) && !ec;
}

class Launcher {
public:
    Launcher()
        : m_exeDir(executablePath().parent_path()),
          m_payload(m_exeDir / "payload"),
          m_source(findSourceRoot(m_exeDir, m_payload)),
          m_python((m_payload / "tools/python/bin/python3").string()),
          m_workspace(defaultWorkspace()) {
        if (!fs::exists(m_python)) m_python = "python3";
        m_payloadAvailable = fs::exists(m_payload / "payload.json") &&
                             fs::exists(m_payload / "include/SDL2/SDL.h") &&
                             fs::exists(m_payload / "lib/libSDL2-2.0.so.0");
        openLogFile(m_workspace, false);
        addLog("Launcher started.");
        {
            std::lock_guard<std::mutex> lock(m_logMutex);
            m_log.clear();
        }
        if (m_source.empty())
            addLog("ERROR: The source payload was not found beside the launcher.");
    }

    ~Launcher() {
        m_buildCancelled = true;
        if (m_childPid > 0) kill(-m_childPid, SIGTERM);
        if (m_worker.joinable()) m_worker.join();
        if (m_installPid > 0) kill(m_installPid, SIGTERM);
        if (m_installWorker.joinable()) m_installWorker.join();
    }

    bool initialize(bool hidden = false) {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
            setError(std::string("SDL could not start: ") + SDL_GetError());
            return false;
        }
        m_sdlInitialized = true;
        SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
        const Uint32 flags = (hidden ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN) |
                             SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
        m_window = SDL_CreateWindow("DQIX Enhanced — Builder",
                                    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                    kWidth, kHeight, flags);
        if (!m_window) {
            setError(std::string("Could not create the launcher window: ") + SDL_GetError());
            return false;
        }
        m_renderer = SDL_CreateRenderer(m_window, -1, hidden ? SDL_RENDERER_SOFTWARE :
                                        (SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC));
        if (!m_renderer)
            m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_SOFTWARE);
        if (!m_renderer) {
            setError(std::string("Could not create the launcher renderer: ") + SDL_GetError());
            return false;
        }
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        m_imguiInitialized = true;
        ImGuiIO &io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.IniFilename = nullptr;
        applyStyle();
        if (!ImGui_ImplSDL2_InitForSDLRenderer(m_window, m_renderer)) {
            setError("Dear ImGui's SDL2 platform backend could not initialize.");
            return false;
        }
        m_platformBackendInitialized = true;
        if (!ImGui_ImplSDLRenderer2_Init(m_renderer)) {
            setError("Dear ImGui's SDL2 renderer backend could not initialize.");
            return false;
        }
        m_rendererBackendInitialized = true;
        SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
        return true;
    }

    void shutdown() {
        m_buildCancelled = true;
        if (m_childPid > 0) kill(-m_childPid, SIGTERM);
        if (m_installPid > 0) kill(m_installPid, SIGTERM);
        if (m_worker.joinable()) m_worker.join();
        if (m_installWorker.joinable()) m_installWorker.join();
        {
            std::lock_guard<std::mutex> lock(m_logMutex);
            if (m_logFile.is_open()) {
                m_logFile.flush();
                m_logFile.close();
            }
        }
        if (m_rendererBackendInitialized) ImGui_ImplSDLRenderer2_Shutdown();
        if (m_platformBackendInitialized) ImGui_ImplSDL2_Shutdown();
        if (m_imguiInitialized) ImGui::DestroyContext();
        if (m_renderer) SDL_DestroyRenderer(m_renderer);
        if (m_window) SDL_DestroyWindow(m_window);
        m_renderer = nullptr;
        m_window = nullptr;
        if (m_sdlInitialized) SDL_Quit();
        m_sdlInitialized = false;
    }

    void run() {
        bool running = true;
        while (running) {
            SDL_Event event;
            if (m_recheckToolsRequested.exchange(false)) m_tools = checkTools();
            if (m_page == Page::Tools && m_tools.empty() && !m_preview) m_tools = checkTools();
            while (SDL_PollEvent(&event)) {
                ImGui_ImplSDL2_ProcessEvent(&event);
                if (event.type == SDL_QUIT) running = false;
                if (event.type == SDL_DROPFILE && event.drop.file) {
                    if (!m_buildRunning) {
                        m_rom = fs::absolute(event.drop.file).string();
                        std::snprintf(m_romBuffer.data(), m_romBuffer.size(), "%s", m_rom.c_str());
                        m_page = Page::Tools;
                    }
                    SDL_free(event.drop.file);
                }
                if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                    if (m_page != Page::Rom && !m_buildRunning.load()) m_page = static_cast<Page>(std::max(0, static_cast<int>(m_page) - 1));
                }
            }
            drawFrame();
        }
    }

    bool makePreview(const fs::path &directory) {
        std::error_code ec;
        fs::create_directories(directory, ec);
        if (ec) {
            std::fprintf(stderr, "launcher: cannot create preview directory: %s\n", ec.message().c_str());
            return false;
        }
        m_preview = true;
        m_rom.clear();
        m_workspace = (fs::current_path() / "preview-workspace").string();
        openLogFile(m_workspace, false);
        m_tools = {
            {"C compiler", "Compiles the native game and runtime", false, false, "not found"},
            {"GNU Make", "Runs the build rules", false, false, "not found"},
            {"OpenGL headers", "Compile the enhanced renderer", false, false, "not found"},
            {"Python + PyYAML", "Translates ARM9 and ARM7 code", true, true, "Python 3.12.15 · PyYAML 6.0.2 · bundled"},
            {"SDL2", "Window, sound and input", true, true, "SDL2 · bundled"}
        };

        struct Snapshot { Page page; const char *name; bool failed; };
        const std::array<Snapshot, 7> pages = {{
            {Page::Rom, "launcher-disc-empty.bmp", false},
            {Page::Tools, "launcher-tools-missing.bmp", false},
            {Page::Tools, "launcher-tools-ready.bmp", true},
            {Page::Options, "launcher-options.bmp", false},
            {Page::Build, "launcher-building.bmp", false},
            {Page::Build, "launcher-build-failed.bmp", true},
            {Page::Play, "launcher-play.bmp", false}
        }};
        bool ok = true;
        for (const auto &snapshot : pages) {
            m_page = snapshot.page;
            m_previewFailed = snapshot.failed && snapshot.page == Page::Build;
            m_previewToolsReady = snapshot.page == Page::Tools && snapshot.failed;
            if (m_previewToolsReady) {
                for (auto &tool : m_tools) {
                    tool.ready = true;
                    if (!tool.included) tool.detail = "found on this computer";
                }
            }
            if (snapshot.page == Page::Build) {
                m_buildFinished = true;
                m_buildSuccess = !m_previewFailed;
                m_buildRunning = !m_previewFailed;
                m_stage = m_previewFailed ? 5 : 4;
                m_log = m_previewFailed
                    ? std::vector<std::string>{"== Checking ROM", "== Extracting game code", "== Recompiling ARM9",
                                               "== Recompiling ARM7", "== Compiling game", "error: compiler returned exit code 1"}
                    : std::vector<std::string>{"== Checking ROM", "== Extracting game code", "== Recompiling ARM9",
                                               "== Recompiling ARM7", "== Compiling game", "== Assembling game folder"};
            }
            drawFrame(false);
            SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, kWidth, kHeight, 32, SDL_PIXELFORMAT_ARGB8888);
            if (!surface) { ok = false; continue; }
            if (SDL_RenderReadPixels(m_renderer, nullptr, SDL_PIXELFORMAT_ARGB8888,
                                     surface->pixels, surface->pitch) != 0) {
                std::fprintf(stderr, "launcher: screenshot readback failed: %s\n", SDL_GetError());
                ok = false;
            } else {
                const fs::path path = directory / snapshot.name;
                if (SDL_SaveBMP(surface, path.c_str()) != 0) {
                    std::fprintf(stderr, "launcher: screenshot write failed: %s\n", SDL_GetError());
                    ok = false;
                }
            }
            SDL_FreeSurface(surface);
        }
        m_preview = false;
        m_buildRunning = false;
        return ok;
    }

private:
    fs::path m_exeDir;
    fs::path m_payload;
    fs::path m_source;
    std::string m_python;
    std::string m_workspace;
    std::string m_rom;
    std::array<char, 4096> m_romBuffer{};
    SDL_Window *m_window = nullptr;
    SDL_Renderer *m_renderer = nullptr;
    Page m_page = Page::Rom;
    bool m_payloadAvailable = false;
    bool m_preview = false;
    bool m_previewFailed = false;
    bool m_previewToolsReady = false;
    std::atomic<bool> m_buildRunning{false};
    std::atomic<bool> m_buildFinished{false};
    std::atomic<bool> m_buildSuccess{false};
    std::atomic<bool> m_buildCancelled{false};
    std::atomic<bool> m_installRunning{false};
    int m_exitCode = 0;
    int m_jobs = std::max(1, static_cast<int>(std::thread::hardware_concurrency()));
    std::atomic<int> m_stage{0};
    std::string m_error;
    std::vector<Tool> m_tools;
    std::vector<std::string> m_log;
    std::mutex m_logMutex;
    std::mutex m_errorMutex;
    std::ofstream m_logFile;
    std::string m_logFeedback;
    std::thread m_worker;
    std::thread m_installWorker;
    std::atomic<pid_t> m_childPid{0};
    std::atomic<pid_t> m_installPid{0};
    std::atomic<bool> m_recheckToolsRequested{false};
    std::atomic<bool> m_sdlInitialized{false};
    std::atomic<bool> m_imguiInitialized{false};
    std::atomic<bool> m_platformBackendInitialized{false};
    std::atomic<bool> m_rendererBackendInitialized{false};

    void setError(const std::string &value) {
        std::lock_guard<std::mutex> lock(m_errorMutex);
        m_error = value;
    }

    void clearError() { setError({}); }

    bool openLogFile(const std::string &workspace, bool truncate) {
        if (workspace.empty()) {
            setError("The workspace path is empty; build logs cannot be saved.");
            return false;
        }
        std::error_code ec;
        fs::create_directories(fs::path(workspace), ec);
        if (ec) {
            setError("Cannot create the workspace for log.txt: " + ec.message());
            return false;
        }
        const fs::path logPath = fs::path(workspace) / "log.txt";
        std::lock_guard<std::mutex> lock(m_logMutex);
        if (m_logFile.is_open()) {
            m_logFile.flush();
            m_logFile.close();
        }
        const auto mode = std::ios::out | (truncate ? std::ios::trunc : std::ios::app);
        m_logFile.open(logPath, mode);
        if (!m_logFile) {
            setError("Cannot write the build log: " + logPath.string());
            return false;
        }
        m_logFile << "\n=== DQIX Enhanced launcher session ===\n";
        m_logFile.flush();
        return true;
    }

    std::string errorCopy() {
        std::lock_guard<std::mutex> lock(m_errorMutex);
        return m_error;
    }

    void applyStyle() {
        ImGuiStyle &style = ImGui::GetStyle();
        style.WindowRounding = 19.0f;
        style.ChildRounding = 13.0f;
        style.FrameRounding = 8.0f;
        style.PopupRounding = 10.0f;
        style.ScrollbarRounding = 12.0f;
        style.GrabRounding = 8.0f;
        style.WindowPadding = ImVec2(0.0f, 0.0f);
        style.FramePadding = ImVec2(13.0f, 9.0f);
        style.ItemSpacing = ImVec2(10.0f, 9.0f);
        style.ItemInnerSpacing = ImVec2(8.0f, 7.0f);
        style.IndentSpacing = 22.0f;
        style.ScrollbarSize = 13.0f;
        style.WindowBorderSize = 0.0f;
        style.ChildBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;
        style.Colors[ImGuiCol_Text] = ImVec4(0.965f, 0.969f, 1.0f, 1.0f);
        style.Colors[ImGuiCol_TextDisabled] = kMuted;
        style.Colors[ImGuiCol_WindowBg] = kWindow;
        style.Colors[ImGuiCol_ChildBg] = kSolid;
        style.Colors[ImGuiCol_PopupBg] = kSolid;
        style.Colors[ImGuiCol_Border] = ImVec4(0.58f, 0.64f, 0.82f, 0.22f);
        style.Colors[ImGuiCol_FrameBg] = ImVec4(1.0f, 1.0f, 1.0f, 0.065f);
        style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.43f, 0.55f, 1.0f, 0.16f);
        style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.43f, 0.55f, 1.0f, 0.22f);
        style.Colors[ImGuiCol_Button] = ImVec4(1.0f, 1.0f, 1.0f, 0.075f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.43f, 0.55f, 1.0f, 0.22f);
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.43f, 0.55f, 1.0f, 0.30f);
        style.Colors[ImGuiCol_Header] = ImVec4(0.43f, 0.55f, 1.0f, 0.16f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.43f, 0.55f, 1.0f, 0.25f);
        style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.43f, 0.55f, 1.0f, 0.32f);
        style.Colors[ImGuiCol_Separator] = ImVec4(0.82f, 0.86f, 1.0f, 0.12f);
        style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0.10f);
        style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.68f, 0.74f, 0.91f, 0.25f);
        style.Colors[ImGuiCol_CheckMark] = kGold;
        style.Colors[ImGuiCol_SliderGrab] = kGold;
        style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(1.0f, 0.88f, 0.46f, 1.0f);
    }

    void addLog(const std::string &line) {
        std::string value = line;
        while (!value.empty() && (value.back() == '\n' || value.back() == '\r'))
            value.pop_back();
        if (value.empty()) return;
        std::lock_guard<std::mutex> lock(m_logMutex);
        m_log.push_back(value);
        if (m_logFile.is_open()) {
            m_logFile << value << '\n';
            m_logFile.flush();
        }
        const std::string stageLine = trim(value);
        if (stageLine.find("Checking ROM") != std::string::npos) m_stage = 0;
        else if (stageLine.find("Extracting") != std::string::npos) m_stage = 1;
        else if (stageLine.find("Mapping ROM region") != std::string::npos) m_stage = 2;
        else if (stageLine.find("Recompiling ARM9") != std::string::npos) m_stage = 3;
        else if (stageLine.find("Recompiling ARM7") != std::string::npos) m_stage = 4;
        else if (stageLine.find("Compiling") != std::string::npos) m_stage = 5;
        else if (stageLine.find("Assembling") != std::string::npos) m_stage = 6;
    }

    void copyBuildLog() {
        const auto lines = logCopy();
        if (lines.empty()) {
            m_logFeedback = "No build output to copy yet.";
            return;
        }
        std::string content;
        for (const auto &line : lines) {
            content += line;
            content.push_back('\n');
        }
        if (SDL_SetClipboardText(content.c_str()) == 0)
            m_logFeedback = "Build log copied to the clipboard.";
        else
            m_logFeedback = std::string("Could not copy the log: ") + SDL_GetError();
    }

    std::vector<std::string> logCopy() {
        std::lock_guard<std::mutex> lock(m_logMutex);
        return m_log;
    }

    void startBuild() {
        if (m_buildRunning) return;
        if (m_source.empty()) {
            setError("The source payload is missing. Reinstall or re-extract the Linux release.");
            addLog("ERROR: source payload is missing.");
            return;
        }
        std::error_code romError;
        if (m_rom.empty() || !fs::is_regular_file(m_rom, romError) || romError) {
            setError("Select an existing Dragon Quest IX .nds ROM before building.");
            addLog("ERROR: no readable ROM file is selected.");
            return;
        }
        const std::string compiler = cCompiler();
        if (compiler.empty() || !toolsReady()) {
            setError("Required build tools are missing. Open Tools and check the requirements.");
            addLog("ERROR: required build tools are missing.");
            m_page = Page::Tools;
            return;
        }
        if (m_worker.joinable()) m_worker.join();
        m_buildRunning = true;
        m_buildFinished = false;
        m_buildSuccess = false;
        m_buildCancelled = false;
        m_stage = 0;
        clearError();
        m_logFeedback.clear();
        m_page = Page::Build;
        std::error_code ec;
        fs::create_directories(m_workspace, ec);
        if (ec) {
            m_buildRunning = false;
            m_buildFinished = true;
            setError("Cannot create the workspace: " + ec.message());
            addLog("ERROR: " + errorCopy());
            return;
        }
        const std::string source = m_source.string();
        const std::string rom = fs::absolute(m_rom).string();
        const std::string workspace = fs::absolute(m_workspace).string();
        if (!openLogFile(workspace, false)) {
            m_buildRunning = false;
            m_buildFinished = true;
            return;
        }
        {
            std::lock_guard<std::mutex> lock(m_logMutex);
            m_log.clear();
        }
        addLog("===== Build started =====");
        const std::string python = m_python;
        const std::string payload = m_payloadAvailable ? m_payload.string() : "";
        const std::string jobs = std::to_string(m_jobs);
        m_worker = std::thread([this, source, rom, workspace, python, payload, jobs, compiler]() {
            int pipefd[2];
            if (pipe(pipefd) != 0) {
                const std::string message = std::string("Could not open the build log pipe: ") + std::strerror(errno);
                addLog("ERROR: " + message);
                setError(message);
                m_buildRunning = false;
                m_buildFinished = true;
                return;
            }
            std::vector<std::string> args = {
                "make", "-C", source, "-f", "Makefile", "build",
                "ROM=" + rom, "WORKSPACE=" + workspace, "PYTHON=" + python,
                "JOBS=" + jobs, "CC=" + compiler
            };
            if (!payload.empty()) args.push_back("PAYLOAD=" + payload);
            std::vector<char *> argv;
            for (auto &arg : args) argv.push_back(arg.data());
            argv.push_back(nullptr);

            posix_spawn_file_actions_t actions;
            posix_spawnattr_t attributes;
            posix_spawn_file_actions_init(&actions);
            posix_spawn_file_actions_addclose(&actions, pipefd[0]);
            posix_spawn_file_actions_adddup2(&actions, pipefd[1], STDOUT_FILENO);
            posix_spawn_file_actions_adddup2(&actions, pipefd[1], STDERR_FILENO);
            posix_spawn_file_actions_addclose(&actions, pipefd[1]);
            posix_spawnattr_init(&attributes);
            posix_spawnattr_setpgroup(&attributes, 0);
            posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP);
            pid_t pid = 0;
            const int spawnError = posix_spawnp(&pid, argv[0], &actions, &attributes, argv.data(), environ);
            posix_spawn_file_actions_destroy(&actions);
            posix_spawnattr_destroy(&attributes);
            close(pipefd[1]);
            if (spawnError != 0) {
                close(pipefd[0]);
                const std::string message = std::string("Could not start the build: ") + std::strerror(spawnError);
                addLog("ERROR: " + message);
                setError(message);
                m_buildRunning = false;
                m_buildFinished = true;
                return;
            }
            m_childPid = pid;
            if (m_buildCancelled.load()) kill(-pid, SIGTERM);
            FILE *stream = fdopen(pipefd[0], "r");
            if (stream) {
                char *line = nullptr;
                size_t capacity = 0;
                ssize_t count = 0;
                while ((count = getline(&line, &capacity, stream)) != -1)
                    addLog(std::string(line, static_cast<size_t>(count)));
                free(line);
                fclose(stream);
            } else {
                close(pipefd[0]);
            }
            int status = 0;
            pid_t waited = -1;
            do {
                waited = waitpid(pid, &status, 0);
            } while (waited < 0 && errno == EINTR);
            m_childPid = 0;
            if (waited < 0) {
                m_exitCode = 127;
                setError(std::string("Could not collect the build process status: ") + std::strerror(errno));
                addLog("ERROR: " + errorCopy());
            } else {
                m_exitCode = WIFEXITED(status) ? WEXITSTATUS(status) :
                    128 + (WIFSIGNALED(status) ? WTERMSIG(status) : 0);
            }
            const bool cancelled = m_buildCancelled.load();
            const bool artifactsReady = gameArtifactsReady(workspace);
            m_buildSuccess = !cancelled && m_exitCode == 0 && artifactsReady;
            m_buildFinished = true;
            m_buildRunning = false;
            if (cancelled) {
                setError("The build was cancelled. The partial output can be safely rebuilt.");
                addLog("Build cancelled by the user.");
            } else if (m_exitCode == 0 && !artifactsReady) {
                setError("Make finished without producing all required game files. See log.txt.");
                addLog("ERROR: build exited successfully but the game output is incomplete.");
            } else if (!m_buildSuccess) {
                setError("The build failed. Open log.txt for the complete compiler output.");
                addLog("ERROR: build failed with exit code " + std::to_string(m_exitCode) + ".");
            } else {
                addLog("===== Build completed successfully =====");
            }
        });
    }

    void cancelBuild() {
        if (!m_buildRunning) return;
        m_buildCancelled = true;
        const pid_t pid = m_childPid.load();
        if (pid > 0) kill(-pid, SIGTERM);
        addLog("Cancelling the build...");
    }

    void launchGame() {
        const fs::path workspace = fs::absolute(fs::path(m_workspace)).lexically_normal();
        if (!gameArtifactsReady(workspace)) {
            setError("The game is not fully built in this workspace. Complete the Build step first.");
            return;
        }
        const std::string script = (workspace / "game/play.sh").string();
        char *args[] = {const_cast<char *>("sh"), const_cast<char *>(script.c_str()), nullptr};
        pid_t pid = 0;
        const int spawnError = posix_spawn(&pid, "/bin/sh", nullptr, nullptr, args, environ);
        if (spawnError != 0) {
            setError(std::string("Could not launch the game: ") + std::strerror(spawnError));
            addLog("ERROR: " + errorCopy());
            return;
        }
        addLog("Game process started.");
        std::thread([pid]() {
            int status = 0;
            while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
        }).detach();
    }

    void openGameFolder() {
        if (!executableOnPath("xdg-open")) {
            setError("xdg-open is not available on this desktop. Open the game folder from your file manager.");
            return;
        }
        const std::string directory = (fs::path(m_workspace) / "game").string();
        char *args[] = {const_cast<char *>("xdg-open"), const_cast<char *>(directory.c_str()), nullptr};
        pid_t pid = 0;
        const int spawnError = posix_spawnp(&pid, "xdg-open", nullptr, nullptr, args, environ);
        if (spawnError != 0) {
            setError(std::string("Could not open the game folder: ") + std::strerror(spawnError));
            return;
        }
        std::thread([pid]() {
            int status = 0;
            while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
        }).detach();
    }

    std::string cCompiler() const {
        if (executableOnPath("cc")) return "cc";
        if (executableOnPath("gcc")) return "gcc";
        if (executableOnPath("clang")) return "clang";
        return {};
    }

    std::vector<Tool> checkTools() const {
        std::vector<Tool> result;
        const std::string compiler = cCompiler();
        result.push_back({"C compiler", "Compiles the generated game and runtime", !compiler.empty(), false,
                          compiler.empty() ? "not found" : readPipe(compiler + " --version 2>/dev/null | head -1")});
        const bool make = executableOnPath("make");
        result.push_back({"GNU Make", "Runs the project's Makefile", make, false,
                          make ? readPipe("make --version 2>/dev/null | head -1") : "not found"});
        const bool glHeaders = fs::exists("/usr/include/GL/gl.h");
        result.push_back({"OpenGL development headers", "Compile the enhanced renderer", glHeaders, false,
                          glHeaders ? "found" : "missing GL/gl.h"});
        result.push_back({"Python + PyYAML", "ARM9 / ARM7 code generator", fs::exists(m_payload / "tools/python/bin/python3") &&
                          fs::exists(m_payload / "tools/python/lib/python3.12/site-packages/yaml/__init__.py"),
                          true, "Python 3.12.15 · PyYAML 6.0.2 · bundled"});
        result.push_back({"SDL2", "Launcher and game window / audio / input",
                          fs::exists(m_payload / "lib/libSDL2-2.0.so.0"), true, "bundled in payload"});
        if (!m_payloadAvailable) {
            result[3].ready = executableOnPath("python3") &&
                readPipe("python3 -c 'import yaml; print(yaml.__version__)' 2>/dev/null").size() > 0;
            result[3].included = false;
            result[3].detail = result[3].ready ? readPipe("python3 --version 2>/dev/null") + " · PyYAML found" : "Python 3 + PyYAML not found";
            result[4].ready = executableOnPath("sdl2-config");
            result[4].included = false;
            result[4].detail = result[4].ready ? readPipe("sdl2-config --version 2>/dev/null") : "SDL2 development files not found";
        }
        return result;
    }

    bool toolsReady() {
        if (m_preview) return m_previewToolsReady;
        return !m_tools.empty() &&
            std::all_of(m_tools.begin(), m_tools.end(), [](const Tool &tool) { return tool.ready; });
    }

    void installForMe() {
        if (m_installRunning) return;
        const std::string command = installCommand();
        if (command.empty()) {
            setError("This Linux distribution is not recognized. Install a C compiler, GNU Make and OpenGL headers with its package manager, then press Check again.");
            return;
        }
        const pid_t pid = fork();
        if (pid == 0) {
            execlp("pkexec", "pkexec", "sh", "-c", command.c_str(), static_cast<char *>(nullptr));
            _exit(127);
        }
        if (pid < 0) {
            setError(std::string("Could not start the package installer: ") + std::strerror(errno));
        } else {
            if (m_installWorker.joinable()) m_installWorker.join();
            m_installPid = pid;
            m_installRunning = true;
            m_installWorker = std::thread([this, pid]() {
                int status = 0;
                waitpid(pid, &status, 0);
                m_installPid = 0;
                m_installRunning = false;
                if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                    clearError();
                    m_recheckToolsRequested = true;
                } else {
                    setError("The system package installer did not complete. Check the package manager output and try again.");
                }
            });
        }
    }

    bool primaryButton(const char *label, ImVec2 size = ImVec2(0, 44)) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(kGold.x, kGold.y, kGold.z, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.88f, 0.48f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.91f, 0.69f, 0.22f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.08f, 0.10f, 0.18f, 1.0f));
        const bool clicked = ImGui::Button(label, size);
        ImGui::PopStyleColor(4);
        return clicked;
    }

    void drawSectionTitle(const char *title, const char *subtitle) {
        ImGui::TextColored(kGold, "%s", title);
        ImGui::Spacing();
        ImGui::TextWrapped("%s", subtitle);
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
    }

    void drawRomPage() {
        drawSectionTitle("Your Dragon Quest IX ROM", "Choose the .nds file dumped from your own Nintendo DS cartridge. The ROM stays on your computer and is never included in the release.");
        ImGui::Spacing();
        ImGui::BeginChild("rom-drop-zone", ImVec2(0, 205), true);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 18);
        ImGui::SetCursorPosX(26);
        ImGui::TextColored(kGold, "DROP YOUR .NDS FILE HERE");
        ImGui::Spacing();
        ImGui::TextWrapped("Drag a ROM onto this window, or browse to its location.");
        ImGui::Spacing();
        if (!m_rom.empty()) {
            ImGui::TextColored(kGood, "Selected file");
            ImGui::TextWrapped("%s", m_rom.c_str());
        } else {
            ImGui::TextDisabled("No ROM selected yet");
        }
        ImGui::Spacing();
        ImGui::BeginDisabled(m_buildRunning);
        if (ImGui::Button("Browse for ROM", ImVec2(170, 40))) {
            const std::string selected = chooseFile();
            if (!selected.empty()) {
                m_rom = selected;
                std::snprintf(m_romBuffer.data(), m_romBuffer.size(), "%s", m_rom.c_str());
            }
        }
        ImGui::SameLine();
        if (!m_rom.empty() && ImGui::Button("Clear", ImVec2(90, 40))) {
            m_rom.clear();
            m_romBuffer[0] = '\0';
        }
        ImGui::EndDisabled();
        ImGui::EndChild();
        ImGui::Spacing();
        ImGui::Text("ROM path (you can paste or type it here)");
        ImGui::SetNextItemWidth(-1);
        ImGui::BeginDisabled(m_buildRunning);
        if (ImGui::InputText("##rom-path", m_romBuffer.data(), m_romBuffer.size()))
            m_rom = trim(m_romBuffer.data());
        ImGui::EndDisabled();
        ImGui::Spacing();
        ImGui::TextColored(kMuted, "Supported cartridge versions");
        ImGui::BulletText("USA (YDQE)");
        ImGui::BulletText("Europe (YDQP)");
        ImGui::TextWrapped("The Japanese revision may be attempted, but is not a supported target.");
        std::error_code romError;
        if (!m_rom.empty() && (!fs::is_regular_file(m_rom, romError) || romError))
            ImGui::TextColored(kBad, "This ROM path does not point to a readable file.");
    }

    void drawToolsPage() {
        drawSectionTitle("Tools on this computer", "The release carries Python, PyYAML and SDL2. Your system supplies a C compiler, GNU Make and OpenGL development headers used to build the game from your ROM.");
        for (const auto &tool : m_tools) {
            ImGui::BeginChild(("tool-" + tool.name).c_str(), ImVec2(0, 58), true);
            ImGui::TextColored(tool.ready ? kGood : kWarn, "%s", tool.ready ? "●" : "○");
            ImGui::SameLine();
            ImGui::BeginGroup();
            ImGui::Text("%s", tool.name.c_str());
            ImGui::TextDisabled("%s", tool.purpose.c_str());
            ImGui::EndGroup();
            ImGui::SameLine();
            ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowWidth() - 225.0f));
            ImGui::TextWrapped("%s", tool.detail.c_str());
            ImGui::EndChild();
        }
        ImGui::Spacing();
        const bool ready = toolsReady();
        if (!ready) {
            ImGui::TextWrapped("Some build tools are missing. Install them through your distribution, then check again.");
            if (ImGui::Button("Install for me", ImVec2(150, 40))) installForMe();
            ImGui::SameLine();
            if (ImGui::Button("Check again", ImVec2(130, 40))) m_tools = checkTools();
        } else {
            ImGui::TextColored(kGood, "Everything needed for the Linux build is available.");
        }
    }

    void drawOptionsPage() {
        drawSectionTitle("Where should the game live?", "The ROM copy, generated C code, compiled game and save files stay in this workspace. You can move or back it up like any other folder.");
        static std::array<char, 4096> workspaceBuffer{};
        static bool initialized = false;
        if (!initialized) {
            std::snprintf(workspaceBuffer.data(), workspaceBuffer.size(), "%s", m_workspace.c_str());
            initialized = true;
        }
        ImGui::Text("Workspace folder");
        ImGui::SetNextItemWidth(-1);
        ImGui::BeginDisabled(m_buildRunning);
        if (ImGui::InputText("##workspace", workspaceBuffer.data(), workspaceBuffer.size()))
            m_workspace = workspaceBuffer.data();
        if (ImGui::IsItemDeactivatedAfterEdit()) openLogFile(m_workspace, false);
        if (ImGui::Button("Choose folder", ImVec2(145, 38))) {
            const std::string selected = chooseDirectory();
            if (!selected.empty()) {
                m_workspace = selected;
                std::snprintf(workspaceBuffer.data(), workspaceBuffer.size(), "%s", selected.c_str());
                openLogFile(m_workspace, false);
            }
        }
        ImGui::EndDisabled();
        ImGui::Spacing();
        ImGui::Text("Native compile jobs");
        ImGui::SetNextItemWidth(-1);
        ImGui::BeginDisabled(m_buildRunning);
        ImGui::SliderInt("##jobs", &m_jobs, 1, std::max(2, static_cast<int>(std::thread::hardware_concurrency())));
        ImGui::EndDisabled();
        ImGui::TextDisabled("%d parallel C compiler jobs", m_jobs);
        ImGui::TextWrapped("ARM9 and ARM7 code generation currently runs sequentially; this setting controls native C compilation.");
        ImGui::Spacing();
        ImGui::TextWrapped("The default workspace is in your XDG data folder. Choose a different location if you want the game and saves elsewhere.");
    }

    void drawBuildPage() {
        drawSectionTitle("Building DQIX Enhanced", "The launcher checks your ROM, extracts both DS CPUs, generates native C code and compiles the game. The first build takes longer; later builds reuse unchanged outputs.");
        static const std::array<const char *, 7> names = {
            "Check the ROM", "Extract game code", "Map the region", "Recompile ARM9", "Recompile ARM7", "Compile the game", "Assemble the game folder"
        };
        ImGui::BeginChild("build-progress", ImVec2(0, 218), true);
        for (int i = 0; i < static_cast<int>(names.size()); ++i) {
            const bool done = m_buildFinished && m_buildSuccess ? true : i < m_stage;
            const bool current = m_buildRunning && i == m_stage;
            ImGui::TextColored(done ? kGood : (current ? kGold : kMuted), "%s", done ? "✓" : (current ? "●" : "○"));
            ImGui::SameLine();
            ImGui::Text("%s", names[static_cast<size_t>(i)]);
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 120.0f);
            ImGui::TextDisabled("%s", done ? "Done" : (current ? "Working…" : "Waiting"));
        }
        ImGui::EndChild();
        ImGui::Spacing();
        if (m_buildRunning) {
            ImGui::TextColored(kGold, "The build is running. You can cancel it without closing the launcher.");
            ImGui::ProgressBar(std::min(0.97f, (static_cast<float>(m_stage) + 0.45f) / 7.0f), ImVec2(-1, 10), "");
            if (ImGui::Button("Cancel build", ImVec2(135, 38))) cancelBuild();
        } else if (m_buildFinished && m_buildSuccess) {
            ImGui::TextColored(kGood, "Build finished. Your game is ready.");
            if (ImGui::Button("Open game folder", ImVec2(160, 38))) openGameFolder();
        } else if (m_buildFinished || m_previewFailed) {
            const std::string buildError = errorCopy();
            ImGui::TextColored(m_buildCancelled ? kWarn : kBad, "%s",
                               m_buildCancelled ? "Build cancelled. Some workspace files may be incomplete; run Build again before Play."
                                                : (buildError.empty() ? "The build failed. Check log.txt below." : buildError.c_str()));
            if (ImGui::Button("Build again", ImVec2(135, 38))) startBuild();
        } else {
            ImGui::TextWrapped("Ready to build. Your ROM and workspace are not uploaded anywhere.");
            if (primaryButton("Start building", ImVec2(180, 44))) startBuild();
        }
        ImGui::Spacing();
        ImGui::Text("Build log");
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 172.0f);
        const auto availableLog = logCopy();
        ImGui::BeginDisabled(availableLog.empty());
        if (ImGui::Button("Copy build log", ImVec2(142, 32))) copyBuildLog();
        ImGui::EndDisabled();
        ImGui::TextDisabled("Saved to %s/log.txt", m_workspace.c_str());
        if (!m_logFeedback.empty())
            ImGui::TextColored(m_logFeedback.find("Could not") != std::string::npos ? kWarn : kGood,
                               "%s", m_logFeedback.c_str());
        ImGui::BeginChild("build-log", ImVec2(0, 185), true, ImGuiWindowFlags_HorizontalScrollbar);
        const auto lines = logCopy();
        for (const auto &line : lines) ImGui::TextUnformatted(line.c_str());
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();
    }

    void drawPlayPage() {
        const bool gameReady = gameArtifactsReady(fs::path(m_workspace));
        drawSectionTitle(gameReady ? "Ready to play" : "Game not built yet",
                         gameReady
                             ? "Your compiled game and save files live in the workspace you chose. The launcher does not upload or share them."
                             : "Complete the Build step to generate the game files. Nothing can be launched until the executable, core library and ROM data are present.");
        ImGui::BeginChild("play-card", ImVec2(0, 220), true);
        ImGui::TextColored(gameReady ? kGood : kGold, "%s", gameReady ? "Your game is ready" : "No built game in this workspace yet");
        ImGui::Spacing();
        ImGui::TextWrapped("%s", (fs::path(m_workspace) / "game").string().c_str());
        ImGui::Spacing();
        if (gameReady && primaryButton("Play Dragon Quest IX", ImVec2(240, 48))) launchGame();
        if (!gameReady && ImGui::Button("Go to Build", ImVec2(140, 40))) m_page = Page::Build;
        ImGui::EndChild();
        ImGui::Spacing();
        ImGui::TextDisabled("To report a problem, attach log.txt from the workspace root and describe what happened.");
    }

    void drawSidebar() {
        ImGui::BeginChild("sidebar", ImVec2(218, 0), false);
        ImGui::SetCursorPos(ImVec2(22, 18));
        ImGui::TextColored(kMuted, "SETUP");
        ImGui::Spacing();
        static const std::array<const char *, 5> names = {"Your ROM", "Tools", "Options", "Build", "Play"};
        static const std::array<const char *, 5> subtitles = {"Choose the cartridge", "Check requirements", "Choose workspace", "Compile the game", "Start playing"};
        for (int i = 0; i < 5; ++i) {
            const bool selected = static_cast<int>(m_page) == i;
            ImGui::PushID(i);
            if (selected) {
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.43f, 0.55f, 1.0f, 0.14f));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.43f, 0.55f, 1.0f, 0.14f));
            }
            ImGui::SetCursorPosX(12);
            ImGui::BeginChild("nav-item", ImVec2(194, 60), false);
            const Page target = static_cast<Page>(i);
            const bool requirementsMet =
                target != Page::Build ||
                (!m_source.empty() && !m_rom.empty() && fs::is_regular_file(m_rom) && toolsReady());
            const bool enabled = !m_buildRunning ? (requirementsMet &&
                (target != Page::Play || gameArtifactsReady(fs::path(m_workspace)))) : target == Page::Build;
            ImGui::BeginDisabled(!enabled);
            ImDrawList *draw = ImGui::GetWindowDrawList();
            const ImVec2 p = ImGui::GetWindowPos();
            if (selected) {
                draw->AddRectFilled(ImVec2(p.x + 1, p.y + 4), ImVec2(p.x + 4, p.y + 56),
                                    IM_COL32(244, 203, 82, 255), 2.0f);
                draw->AddTriangleFilled(ImVec2(p.x + 17, p.y + 24), ImVec2(p.x + 28, p.y + 30),
                                        ImVec2(p.x + 17, p.y + 36), IM_COL32(255, 255, 255, 255));
            } else {
                draw->AddCircleFilled(ImVec2(p.x + 22, p.y + 30), 3.5f, IM_COL32(158, 168, 204, 150));
            }
            ImGui::SetCursorPos(ImVec2(selected ? 42 : 42, 10));
            ImGui::TextColored(selected ? kGold : ImVec4(0.93f, 0.95f, 1.0f, 1.0f), "%s", names[static_cast<size_t>(i)]);
            ImGui::SetCursorPos(ImVec2(42, 32));
            ImGui::TextDisabled("%s", subtitles[static_cast<size_t>(i)]);
            ImGui::SetCursorPos(ImVec2(0, 0));
            if (ImGui::InvisibleButton("hitbox", ImVec2(194, 60))) m_page = target;
            ImGui::EndDisabled();
            ImGui::EndChild();
            ImGui::PopStyleColor(selected ? 2 : 0);
            ImGui::PopID();
        }
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 70.0f);
        ImGui::Separator();
        ImGui::TextDisabled("DQIX ENHANCED");
        ImGui::TextDisabled("Linux Builder");
        ImGui::EndChild();
    }

    void drawContent() {
        ImGui::BeginChild("page-content", ImVec2(0, 0), false);
        ImGui::SetCursorPos(ImVec2(34, 27));
        ImGui::BeginGroup();
        ImGui::TextColored(kMuted, "STEP %02d / 05", static_cast<int>(m_page) + 1);
        ImGui::Spacing();
        switch (m_page) {
        case Page::Rom: drawRomPage(); break;
        case Page::Tools: drawToolsPage(); break;
        case Page::Options: drawOptionsPage(); break;
        case Page::Build: drawBuildPage(); break;
        case Page::Play: drawPlayPage(); break;
        }
        ImGui::EndGroup();
        ImGui::EndChild();
    }

    void drawFooter() {
        ImGui::BeginChild("footer", ImVec2(0, 78), false);
        ImGui::SetCursorPos(ImVec2(25, 15));
        if (m_page != Page::Rom && !m_buildRunning && ImGui::Button("Back", ImVec2(100, 42)))
            m_page = static_cast<Page>(std::max(0, static_cast<int>(m_page) - 1));
        if (m_page != Page::Rom) ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 210.0f);
        bool canContinue = false;
        std::error_code romError;
        const bool romReady = !m_rom.empty() && fs::is_regular_file(m_rom, romError) && !romError;
        if (!m_buildRunning) {
            switch (m_page) {
            case Page::Rom: canContinue = romReady; break;
            case Page::Tools: canContinue = toolsReady(); break;
            case Page::Options: canContinue = romReady && !m_source.empty() && toolsReady(); break;
            case Page::Build: canContinue = gameArtifactsReady(fs::path(m_workspace)); break;
            case Page::Play: break;
            }
        }
        if (m_page != Page::Play) {
            if (canContinue) {
                if (primaryButton(m_page == Page::Build ? "Continue to Play" : "Continue", ImVec2(185, 44)))
                    m_page = static_cast<Page>(std::min(4, static_cast<int>(m_page) + 1));
            } else {
                ImGui::Dummy(ImVec2(185, 44));
            }
        } else if (gameArtifactsReady(fs::path(m_workspace))) {
            if (primaryButton("Play", ImVec2(185, 44))) launchGame();
        } else {
            ImGui::Dummy(ImVec2(185, 44));
        }
        ImGui::EndChild();
    }

    void drawFrame(bool present = true) {
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGuiIO &io = ImGui::GetIO();
        const ImVec2 display = io.DisplaySize;
        const ImVec2 origin(20.0f, 20.0f);
        const ImVec2 size(std::max(740.0f, display.x - 40.0f), std::max(600.0f, display.y - 40.0f));
        ImDrawList *background = ImGui::GetBackgroundDrawList();
        background->AddRectFilled(ImVec2(origin.x + 2, origin.y + 5), ImVec2(origin.x + size.x + 2, origin.y + size.y + 7),
                                  IM_COL32(0, 0, 0, 105), 20.0f);
        ImGui::SetNextWindowPos(origin, ImGuiCond_Always);
        ImGui::SetNextWindowSize(size, ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 19.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, kWindow);
        ImGui::Begin("##dqix-launcher", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);
        const ImVec2 panelMin = ImGui::GetWindowPos();
        const ImVec2 panelMax(panelMin.x + ImGui::GetWindowSize().x, panelMin.y + ImGui::GetWindowSize().y);
        ImDrawList *draw = ImGui::GetWindowDrawList();
        draw->AddRect(panelMin, panelMax, IM_COL32(242, 243, 252, 240), 19.0f, 0, 1.25f);
        draw->AddRect(ImVec2(panelMin.x + 3, panelMin.y + 3), ImVec2(panelMax.x - 3, panelMax.y - 3),
                      IM_COL32(255, 255, 255, 42), 16.0f, 0, 1.0f);

        // DQ8's fixed header: emblem, project name and one-line explanation.
        ImGui::BeginChild("header", ImVec2(0, 104), false);
        ImGui::SetCursorPos(ImVec2(27, 20));
        ImDrawList *headerDraw = ImGui::GetWindowDrawList();
        const ImVec2 logo(ImGui::GetWindowPos().x + 42, ImGui::GetWindowPos().y + 47);
        headerDraw->AddCircleFilled(logo, 24.0f, IM_COL32(244, 203, 82, 34), 36);
        headerDraw->AddCircle(logo, 24.0f, IM_COL32(244, 203, 82, 220), 36, 1.5f);
        ImGui::TextColored(kGold, "DQ");
        ImGui::SetCursorPos(ImVec2(80, 18));
        ImGui::BeginGroup();
        ImGui::Text("DQIX Enhanced");
        ImGui::TextDisabled("Build Dragon Quest IX natively from your own Nintendo DS ROM");
        ImGui::EndGroup();
        ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() - 210.0f, 34));
        ImGui::TextColored(kMuted, "LINUX · %s", m_payloadAvailable ? "RELEASE PAYLOAD" : "SOURCE BUILD");
        ImGui::EndChild();
        ImGui::Separator();

        const float bodyHeight = ImGui::GetWindowHeight() - 184.0f;
        ImGui::BeginChild("body", ImVec2(0, bodyHeight), false);
        // The body child starts at local (0, 0): WindowPadding is explicitly zero above.
        // Avoid a redundant SetCursorPos() here; child windows follow immediately.
        drawSidebar();
        ImGui::SameLine(0, 0);
        ImGui::BeginChild("main", ImVec2(0, 0), false);
        drawContent();
        ImGui::EndChild();
        ImGui::EndChild();
        ImGui::Separator();
        drawFooter();

        const std::string displayedError = errorCopy();
        if (!displayedError.empty() && m_page != Page::Build) {
            ImGui::SetNextWindowPos(ImVec2(36, display.y - 112), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(display.x - 72, 54), ImGuiCond_Always);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.40f, 0.08f, 0.10f, 0.96f));
            ImGui::Begin("##error-toast", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
            ImGui::TextWrapped("%s", displayedError.c_str());
            ImGui::End();
            ImGui::PopStyleColor();
        }

        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
        ImGui::Render();
        SDL_SetRenderDrawColor(m_renderer, 7, 10, 27, 255);
        SDL_RenderClear(m_renderer);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), m_renderer);
        if (present) SDL_RenderPresent(m_renderer);
    }
};

} // namespace

int main(int argc, char **argv) {
    bool preview = false;
    fs::path previewDir;
    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg == "--version") {
            std::puts("DQIX Enhanced Linux Launcher 1.0.0");
            return 0;
        }
        if (arg == "--preview" && i + 1 < argc) {
            preview = true;
            previewDir = argv[++i];
        }
        if (arg == "--smoke") {
            preview = true;
            previewDir = "build/launcher-screenshots";
        }
        if (arg == "--help" || arg == "-h") {
            std::puts("DQIX Enhanced Linux Launcher\n\n"
                      "  dqix-launcher              Open the graphical builder\n"
                      "  dqix-launcher --version    Print the launcher version\n"
                      "  dqix-launcher --preview DIR Render the UI pages to BMP screenshots\n"
                      "  dqix-launcher --smoke      Render screenshots for CI smoke tests\n");
            return 0;
        }
    }

    Launcher app;
    if (!app.initialize(preview)) {
        std::fprintf(stderr, "launcher: initialization failed\n");
        app.shutdown();
        return 1;
    }
    int result = 0;
    if (preview) {
        if (!app.makePreview(previewDir)) result = 1;
    } else {
        app.run();
    }
    app.shutdown();
    return result;
}
