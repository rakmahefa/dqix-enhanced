#include "build_job.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;
using dqix::linux_ui::BuildEvent;
using dqix::linux_ui::BuildJob;
using dqix::linux_ui::BuildRequest;

namespace {
bool require(bool condition, const std::string& message) {
    if (!condition) std::cerr << "FAIL: " << message << '\n';
    return condition;
}
void write_file(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out << text;
}
}

int main() {
    const fs::path temp = fs::temp_directory_path() /
        ("dqix-linux-builder-test-" + std::to_string(static_cast<long long>(::getpid())));
    std::error_code ec;
    fs::remove_all(temp, ec);
    fs::create_directories(temp);

    const fs::path rom = temp / "fixture.nds";
    write_file(rom, "synthetic ROM placeholder for builder integration test only\n");
    write_file(temp / "fixture.txt", "not a ROM\n");

    bool ok = true;
    std::string error;
    ok &= require(dqix::linux_ui::valid_rom_path(rom, error), "synthetic .nds fixture must validate");
    error.clear();
    ok &= require(!dqix::linux_ui::valid_rom_path(temp / "fixture.txt", error),
                  "non-.nds file must be rejected");

    const fs::path success_source = temp / "source-success";
    write_file(success_source / "tools/build-game.sh", R"script(#!/bin/bash
set -eu
echo "== Checking the ROM"
mkdir -p game/data/code
printf '#!/bin/sh\nexit 0\n' > game/dqix
chmod +x game/dqix
printf 'synthetic generated ROM placeholder\n' > game/data/dqix.nds
touch game/data/code/generated.c
echo "== Done."
)script");
    write_file(success_source / "game/should-not-be-copied.txt", "must be excluded\n");
    write_file(success_source / ".git/config", "must be excluded\n");
    const fs::path success_workspace = temp / "workspace-success";
    error.clear();
    ok &= require(dqix::linux_ui::validate_workspace_path(success_source, success_workspace, error),
                  "new nested workspace should validate: " + error);
    error.clear();
    ok &= require(!dqix::linux_ui::validate_workspace_path(success_source, success_source, error),
                  "workspace equal to source must be rejected");

    const fs::path merge = temp / "existing-unmarked";
    write_file(merge / "keep-me.txt", "user data\n");
    ok &= require(dqix::linux_ui::workspace_needs_merge_confirmation(merge),
                  "non-empty unmarked workspace must require confirmation");
    write_file(merge / ".dqix-enhanced-appimage-workspace", "test marker\n");
    ok &= require(!dqix::linux_ui::workspace_needs_merge_confirmation(merge),
                  "marked workspace should not ask for merge confirmation");

    BuildRequest request;
    request.source_root = success_source;
    request.workspace = success_workspace;
    request.rom = rom;
    BuildJob success_job;
    error.clear();
    ok &= require(success_job.start(request, error), "simulated successful build should start: " + error);
    success_job.wait();
    bool saw_success=false, saw_progress=false;
    for (const auto& event : success_job.drain_events()) {
        saw_success |= event.kind == BuildEvent::Kind::Succeeded;
        saw_progress |= event.kind == BuildEvent::Kind::Progress && event.percent >= 100;
    }
    ok &= require(saw_success, "successful child should emit Succeeded");
    ok &= require(saw_progress, "successful child should emit final progress");
    ok &= require(dqix::linux_ui::game_directory_ready(success_workspace / "game"),
                  "simulated game output should pass readiness checks");
    ok &= require(fs::is_regular_file(success_workspace / "launcher-build.log"),
                  "build output should be persisted to launcher-build.log");
    ok &= require(!fs::exists(success_workspace / "game/should-not-be-copied.txt"),
                  "source staging must preserve the game's generated output area");
    ok &= require(!fs::exists(success_workspace / ".git/config"),
                  "source staging must exclude .git metadata");

    const fs::path failure_source = temp / "source-failure";
    write_file(failure_source / "tools/build-game.sh",
               "#!/bin/bash\nset -eu\necho 'simulated build failure'\nexit 7\n");
    const fs::path failure_workspace = temp / "workspace-failure";
    BuildRequest fail_request;
    fail_request.source_root = failure_source;
    fail_request.workspace = failure_workspace;
    fail_request.rom = rom;
    BuildJob failure_job;
    error.clear();
    ok &= require(failure_job.start(fail_request, error), "simulated failing build should start: " + error);
    failure_job.wait();
    bool saw_failure=false;
    for (const auto& event : failure_job.drain_events())
        saw_failure |= event.kind == BuildEvent::Kind::Failed;
    ok &= require(saw_failure, "non-zero child exit must emit Failed");
    ok &= require(fs::is_regular_file(failure_workspace / "launcher-build.log"),
                  "failed build diagnostics must be persisted");

    fs::remove_all(temp, ec);
    if (!ok) return 1;
    std::cout << "Native SDL2 builder dry-run tests passed (synthetic ROM fixture; no game data).\n";
    return 0;
}
