/* tests/relay_bench: what a session costs the relay, counted at the relay.
 *
 * The game talks to the relay through relay.c; the relay counts every message
 * it receives, and its daily allowance is spent per message. This harness
 * plays the two sides of a co-op session against a stand-in relay
 * (server.py), one process per player, and sends what a session sends:
 * one small message per frame, every frame.
 *
 * usage: bench host <code> <port> <friends> <seconds> [fps]
 *        bench friend <code> <seconds> [fps]
 *
 * The host listens on <port> for the connections the relay's bridge makes (the
 * game's own listening port) and writes to every friend on each frame, as the
 * session does for its own inputs and for the inputs it passes on.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include "relay.h"

#define MAXFRIENDS 8

static double now_ms(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}
static void sleep_ms(double ms) {
    if (ms <= 0) return;
    struct timespec t = { (time_t)(ms / 1000), (long)((ms - (double)(time_t)(ms / 1000) * 1000) * 1e6) };
    nanosleep(&t, NULL);
}
static int listen_on(int port) {
    int s = socket(AF_INET, SOCK_STREAM, 0), one = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&one, sizeof one);
    struct sockaddr_in a; memset(&a, 0, sizeof a);
    a.sin_family = AF_INET; a.sin_port = htons((unsigned short)port); a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(s, (struct sockaddr*)&a, sizeof a) != 0 || listen(s, MAXFRIENDS) != 0) { perror("listen"); exit(3); }
    return s;
}
static int connect_to(int port) {
    int s = socket(AF_INET, SOCK_STREAM, 0), one = 1;
    struct sockaddr_in a; memset(&a, 0, sizeof a);
    a.sin_family = AF_INET; a.sin_port = htons((unsigned short)port); a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(s, (struct sockaddr*)&a, sizeof a) != 0) { perror("connect"); exit(4); }
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char*)&one, sizeof one);
    return s;
}
/* the same size as a session's M_INPUT: a byte, a frame number, one input */
static size_t fill(unsigned char* b, unsigned long long frame) {
    memset(b, 0x5a, 18);
    b[0] = 7; b[1] = (unsigned char)frame; b[2] = (unsigned char)(frame >> 8);
    return 18;
}
static void drain(int s) {
    char tmp[512]; while (recv(s, tmp, sizeof tmp, MSG_DONTWAIT) > 0) { }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: bench host <code> <port> <friends> <seconds> [fps]\n"
                        "       bench friend <code> <seconds> [fps]\n");
        return 2;
    }
    int host = !strcmp(argv[1], "host");
    const char* code = argv[2];
    int friends = 0, port = 0;
    double seconds, fps = 59.83;
    int next = 3;
    if (host) {
        port = atoi(argv[3]); friends = atoi(argv[4]); seconds = atof(argv[5]);
        if (argc > 6) fps = atof(argv[6]);
        next = 6;
    } else {
        seconds = atof(argv[3]);
        if (argc > 4) fps = atof(argv[4]);
    }
    (void)next;

    int conns[MAXFRIENDS]; int nconn = 0;
    if (host) {
        int l = listen_on(port);                       /* the session's own listening port */
        char why[300] = "";
        if (!relay_host_start(code, port, why, sizeof why)) { fprintf(stderr, "host: %s\n", why); return 5; }
        double t0 = now_ms();
        while (nconn < friends && now_ms() - t0 < 5000) {   /* the bridge connects each friend to us */
            fd_set rd; FD_ZERO(&rd); FD_SET(l, &rd);
            struct timeval tv = { 0, 50000 };
            if (select(l + 1, &rd, NULL, NULL, &tv) > 0) {
                int s = accept(l, NULL, NULL), one = 1;
                setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char*)&one, sizeof one);
                conns[nconn++] = s;
            }
        }
        close(l);
        if (nconn < friends) { fprintf(stderr, "host: only %d of %d friends arrived\n", nconn, friends); return 6; }
    } else {
        int lp = 0;
        char why[300] = "";
        if (!relay_join_start(code, &lp, why, sizeof why)) { fprintf(stderr, "friend: %s\n", why); return 5; }
        conns[nconn++] = connect_to(lp);               /* the session connects through it */
    }

    unsigned char buf[64];
    double period = 1000.0 / fps, next_at = now_ms();
    unsigned long long frames = 0;
    for (;;) {
        double t = now_ms();
        if (t - next_at > 200) next_at = t;            /* fell behind: resynchronise */
        if (next_at >= now_ms() + 0.05) { sleep_ms(next_at - t); }
        size_t n = fill(buf, frames);
        for (int i = 0; i < nconn; i++) {
            if (send(conns[i], buf, n, 0) <= 0) { fprintf(stderr, "send failed on connection %d\n", i); return 7; }
            drain(conns[i]);
        }
        frames++;
        next_at += period;
        if ((double)frames / fps >= seconds) break;
    }
    sleep_ms(120);                                     /* let the bridge flush what is left */
    printf("%s: %llu frames in %.1f s (%.1f fps)\n", host ? "host" : "friend",
           (unsigned long long)frames, (double)frames / fps, fps);
    return 0;
}
