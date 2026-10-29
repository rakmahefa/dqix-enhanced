#include "game_process.hpp"

#include <cerrno>
#include <cstddef>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fcntl.h>
#include <string>
#include <system_error>

#include <sys/types.h>
#include <unistd.h>

namespace dqix::linux_ui {
namespace fs = std::filesystem;
namespace {

std::string local_timestamp() {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
    if (::localtime_r(&now, &local) == nullptr) return "time-unavailable";
    char buffer[40]{};
    if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S%z", &local) == 0)
        return "time-unavailable";
    return buffer;
}

bool write_all(int fd, const char* data, std::size_t size) {
    while (size > 0) {
        const ssize_t count = ::write(fd, data, size);
        if (count < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (count == 0) return false;
        data += count;
        size -= static_cast<std::size_t>(count);
    }
    return true;
}

bool write_log_record(int fd, const std::string& message) {
    std::string line = "[" + local_timestamp() + "] " + message;
    if (line.empty() || line.back() != '\n') line.push_back('\n');
    return write_all(fd, line.data(), line.size());
}

int open_append_no_follow(const fs::path& path) {
    if (path.empty()) return -1;
    return ::open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC | O_NOFOLLOW, 0600);
}

template <std::size_t N>
void child_error(const char (&message)[N], int exit_code) {
    const ssize_t written = ::write(STDERR_FILENO, message, N - 1);
    (void)written;
    _exit(exit_code);
}

} // namespace

bool append_devlog_message(const fs::path& path, const std::string& message) {
    const int fd = open_append_no_follow(path);
    if (fd < 0) return false;
    const bool ok = write_log_record(fd, message);
    (void)::close(fd);
    return ok;
}

GameProcessLaunch launch_game_process(const fs::path& workspace,
                                      const fs::path& game_dir,
                                      const fs::path& launcher_log,
                                      const std::string& version) {
    GameProcessLaunch result;
    const fs::path log_root = workspace.empty() ? game_dir : workspace;
    result.devlog_path = log_root / "devlog.log";
    const fs::path play_script_path = game_dir / "play.sh";
    std::error_code ec;
    result.used_play_script = fs::is_regular_file(play_script_path, ec) && !ec &&
                              ::access(play_script_path.c_str(), R_OK) == 0;
    const fs::path executable_path = game_dir / "dqix";
    const std::string game_cwd = game_dir.string();
    const std::string executable = executable_path.string();
    const std::string play_script = play_script_path.string();

    int output_fd = open_append_no_follow(result.devlog_path);
    result.devlog_enabled = output_fd >= 0;
    if (result.devlog_enabled) {
        result.output_log_path = result.devlog_path;
        (void)write_log_record(output_fd, "========== GAME SESSION START ==========");
        (void)write_log_record(output_fd, "[launcher] version=" + version);
        (void)write_log_record(output_fd, "[launcher] workspace=" + log_root.string());
        (void)write_log_record(output_fd, "[launcher] game_dir=" + game_dir.string());
        (void)write_log_record(output_fd, "[launcher] executable=" + executable);
        (void)write_log_record(output_fd, "[launcher] invocation=" +
                              std::string(result.used_play_script ? "game/play.sh" : "direct dqix fallback"));
        (void)write_log_record(output_fd,
            "[launcher] following output is game stdout/stderr, including renderer diagnostics emitted by the runtime");
    } else {
        const int devlog_error = errno;
        output_fd = open_append_no_follow(launcher_log);
        result.fallback_log_enabled = output_fd >= 0;
        if (result.fallback_log_enabled) {
            result.output_log_path = launcher_log;
            (void)write_log_record(output_fd, "[launcher] devlog.log unavailable (" +
                                  std::string(std::strerror(devlog_error)) +
                                  "); capturing game stdout/stderr in launcher.log");
            (void)write_log_record(output_fd, "========== GAME SESSION START (FALLBACK LOG) ==========");
            (void)write_log_record(output_fd, "[launcher] version=" + version);
            (void)write_log_record(output_fd, "[launcher] workspace=" + log_root.string());
            (void)write_log_record(output_fd, "[launcher] game_dir=" + game_dir.string());
            (void)write_log_record(output_fd, "[launcher] invocation=" +
                                  std::string(result.used_play_script ? "game/play.sh" : "direct dqix fallback"));
        }
    }

    const pid_t pid = ::fork();
    if (pid < 0) {
        const int fork_error = errno;
        result.error = "Could not start game: " + std::string(std::strerror(fork_error));
        if (output_fd >= 0)
            (void)write_log_record(output_fd, "[launcher] fork failed: " + std::string(std::strerror(fork_error)));
        if (output_fd >= 0) (void)::close(output_fd);
        return result;
    }
    if (pid == 0) {
        // Map the already-open log before chdir/exec so child setup errors are captured too.
        if (output_fd >= 0) {
            if (output_fd != STDOUT_FILENO && ::dup2(output_fd, STDOUT_FILENO) < 0)
                child_error("[launcher] could not redirect stdout to devlog\n", 126);
            if (output_fd != STDERR_FILENO && ::dup2(output_fd, STDERR_FILENO) < 0)
                child_error("[launcher] could not redirect stderr to devlog\n", 126);
            if (output_fd <= STDERR_FILENO) (void)::fcntl(output_fd, F_SETFD, 0);
            if (output_fd > STDERR_FILENO) (void)::close(output_fd);
        }
        if (::chdir(game_cwd.c_str()) != 0)
            child_error("[launcher] chdir to game directory failed\n", 126);
        if (result.used_play_script) {
            char* const play_args[] = {
                const_cast<char*>("bash"), const_cast<char*>(play_script.c_str()), nullptr
            };
            ::execv("/bin/bash", play_args);
            child_error("[launcher] exec /bin/bash play.sh failed\n", 127);
        }
        // Compatibility fallback for workspaces that predate play.sh.
        char* const args[] = {
            const_cast<char*>(executable.c_str()), const_cast<char*>("data/dqix.nds"),
            const_cast<char*>("data/code"), const_cast<char*>("0"), const_cast<char*>("1"),
            const_cast<char*>("dqix.sav"), nullptr
        };
        ::execv(executable.c_str(), args);
        child_error("[launcher] exec dqix failed\n", 127);
    }

    result.pid = pid;
    if (output_fd >= 0) {
        (void)write_log_record(output_fd, "[game] process started pid=" + std::to_string(pid));
        (void)::close(output_fd);
    }
    return result;
}

} // namespace dqix::linux_ui
