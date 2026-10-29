// Minimal Linux-only launcher using the project's existing SDL2 stack.
// No ImGui, SDL3, Vulkan, SDL_ttf or other UI library is required.
#include "../settings/settings_model.hpp"
#include <SDL.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <cstdarg>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace fs = std::filesystem;
using dqix::linux_ui::IniDocument;
namespace {
constexpr int kWidth = 1280;
constexpr int kHeight = 800;
constexpr int kUiScale = 2;
enum class Page { Game, Graphics, Display, Audio, Controls, Mods, Wireless, System };
constexpr std::array<const char*, 8> kPageNames = {
    "GAME", "GRAPHICS", "SCREEN / DISPLAY", "AUDIO / MUSIC",
    "CONTROLS", "MODS", "WIRELESS / ONLINE", "SYSTEM"
};
struct LauncherState {
    fs::path game_dir, settings_path, log_path;
    IniDocument settings;
    Page page = Page::Game;
    std::string message = "Choose a built game folder with --game-dir PATH.";
    bool message_is_error = false, settings_load_ok = true;
    pid_t game_pid = -1;
    std::string child_result;
    int focus_index = 0, previous_button_count = 1, button_count = 0;
};
bool g_mouse_click = false;
int g_mouse_x = 0, g_mouse_y = 0;
bool g_activate = false;

void log_message(const fs::path& path, const char* format, ...) {
    char buffer[2048]{};
    va_list args;
    va_start(args, format);
    std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    std::fprintf(stderr, "%s", buffer);
    std::fflush(stderr);
    if (!path.empty()) {
        std::ofstream file(path, std::ios::binary | std::ios::app);
        if (file) file << buffer;
    }
}
fs::path absolute_path(const fs::path& path) {
    std::error_code ec;
    const fs::path result = fs::absolute(path, ec);
    return (ec ? path : result).lexically_normal();
}
std::string setting_value(const LauncherState& state, std::string_view key, std::string_view fallback) {
    return state.settings.get(key).value_or(std::string(fallback));
}
bool boolean_setting(const LauncherState& state, std::string_view key, bool fallback = false) {
    const std::string value = setting_value(state, key, fallback ? "1" : "0");
    char* end = nullptr;
    const long parsed = std::strtol(value.c_str(), &end, 10);
    return end != value.c_str() && *end == '\0' && parsed != 0;
}
int integer_setting(const LauncherState& state, std::string_view key, int fallback, int minimum, int maximum) {
    const std::string value = setting_value(state, key, std::to_string(fallback));
    char* end = nullptr;
    const long parsed = std::strtol(value.c_str(), &end, 10);
    if (end == value.c_str() || *end != '\0' || parsed < minimum || parsed > maximum) return fallback;
    return static_cast<int>(parsed);
}
bool load_settings(LauncherState& state) {
    state.settings_path = state.game_dir / "settings.ini";
    state.settings = IniDocument::parse("");
    state.settings_load_ok = true;
    std::error_code ec;
    const bool exists = fs::exists(state.settings_path, ec);
    if (ec) {
        state.message = "Cannot inspect settings.ini: " + ec.message();
        state.message_is_error = true; state.settings_load_ok = false; return false;
    }
    if (!exists) {
        state.message = "No settings.ini found; defaults are shown until you save a change.";
        state.message_is_error = false; return true;
    }
    std::string error;
    if (!state.settings.load_file(state.settings_path, error)) {
        state.message = error; state.message_is_error = true; state.settings_load_ok = false;
        log_message(state.log_path, "[settings] %s\n", error.c_str()); return false;
    }
    state.message = "Loaded settings.ini. Saved values may still depend on runtime capabilities.";
    state.message_is_error = false;
    log_message(state.log_path, "[settings] loaded %s\n", state.settings_path.c_str());
    return true;
}
bool commit_setting(LauncherState& state, std::string_view key, std::string_view value) {
    if (!state.settings_load_ok) {
        state.message = "Settings cannot be edited because settings.ini could not be read.";
        state.message_is_error = true; return false;
    }
    const IniDocument previous = state.settings;
    std::string error;
    if (!state.settings.set_validated(key, value, error)) {
        state.message = error; state.message_is_error = true; return false;
    }
    std::error_code ec;
    fs::create_directories(state.game_dir, ec);
    if (ec || !state.settings.save_atomic(state.settings_path, error)) {
        state.settings = previous;
        state.message = ec ? "Cannot create game directory: " + ec.message() : error;
        state.message_is_error = true;
        log_message(state.log_path, "[settings] save failed: %s\n", state.message.c_str());
        return false;
    }
    state.message = "Saved " + std::string(key) + ". Runtime support is checked when the game starts.";
    state.message_is_error = false;
    log_message(state.log_path, "[settings] saved %s to %s\n", std::string(key).c_str(), state.settings_path.c_str());
    return true;
}
bool path_has_file(const fs::path& path) {
    std::error_code ec; return fs::is_regular_file(path, ec);
}
bool game_ready(const LauncherState& state) {
    const fs::path executable = state.game_dir / "dqix";
    std::error_code ec;
    return path_has_file(executable) && ::access(executable.c_str(), X_OK) == 0 &&
           path_has_file(state.game_dir / "data" / "dqix.nds") &&
           fs::is_directory(state.game_dir / "data" / "code", ec);
}
std::string short_path(const fs::path& path, std::size_t maximum = 72) {
    const std::string value = path.string();
    if (value.size() <= maximum) return value;
    return "..." + value.substr(value.size() - maximum + 3);
}
void launch_game(LauncherState& state) {
    if (state.game_pid > 0) {
        state.message = "The game is already running (PID " + std::to_string(state.game_pid) + ").";
        state.message_is_error = false; return;
    }
    if (!game_ready(state)) {
        state.message = "Game not ready: expected dqix, data/dqix.nds and data/code/.";
        state.message_is_error = true; return;
    }
    const std::string executable = (state.game_dir / "dqix").string();
    const std::string cwd = state.game_dir.string();
    const pid_t pid = ::fork();
    if (pid < 0) {
        state.message = "Could not start game: " + std::string(std::strerror(errno));
        state.message_is_error = true;
        log_message(state.log_path, "[launch] fork failed: %s\n", std::strerror(errno)); return;
    }
    if (pid == 0) {
        if (::chdir(cwd.c_str()) != 0) _exit(126);
        const int log_fd = ::open(state.log_path.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0600);
        if (log_fd >= 0) {
            (void)::dup2(log_fd, STDOUT_FILENO); (void)::dup2(log_fd, STDERR_FILENO);
            if (log_fd > STDERR_FILENO) ::close(log_fd);
        }
        char* const args[] = {
            const_cast<char*>(executable.c_str()), const_cast<char*>("data/dqix.nds"),
            const_cast<char*>("data/code"), const_cast<char*>("0"), const_cast<char*>("1"),
            const_cast<char*>("dqix.sav"), nullptr
        };
        ::execv(executable.c_str(), args);
        _exit(127);
    }
    state.game_pid = pid; state.child_result.clear();
    state.message = "Game started (PID " + std::to_string(pid) + "). Runtime output goes to launcher.log.";
    state.message_is_error = false;
    log_message(state.log_path, "[launch] started game pid=%ld from %s\n", static_cast<long>(pid), cwd.c_str());
}
void update_child_status(LauncherState& state) {
    if (state.game_pid <= 0) return;
    int status = 0;
    const pid_t result = ::waitpid(state.game_pid, &status, WNOHANG);
    if (result == 0) return;
    if (result < 0) {
        if (errno == EINTR) return;
        state.message = "Could not inspect game process: " + std::string(std::strerror(errno));
        state.message_is_error = true; state.game_pid = -1; return;
    }
    if (WIFEXITED(status)) state.child_result = "Game exited with code " + std::to_string(WEXITSTATUS(status)) + ".";
    else if (WIFSIGNALED(status)) state.child_result = "Game stopped by signal " + std::to_string(WTERMSIG(status)) + ".";
    else state.child_result = "Game process ended.";
    state.game_pid = -1; state.message = state.child_result; state.message_is_error = false;
    log_message(state.log_path, "[launch] %s\n", state.child_result.c_str());
}

// Compact 5x7 bitmap font: no external text or UI dependency.
const char* glyph_bits(char c) {
    static constexpr const char* blank = "00000""00000""00000""00000""00000""00000""00000";
    switch (static_cast<char>(std::toupper(static_cast<unsigned char>(c)))) {
    case 'A': return "01110""10001""10001""11111""10001""10001""10001";
    case 'B': return "11110""10001""10001""11110""10001""10001""11110";
    case 'C': return "01111""10000""10000""10000""10000""10000""01111";
    case 'D': return "11110""10001""10001""10001""10001""10001""11110";
    case 'E': return "11111""10000""10000""11110""10000""10000""11111";
    case 'F': return "11111""10000""10000""11110""10000""10000""10000";
    case 'G': return "01111""10000""10000""10111""10001""10001""01111";
    case 'H': return "10001""10001""10001""11111""10001""10001""10001";
    case 'I': return "11111""00100""00100""00100""00100""00100""11111";
    case 'J': return "00111""00010""00010""00010""10010""10010""01100";
    case 'K': return "10001""10010""10100""11000""10100""10010""10001";
    case 'L': return "10000""10000""10000""10000""10000""10000""11111";
    case 'M': return "10001""11011""10101""10101""10001""10001""10001";
    case 'N': return "10001""11001""10101""10011""10001""10001""10001";
    case 'O': return "01110""10001""10001""10001""10001""10001""01110";
    case 'P': return "11110""10001""10001""11110""10000""10000""10000";
    case 'Q': return "01110""10001""10001""10001""10101""10010""01101";
    case 'R': return "11110""10001""10001""11110""10100""10010""10001";
    case 'S': return "01111""10000""10000""01110""00001""00001""11110";
    case 'T': return "11111""00100""00100""00100""00100""00100""00100";
    case 'U': return "10001""10001""10001""10001""10001""10001""01110";
    case 'V': return "10001""10001""10001""10001""10001""01010""00100";
    case 'W': return "10001""10001""10001""10101""10101""10101""01010";
    case 'X': return "10001""10001""01010""00100""01010""10001""10001";
    case 'Y': return "10001""10001""01010""00100""00100""00100""00100";
    case 'Z': return "11111""00001""00010""00100""01000""10000""11111";
    case '0': return "01110""10001""10011""10101""11001""10001""01110";
    case '1': return "00100""01100""00100""00100""00100""00100""01110";
    case '2': return "01110""10001""00001""00010""00100""01000""11111";
    case '3': return "11110""00001""00001""01110""00001""00001""11110";
    case '4': return "00010""00110""01010""10010""11111""00010""00010";
    case '5': return "11111""10000""10000""11110""00001""00001""11110";
    case '6': return "01110""10000""10000""11110""10001""10001""01110";
    case '7': return "11111""00001""00010""00100""01000""01000""01000";
    case '8': return "01110""10001""10001""01110""10001""10001""01110";
    case '9': return "01110""10001""10001""01111""00001""00001""01110";
    case '-': return "00000""00000""00000""11111""00000""00000""00000";
    case '_': return "00000""00000""00000""00000""00000""00000""11111";
    case '.': return "00000""00000""00000""00000""00000""00110""00110";
    case ':': return "00000""00110""00110""00000""00110""00110""00000";
    case '/': return "00001""00010""00010""00100""01000""01000""10000";
    case '\\': return "10000""01000""01000""00100""00010""00010""00001";
    case '+': return "00000""00100""00100""11111""00100""00100""00000";
    case '%': return "11001""11010""00100""01000""10110""00110""00000";
    case '=': return "00000""11111""00000""11111""00000""00000""00000";
    case '>': return "10000""01000""00100""00010""00100""01000""10000";
    case '(': return "00010""00100""01000""01000""01000""00100""00010";
    case ')': return "01000""00100""00010""00010""00010""00100""01000";
    case '?': return "01110""10001""00001""00010""00100""00000""00100";
    case ',': return "00000""00000""00000""00000""00110""00100""01000";
    case '!': return "00100""00100""00100""00100""00100""00000""00100";
    case '\'': return "00100""00100""00000""00000""00000""00000""00000";
    default: return blank;
    }
}
void set_color(SDL_Renderer* renderer, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}
void fill_rect(SDL_Renderer* renderer, int x, int y, int w, int h, SDL_Color color) {
    SDL_Rect rect{x, y, w, h}; set_color(renderer, color); SDL_RenderFillRect(renderer, &rect);
}
void draw_text(SDL_Renderer* renderer, int x, int y, std::string_view text,
               SDL_Color color = SDL_Color{220,228,240,255}, int scale = kUiScale,
               std::size_t maximum_chars = 120) {
    set_color(renderer, color);
    const std::size_t limit = std::min(text.size(), maximum_chars);
    int cursor_x = x;
    for (std::size_t i = 0; i < limit; ++i) {
        const char* bits = glyph_bits(text[i]);
        for (int row = 0; row < 7; ++row)
            for (int column = 0; column < 5; ++column)
                if (bits[row * 5 + column] == '1') {
                    SDL_Rect pixel{cursor_x + column * scale, y + row * scale, scale, scale};
                    SDL_RenderFillRect(renderer, &pixel);
                }
        cursor_x += 6 * scale;
        if (cursor_x >= kWidth - 12) break;
    }
}
bool draw_button(SDL_Renderer* renderer, LauncherState& state, int x, int y, int w, int h,
                 const std::string& label, bool disabled = false) {
    const int index = state.button_count++;
    const bool focused = index == state.focus_index;
    const SDL_Rect bounds{x,y,w,h};
    const bool inside = g_mouse_x >= x && g_mouse_x < x+w && g_mouse_y >= y && g_mouse_y < y+h;
    const bool clicked = g_mouse_click && inside && !disabled;
    if (clicked) state.focus_index = index;
    const bool activated = !disabled && (clicked || (g_activate && focused));
    fill_rect(renderer, x, y, w, h, disabled ? SDL_Color{36,43,55,255} :
              focused || inside ? SDL_Color{52,76,106,255} : SDL_Color{35,48,66,255});
    set_color(renderer, focused ? SDL_Color{102,190,255,255} : SDL_Color{63,80,103,255});
    SDL_RenderDrawRect(renderer, &bounds);
    draw_text(renderer, x+14, y+(h-14)/2, label,
        disabled ? SDL_Color{112,124,140,255} : SDL_Color{235,240,247,255}, 2,
        static_cast<std::size_t>(std::max(1,(w-28)/12)));
    return activated;
}
void page_title(SDL_Renderer* renderer, const char* title, const char* description) {
    draw_text(renderer, 260, 118, title, SDL_Color{246,193,95,255}, 3);
    draw_text(renderer, 260, 152, description, SDL_Color{166,180,199,255}, 2, 115);
}
void setting_toggle(SDL_Renderer* renderer, LauncherState& state, int y, const char* label,
                    const char* key, bool fallback = false) {
    const bool value = boolean_setting(state, key, fallback);
    if (draw_button(renderer, state, 270, y, 440, 46,
                    std::string(label) + ": " + (value ? "ON" : "OFF")))
        (void)commit_setting(state, key, value ? "0" : "1");
}
void render_ui(SDL_Renderer* renderer, LauncherState& state) {
    update_child_status(state);
    state.button_count = 0;
    set_color(renderer, SDL_Color{15,23,42,255}); SDL_RenderClear(renderer);
    fill_rect(renderer, 0, 0, kWidth, 82, SDL_Color{24,35,54,255});
    draw_text(renderer, 28, 25, "DQIX ENHANCED", SDL_Color{245,191,84,255}, 3);
    draw_text(renderer, 330, 32, "LINUX LAUNCHER / SDL2", SDL_Color{168,184,205,255}, 2, 60);
    fill_rect(renderer, 0, 82, 225, kHeight-142, SDL_Color{20,30,46,255});
    draw_text(renderer, 22, 102, "MENU", SDL_Color{126,150,178,255}, 2);
    for (int i = 0; i < static_cast<int>(kPageNames.size()); ++i)
        if (draw_button(renderer,state,12,132+i*51,200,43,kPageNames[static_cast<std::size_t>(i)]))
            state.page = static_cast<Page>(i);

    switch (state.page) {
    case Page::Game: {
        page_title(renderer,"GAME","Select a built game folder using --game-dir PATH.");
        draw_text(renderer,270,207,"GAME FOLDER",SDL_Color{126,150,178,255});
        draw_text(renderer,270,230,short_path(state.game_dir),SDL_Color{232,237,245,255},2,76);
        draw_text(renderer,270,275,std::string("EXECUTABLE: ")+
            (path_has_file(state.game_dir/"dqix") && ::access((state.game_dir/"dqix").c_str(),X_OK)==0?"READY":"MISSING"),
            SDL_Color{190,205,222,255});
        draw_text(renderer,270,298,std::string("ROM: ")+(path_has_file(state.game_dir/"data"/"dqix.nds")?"FOUND":"MISSING"),
            SDL_Color{190,205,222,255});
        std::error_code ec;
        draw_text(renderer,270,321,std::string("GENERATED CODE: ")+
            (fs::is_directory(state.game_dir/"data"/"code",ec)?"FOUND":"MISSING"),SDL_Color{190,205,222,255});
        if (draw_button(renderer,state,270,370,250,48,"RELOAD SETTINGS")) (void)load_settings(state);
        if (draw_button(renderer,state,270,430,250,54,"PLAY NOW",!game_ready(state))) launch_game(state);
        draw_text(renderer,270,510,"Uses the same data/dqix.nds and data/code/ layout as the build script.",
                  SDL_Color{144,160,181,255},2,95);
        draw_text(renderer,270,546,"Game output is appended to launcher.log in the selected folder.",
                  SDL_Color{144,160,181,255},2,95);
        break;
    }
    case Page::Graphics:
        page_title(renderer,"GRAPHICS","Saved values may depend on the active gameplay renderer.");
        if (draw_button(renderer,state,270,215,440,46,
            "MODE: "+std::string(setting_value(state,"mode","classic")=="enhanced"?"ENHANCED":"CLASSIC")))
            (void)commit_setting(state,"mode",setting_value(state,"mode","classic")=="enhanced"?"classic":"enhanced");
        setting_toggle(renderer,state,274,"SMOOTH TEXTURES","smooth_textures");
        setting_toggle(renderer,state,333,"HD TEXT","hd_text");
        setting_toggle(renderer,state,392,"SOFT SHADOWS","soft_shadows",true);
        setting_toggle(renderer,state,451,"SMOOTH SHADING","smooth_shading");
        draw_text(renderer,270,526,"Some changes may require a game restart.",SDL_Color{144,160,181,255},2,96);
        break;
    case Page::Display:
        page_title(renderer,"SCREEN / DISPLAY","Display options are saved for the existing SDL2 runtime.");
        setting_toggle(renderer,state,215,"FULLSCREEN","fullscreen");
        setting_toggle(renderer,state,274,"INTEGER SCALING","integer_scaling");
        setting_toggle(renderer,state,333,"DS LITE FILTER","dslite_filter",true);
        setting_toggle(renderer,state,392,"LCD COLOURS","lcd_colors",true);
        setting_toggle(renderer,state,451,"ACTUAL SIZE","actual_size");
        if (draw_button(renderer,state,270,510,440,46,"WINDOW SCALE: "+
            std::to_string(integer_setting(state,"window_scale",2,1,8))+"X")) {
            const int value=integer_setting(state,"window_scale",2,1,8);
            (void)commit_setting(state,"window_scale",std::to_string(value>=8?1:value+1));
        }
        break;
    case Page::Audio:
        page_title(renderer,"AUDIO / MUSIC","Audio preferences are stored for the next game start.");
        setting_toggle(renderer,state,215,"MUTE","mute");
        if (draw_button(renderer,state,270,274,440,46,"GAME VOLUME: "+
            std::to_string(integer_setting(state,"volume",100,0,100))+"%")) {
            const int value=integer_setting(state,"volume",100,0,100);
            (void)commit_setting(state,"volume",std::to_string(value>=100?0:std::min(100,value+10)));
        }
        setting_toggle(renderer,state,333,"MUSIC PACKS","music_packs",true);
        setting_toggle(renderer,state,392,"SHOW SONG NAMES","music_song_names");
        break;
    case Page::Controls:
        page_title(renderer,"CONTROLS","Keyboard and gamepad remapping is not implemented here yet.");
        draw_text(renderer,270,220,"The game runtime owns input dispatch and controls.ini.",SDL_Color{190,205,222,255});
        draw_text(renderer,270,250,"This page does not claim to apply control changes.",SDL_Color{190,205,222,255});
        break;
    case Page::Mods: {
        page_title(renderer,"MODS","Matching assets are required for these settings to take effect.");
        setting_toggle(renderer,state,215,"TEXTURE PACKS","texture_packs",true);
        setting_toggle(renderer,state,274,"UPSCALE TEXTURES","upscale_textures");
        setting_toggle(renderer,state,333,"MUSIC PACKS","music_packs",true);
        std::error_code e1,e2;
        draw_text(renderer,270,410,std::string("TEXTURES FOLDER: ")+(fs::is_directory(state.game_dir/"textures",e1)?"FOUND":"NOT FOUND"),SDL_Color{190,205,222,255});
        draw_text(renderer,270,440,std::string("MUSIC FOLDER: ")+(fs::is_directory(state.game_dir/"music",e2)?"FOUND":"NOT FOUND"),SDL_Color{190,205,222,255});
        break;
    }
    case Page::Wireless:
        page_title(renderer,"WIRELESS / ONLINE","Network settings are saved; server connectivity is not tested here.");
        setting_toggle(renderer,state,215,"WIRELESS FEATURES","wireless",true);
        draw_text(renderer,270,290,"WFC SERVER: "+setting_value(state,"wfc_server","builtin"),SDL_Color{190,205,222,255},2,76);
        draw_text(renderer,270,325,"PLAYER NAME: "+setting_value(state,"player_name",""),SDL_Color{190,205,222,255},2,76);
        draw_text(renderer,270,360,"Editing these text fields is not yet implemented.",SDL_Color{144,160,181,255},2,90);
        break;
    case Page::System: {
        page_title(renderer,"SYSTEM / DIAGNOSTICS","These details describe the launcher, not gameplay capability.");
        SDL_version version{}; SDL_GetVersion(&version);
        draw_text(renderer,270,215,"SDL2 VERSION: "+std::to_string(version.major)+"."+std::to_string(version.minor)+"."+std::to_string(version.patch),SDL_Color{190,205,222,255});
        draw_text(renderer,270,255,"GAME: "+short_path(state.game_dir,70),SDL_Color{190,205,222,255},2,85);
        draw_text(renderer,270,290,"SETTINGS: "+short_path(state.settings_path,70),SDL_Color{190,205,222,255},2,85);
        draw_text(renderer,270,325,"LOG: "+short_path(state.log_path,70),SDL_Color{190,205,222,255},2,85);
        draw_text(renderer,270,372,"UI renderer: SDL_Renderer; gameplay uses the existing runtime paths.",SDL_Color{144,160,181,255},2,100);
        if (draw_button(renderer,state,270,430,260,46,"RELOAD SETTINGS")) (void)load_settings(state);
        if (draw_button(renderer,state,550,430,260,46,"SHOW LOG PATH")) state.message="Log file: "+state.log_path.string();
        break;
    }
    }
    fill_rect(renderer,0,742,kWidth,58,SDL_Color{24,35,54,255});
    draw_text(renderer,20,765,state.message,
        state.message_is_error?SDL_Color{255,133,125,255}:SDL_Color{166,211,172,255},2,102);
    state.previous_button_count=std::max(1,state.button_count);
    if (state.focus_index>=state.button_count) state.focus_index=std::max(0,state.button_count-1);
    SDL_RenderPresent(renderer);
    g_activate=false; g_mouse_click=false;
}

bool parse_arguments(int argc,char** argv,LauncherState& state,bool& show_help,bool& self_test,std::string& error) {
    fs::path requested; bool requested_explicitly=false;
    for(int i=1;i<argc;++i) {
        const std::string_view arg(argv[i]);
        if(arg=="--help"||arg=="-h"){show_help=true;return true;}
        if(arg=="--self-test"){self_test=true;continue;}
        if(arg=="--game-dir") {
            if(i+1>=argc){error="--game-dir requires a path";return false;}
            requested=argv[++i]; requested_explicitly=true; continue;
        }
        error="unknown argument: "+std::string(arg); return false;
    }
    if(!requested_explicitly) {
        if(const char* env=std::getenv("DQIX_GAME_DIR");env&&*env){requested=env;requested_explicitly=true;}
    }
    if(!requested_explicitly) {
        std::error_code ec; const fs::path current=fs::current_path(ec); const fs::path candidate=current/"game";
        requested=(!ec&&fs::is_directory(candidate,ec))?candidate:current;
    }
    state.game_dir=absolute_path(requested); state.settings_path=state.game_dir/"settings.ini";
    state.log_path=state.game_dir/"launcher.log"; return true;
}
int self_test() {
    (void)::setenv("SDL_VIDEODRIVER","dummy",1); (void)::setenv("SDL_AUDIODRIVER","dummy",1);
    SDL_SetHint(SDL_HINT_RENDER_DRIVER,"software");
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER)!=0){std::fprintf(stderr,"SDL2 self-test init failed: %s\n",SDL_GetError());return 1;}
    SDL_Window* window=SDL_CreateWindow("DQIX SDL2 self-test",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,
        kWidth/2,kHeight/2,SDL_WINDOW_HIDDEN);
    if(!window){std::fprintf(stderr,"SDL2 self-test window failed: %s\n",SDL_GetError());SDL_Quit();return 1;}
    SDL_Renderer* renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);
    if(!renderer){std::fprintf(stderr,"SDL2 self-test renderer failed: %s\n",SDL_GetError());SDL_DestroyWindow(window);SDL_Quit();return 1;}
    (void)SDL_RenderSetLogicalSize(renderer,kWidth,kHeight);
    LauncherState state; state.game_dir=fs::current_path(); state.settings_path=state.game_dir/"settings.ini"; state.log_path.clear();
    render_ui(renderer,state);
    SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
    std::puts("SDL2 software-renderer smoke test passed."); return 0;
}
} // namespace

int main(int argc,char** argv) {
    LauncherState state; bool show_help=false,self_test_requested=false; std::string argument_error;
    if(!parse_arguments(argc,argv,state,show_help,self_test_requested,argument_error)) {
        std::fprintf(stderr,"DQIX Linux launcher: %s\nRun --help for usage.\n",argument_error.c_str()); return 2;
    }
    if(show_help) {
        std::printf("DQIX Enhanced Linux Launcher (SDL2)\nUsage: %s [--game-dir PATH] [--self-test] [--help]\n"
                    "PATH is the built game folder containing dqix and data/.\n"
                    "--self-test runs a hidden SDL2 software-renderer smoke test.\n",argv[0]);
        return 0;
    }
    if(self_test_requested) return self_test();
    std::error_code ec; fs::create_directories(state.game_dir,ec);
    if(ec) std::fprintf(stderr,"Warning: cannot create game directory: %s\n",ec.message().c_str());
    log_message(state.log_path,"[launcher] SDL2 launcher starting\n[launcher] Game directory: %s\n",state.game_dir.c_str());
    (void)load_settings(state);
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER)!=0) {
        log_message(state.log_path,"[sdl2] SDL_Init failed: %s\n",SDL_GetError()); return 1;
    }
    SDL_Window* window=SDL_CreateWindow("DQIX Enhanced - Linux Launcher",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
        kWidth,kHeight,SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI);
    if(!window) {
        log_message(state.log_path,"[sdl2] SDL_CreateWindow failed: %s\n",SDL_GetError()); SDL_Quit(); return 1;
    }
    SDL_Renderer* renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    if(!renderer) renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);
    if(!renderer) {
        log_message(state.log_path,"[sdl2] SDL_CreateRenderer failed: %s\n",SDL_GetError());
        SDL_DestroyWindow(window); SDL_Quit(); return 1;
    }
    (void)SDL_RenderSetLogicalSize(renderer,kWidth,kHeight); (void)SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
    SDL_RendererInfo renderer_info{};
    if(SDL_GetRendererInfo(renderer,&renderer_info)==0)
        log_message(state.log_path,"[sdl2] renderer: %s\n",renderer_info.name);
    SDL_GameController* controller=nullptr;
    for(int i=0;i<SDL_NumJoysticks() && !controller;++i)
        if(SDL_IsGameController(i)) controller=SDL_GameControllerOpen(i);

    bool running=true;
    while(running) {
        SDL_Event event;
        while(SDL_PollEvent(&event)) {
            if(event.type==SDL_QUIT) running=false;
            else if(event.type==SDL_WINDOWEVENT && event.window.event==SDL_WINDOWEVENT_CLOSE) running=false;
            else if(event.type==SDL_MOUSEBUTTONUP && event.button.button==SDL_BUTTON_LEFT) {
                g_mouse_click=true; g_mouse_x=event.button.x; g_mouse_y=event.button.y;
            } else if(event.type==SDL_KEYDOWN && !event.key.repeat) {
                switch(event.key.keysym.sym) {
                case SDLK_ESCAPE: running=false; break;
                case SDLK_LEFT: state.page=static_cast<Page>((static_cast<int>(state.page)+7)%8);state.focus_index=0;break;
                case SDLK_RIGHT: state.page=static_cast<Page>((static_cast<int>(state.page)+1)%8);state.focus_index=0;break;
                case SDLK_UP: state.focus_index=(state.focus_index+state.previous_button_count-1)%state.previous_button_count;break;
                case SDLK_DOWN: case SDLK_TAB: state.focus_index=(state.focus_index+1)%state.previous_button_count;break;
                case SDLK_RETURN: case SDLK_SPACE: g_activate=true;break;
                default: break;
                }
            } else if(event.type==SDL_CONTROLLERBUTTONDOWN) {
                if(event.cbutton.button==SDL_CONTROLLER_BUTTON_B) running=false;
                else if(event.cbutton.button==SDL_CONTROLLER_BUTTON_DPAD_LEFT) {state.page=static_cast<Page>((static_cast<int>(state.page)+7)%8);state.focus_index=0;}
                else if(event.cbutton.button==SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {state.page=static_cast<Page>((static_cast<int>(state.page)+1)%8);state.focus_index=0;}
                else if(event.cbutton.button==SDL_CONTROLLER_BUTTON_DPAD_UP) state.focus_index=(state.focus_index+state.previous_button_count-1)%state.previous_button_count;
                else if(event.cbutton.button==SDL_CONTROLLER_BUTTON_DPAD_DOWN) state.focus_index=(state.focus_index+1)%state.previous_button_count;
                else if(event.cbutton.button==SDL_CONTROLLER_BUTTON_A) g_activate=true;
            }
        }
        render_ui(renderer,state); SDL_Delay(16);
    }
    if(controller) SDL_GameControllerClose(controller);
    SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit(); return 0;
}
