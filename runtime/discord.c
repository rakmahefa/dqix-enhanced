/* Discord: "Playing Dragon Quest IX" on the player's profile (Rich Presence),
 * through the Discord app's local connection (a named pipe on Windows, a
 * socket elsewhere). Nothing is sent anywhere else, and nothing happens when
 * Discord isn't running. Modern > "Show on Discord what I'm playing".
 *
 * The protocol: frames of { opcode (u32), length (u32), JSON }. A handshake
 * (opcode 0) with the application's ID, then SET_ACTIVITY commands (opcode 1).
 * All of it runs on a thread of its own, so a slow or stuck Discord never
 * holds up the game. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "SDL.h"
#include "discord.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#endif

/* The project's Discord application (its ID is not a secret: every program
 * that shows an activity sends it). discord_app_id in settings.ini overrides it. */
#ifndef DISCORD_APP_ID
#define DISCORD_APP_ID "1564002016639062076"
#endif

static char g_app_id[32] = DISCORD_APP_ID;
static SDL_mutex* g_mx;
static SDL_Thread* g_thread;
static int g_on, g_quit;
static char g_details[128], g_state[128];
static int g_changed;
static int64_t g_start;

#ifdef _WIN32
static HANDLE g_pipe = INVALID_HANDLE_VALUE;
static int conn_open(void) {
    for (int i = 0; i < 10; i++) {
        char name[64]; snprintf(name, sizeof name, "\\\\.\\pipe\\discord-ipc-%d", i);
        HANDLE h = CreateFileA(name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (h != INVALID_HANDLE_VALUE) { g_pipe = h; return 1; }
    }
    return 0;
}
static void conn_close(void) { if (g_pipe != INVALID_HANDLE_VALUE) CloseHandle(g_pipe); g_pipe = INVALID_HANDLE_VALUE; }
static int conn_write(const void* p, int n) { DWORD w = 0; return WriteFile(g_pipe, p, (DWORD)n, &w, NULL) && (int)w == n; }
static int conn_read(void* p, int n) {             /* whatever is waiting (0: nothing yet, -1: closed) */
    DWORD avail = 0;
    if (!PeekNamedPipe(g_pipe, NULL, 0, NULL, &avail, NULL)) return -1;
    if (!avail) return 0;
    DWORD r = 0; if (!ReadFile(g_pipe, p, (DWORD)(avail < (DWORD)n ? avail : (DWORD)n), &r, NULL)) return -1;
    return (int)r;
}
static int my_pid(void) { return (int)GetCurrentProcessId(); }
#else
static int g_fd = -1;
static int conn_open(void) {
    const char* dirs[] = { getenv("XDG_RUNTIME_DIR"), getenv("TMPDIR"), "/tmp" };
    for (int d = 0; d < 3; d++) {
        if (!dirs[d]) continue;
        for (int i = 0; i < 10; i++) {
            struct sockaddr_un a; memset(&a, 0, sizeof a); a.sun_family = AF_UNIX;
            snprintf(a.sun_path, sizeof a.sun_path, "%s/discord-ipc-%d", dirs[d], i);
            int fd = socket(AF_UNIX, SOCK_STREAM, 0);
            if (fd < 0) return 0;
            if (connect(fd, (struct sockaddr*)&a, sizeof a) == 0) { fcntl(fd, F_SETFL, O_NONBLOCK); g_fd = fd; return 1; }
            close(fd);
        }
    }
    return 0;
}
static void conn_close(void) { if (g_fd >= 0) close(g_fd); g_fd = -1; }
static int conn_write(const void* p, int n) {
    const char* c = p;
    while (n > 0) { ssize_t w = send(g_fd, c, (size_t)n, 0); if (w <= 0) { if (w < 0) { SDL_Delay(5); continue; } return 0; } c += w; n -= (int)w; }
    return 1;
}
static int conn_read(void* p, int n) { ssize_t r = recv(g_fd, p, (size_t)n, 0); return r > 0 ? (int)r : r == 0 ? -1 : 0; }
static int my_pid(void) { return (int)getpid(); }
#endif

static int send_frame(uint32_t op, const char* json) {
    uint32_t len = (uint32_t)strlen(json);
    uint8_t head[8]; memcpy(head, &op, 4); memcpy(head + 4, &len, 4);
    return conn_write(head, 8) && conn_write(json, (int)len);
}

/* JSON string contents: quotes and backslashes escaped, control characters left out */
static void esc(char* out, size_t n, const char* s) {
    size_t k = 0;
    for (; *s && k + 3 < n; s++) {
        if (*s == '"' || *s == '\\') { out[k++] = '\\'; out[k++] = *s; }
        else if ((unsigned char)*s >= 0x20) out[k++] = *s;
    }
    out[k] = 0;
}

static int thread_main(void* arg) {
    (void)arg;
    int connected = 0, ready = 0; uint32_t next_try = 0, nonce = 0;
    while (!g_quit) {
        SDL_LockMutex(g_mx);
        int on = g_on && g_app_id[0], changed = g_changed;
        char det[256], st[256]; esc(det, sizeof det, g_details); esc(st, sizeof st, g_state);
        SDL_UnlockMutex(g_mx);
        if (!on) { if (connected) { conn_close(); connected = ready = 0; } SDL_Delay(200); continue; }
        if (!connected) {
            if (SDL_GetTicks() < next_try) { SDL_Delay(200); continue; }
            next_try = SDL_GetTicks() + 20000;                      /* Discord not running: try again now and then */
            if (!conn_open()) continue;
            char hs[128]; snprintf(hs, sizeof hs, "{\"v\":1,\"client_id\":\"%s\"}", g_app_id);
            if (!send_frame(0, hs)) { conn_close(); continue; }
            connected = 1; ready = 0;
        }
        char buf[4096]; int r = conn_read(buf, sizeof buf - 1);
        if (r < 0) { conn_close(); connected = ready = 0; continue; }
        if (r > 0) {                                                /* frames start with an 8-byte binary header (zeros in it): search all r bytes */
            for (int i = 0; i + 7 <= r && !ready; i++) if (!memcmp(buf + i, "\"READY\"", 7)) { ready = 1; changed = 1; }
            if (getenv("DQIX_DISCORDLOG")) { for (int i = 0; i < r; i++) if (!buf[i]) buf[i] = '.'; buf[r] = 0; fprintf(stderr, "discord: %s\n", buf); }
        }
        if (ready && changed) {
            char js[1024];
            snprintf(js, sizeof js,
                "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":%d,\"activity\":{%s%s%s\"state\":\"%s\",\"timestamps\":{\"start\":%lld},"
                "\"assets\":{\"large_image\":\"logo\",\"large_text\":\"Dragon Quest IX\"}}},\"nonce\":\"%u\"}",
                my_pid(), det[0] ? "\"details\":\"" : "", det, det[0] ? "\"," : "", st, (long long)g_start, ++nonce);
            if (!send_frame(1, js)) { conn_close(); connected = ready = 0; continue; }
            SDL_LockMutex(g_mx); g_changed = 0; SDL_UnlockMutex(g_mx);
        }
        SDL_Delay(250);
    }
    if (connected) conn_close();
    return 0;
}

void discord_set_app_id(const char* id) {
    if (!id || !*id) return;
    if (!g_mx) g_mx = SDL_CreateMutex();
    SDL_LockMutex(g_mx); snprintf(g_app_id, sizeof g_app_id, "%s", id); g_changed = 1; SDL_UnlockMutex(g_mx);
}
int discord_available(void) { return g_app_id[0] != 0; }

void discord_enable(int on) {
    if (!g_mx) g_mx = SDL_CreateMutex();
    if (!g_start) g_start = (int64_t)time(NULL);
    SDL_LockMutex(g_mx); g_on = on != 0; g_changed = 1; SDL_UnlockMutex(g_mx);
    if (on && !g_thread && g_app_id[0]) g_thread = SDL_CreateThread(thread_main, "discord", NULL);
}

void discord_activity(const char* details, const char* state) {
    if (!g_mx) return;
    SDL_LockMutex(g_mx);
    if (strcmp(details ? details : "", g_details) || strcmp(state ? state : "", g_state)) {
        snprintf(g_details, sizeof g_details, "%s", details ? details : "");
        snprintf(g_state, sizeof g_state, "%s", state ? state : "");
        g_changed = 1;
    }
    SDL_UnlockMutex(g_mx);
}

void discord_quit(void) {
    if (!g_thread) return;
    g_quit = 1; SDL_WaitThread(g_thread, NULL); g_thread = NULL;
}
