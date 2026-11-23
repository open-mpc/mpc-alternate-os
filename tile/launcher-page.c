// launcher-page: the page of the "OpenMPC Launcher" tile. Lists the apps installed for mpc-launch and starts the
// one you tap: MPC stops, the app runs, MPC comes back when it ends. Draws to the modeslot live page host
// (/run/openmpc-page/frame.png, touches on input.sock). The PNG writer and font are from pagedemo (plugin-manager, MIT).
//   launcher-page [PAGE_DIR] [LAUNCHER_DIR]   (defaults /run/openmpc-page, /data/mpc-launcher)
#define _GNU_SOURCE
#include <dirent.h>
#include <sys/wait.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#define MAXW 1280
#define MAXH 800
static uint8_t px[MAXH][MAXW][3];
static int W = 1280, H = 800; // the frame size: the page's, once the shim says

static void fill(int x0, int y0, int x1, int y1, uint32_t c)
{
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > W) x1 = W;
    if (y1 > H) y1 = H;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            px[y][x][0] = (uint8_t)(c >> 16);
            px[y][x][1] = (uint8_t)(c >> 8);
            px[y][x][2] = (uint8_t)c;
        }
}

// 5 x 7 font: space, 0-9, A-Z, ':', '-', '.'
static const char *glyphs = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ:-.";
static const uint8_t font[][7] = {
    {0, 0, 0, 0, 0, 0, 0},
    {14, 17, 19, 21, 25, 17, 14}, {4, 12, 4, 4, 4, 4, 14}, {14, 17, 1, 2, 4, 8, 31}, {31, 2, 4, 2, 1, 17, 14},
    {2, 6, 10, 18, 31, 2, 2}, {31, 16, 30, 1, 1, 17, 14}, {6, 8, 16, 30, 17, 17, 14}, {31, 1, 2, 4, 8, 8, 8},
    {14, 17, 17, 14, 17, 17, 14}, {14, 17, 17, 15, 1, 2, 12},
    {14, 17, 17, 31, 17, 17, 17}, {30, 17, 17, 30, 17, 17, 30}, {14, 17, 16, 16, 16, 17, 14}, {28, 18, 17, 17, 17, 18, 28},
    {31, 16, 16, 30, 16, 16, 31}, {31, 16, 16, 30, 16, 16, 16}, {14, 17, 16, 23, 17, 17, 15}, {17, 17, 17, 31, 17, 17, 17},
    {14, 4, 4, 4, 4, 4, 14}, {7, 2, 2, 2, 2, 18, 12}, {17, 18, 20, 24, 20, 18, 17}, {16, 16, 16, 16, 16, 16, 31},
    {17, 27, 21, 21, 17, 17, 17}, {17, 17, 25, 21, 19, 17, 17}, {14, 17, 17, 17, 17, 17, 14}, {30, 17, 17, 30, 16, 16, 16},
    {14, 17, 17, 17, 21, 18, 13}, {30, 17, 17, 30, 20, 18, 17}, {15, 16, 16, 14, 1, 1, 30}, {31, 4, 4, 4, 4, 4, 4},
    {17, 17, 17, 17, 17, 17, 14}, {17, 17, 17, 17, 17, 10, 4}, {17, 17, 17, 21, 21, 21, 10}, {17, 17, 10, 4, 10, 17, 17},
    {17, 17, 10, 4, 4, 4, 4}, {31, 1, 2, 4, 8, 16, 31},
    {0, 12, 12, 0, 12, 12, 0}, {0, 0, 0, 31, 0, 0, 0}, {0, 0, 0, 0, 0, 12, 12},
};

static void text(int x, int y, int s, const char *t, uint32_t c)
{
    for (; *t; t++, x += 6 * s) {
        const char *g = strchr(glyphs, *t >= 'a' && *t <= 'z' ? *t - 32 : *t);
        if (!g) continue;
        const uint8_t *b = font[g - glyphs];
        for (int r = 0; r < 7; r++)
            for (int k = 0; k < 5; k++)
                if (b[r] & (16 >> k)) fill(x + k * s, y + r * s, x + (k + 1) * s, y + (r + 1) * s, c);
    }
}

static uint32_t crc_table[256];
static uint32_t crc(uint32_t c, const uint8_t *p, size_t n)
{
    c = ~c;
    while (n--) c = crc_table[(c ^ *p++) & 0xff] ^ (c >> 8);
    return ~c;
}
static void be32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }
static void chunk(FILE *f, const char *type, const uint8_t *d, uint32_t n)
{
    uint8_t h[8];
    be32(h, n);
    memcpy(h + 4, type, 4);
    fwrite(h, 1, 8, f);
    if (n) fwrite(d, 1, n, f);
    uint32_t c = crc(crc(0, (const uint8_t *)type, 4), d, n);
    be32(h, c);
    fwrite(h, 1, 4, f);
}

static int write_png(const char *path)
{
    size_t raw_n = (size_t)H * (1 + (size_t)W * 3);
    uint8_t *raw = malloc(raw_n);
    for (int y = 0; y < H; y++) {
        raw[(size_t)y * (1 + W * 3)] = 0;
        memcpy(raw + (size_t)y * (1 + W * 3) + 1, px[y], W * 3);
    }
    size_t blocks = (raw_n + 65534) / 65535;
    size_t z_n = 2 + raw_n + blocks * 5 + 4;
    uint8_t *z = malloc(z_n), *q = z;
    *q++ = 0x78; *q++ = 0x01;
    uint32_t a = 1, b = 0;
    for (size_t off = 0; off < raw_n; off += 65535) {
        size_t n = raw_n - off < 65535 ? raw_n - off : 65535;
        *q++ = off + n == raw_n;
        *q++ = (uint8_t)n; *q++ = (uint8_t)(n >> 8); *q++ = (uint8_t)~n; *q++ = (uint8_t)(~n >> 8);
        memcpy(q, raw + off, n);
        for (size_t i = 0; i < n;) { // Adler-32, the modulo every 4096 bytes
            size_t k = n - i < 4096 ? n - i : 4096;
            for (size_t j = 0; j < k; j++) { a += raw[off + i + j]; b += a; }
            a %= 65521; b %= 65521;
            i += k;
        }
        q += n;
    }
    be32(q, (b << 16) | a);
    q += 4;
    char tmp[300];
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    if (!f) { free(raw); free(z); return -1; }
    fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
    uint8_t ihdr[13];
    be32(ihdr, W); be32(ihdr + 4, H);
    ihdr[8] = 8; ihdr[9] = 2; ihdr[10] = ihdr[11] = ihdr[12] = 0;
    chunk(f, "IHDR", ihdr, 13);
    chunk(f, "IDAT", z, (uint32_t)(q - z));
    chunk(f, "IEND", NULL, 0);
    int ok = fclose(f) == 0;
    free(raw); free(z);
    return ok ? rename(tmp, path) : -1;
}


#define MAXAPPS 12
static struct { char name[64], title[64]; } apps[MAXAPPS];
static int napps, starting = -1;
static time_t started_at;
static const char *home = "/data/mpc-launcher";
#define ROW_Y 150
#define ROW_H 84
#define ROW_X 80

static void scan(void)
{
    napps = 0;
    char path[300];
    snprintf(path, sizeof path, "%s/apps", home);
    DIR *d = opendir(path);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) && napps < MAXAPPS) {
        if (e->d_name[0] == '.' || strlen(e->d_name) > 60) continue;
        snprintf(path, sizeof path, "%s/apps/%s/app.conf", home, e->d_name);
        FILE *f = fopen(path, "r");
        if (!f) continue;
        snprintf(apps[napps].name, 64, "%s", e->d_name);
        snprintf(apps[napps].title, 64, "%s", e->d_name);
        char line[256];
        while (fgets(line, sizeof line, f))
            if (!strncmp(line, "TITLE=\"", 7)) {
                char *q = strchr(line + 7, '"');
                if (q) { *q = 0; snprintf(apps[napps].title, 64, "%s", line + 7); }
            }
        fclose(f);
        napps++;
    }
    closedir(d);
    for (int i = 0; i < napps; i++) // name order
        for (int k = i + 1; k < napps; k++)
            if (strcmp(apps[k].title, apps[i].title) < 0) { __typeof__(apps[0]) t = apps[i]; apps[i] = apps[k]; apps[k] = t; }
}

static void draw(void)
{
    fill(0, 0, W, H, 0x1b1d21);
    text(ROW_X, 40, 6, "OPENMPC LAUNCHER", 0xf2f2f2);
    text(ROW_X, 100, 3, napps ? "TAP AN APP TO RUN IT. MPC COMES BACK WHEN IT ENDS." : "NO APPS INSTALLED", 0x9aa0a6);
    for (int i = 0; i < napps; i++) {
        int y = ROW_Y + i * (ROW_H + 10);
        if (y + ROW_H > H - 10) break;
        int busy = i == starting;
        fill(ROW_X, y, W - ROW_X, y + ROW_H, busy ? 0xe0a030 : 0x2d3036);
        text(ROW_X + 28, y + 22, 5, busy ? "STARTING..." : apps[i].title, busy ? 0x101010 : 0xf2f2f2);
    }
}

static int row_at(int x, int y)
{
    if (x < ROW_X || x >= W - ROW_X || y < ROW_Y) return -1;
    int i = (y - ROW_Y) / (ROW_H + 10);
    return i < napps && (y - ROW_Y) % (ROW_H + 10) < ROW_H ? i : -1;
}

static void run(int i)
{
    char cmd[300];
    snprintf(cmd, sizeof cmd, "%s/mpc-launch", home);
    pid_t p = fork();
    if (p == 0) {
        setsid();
        if (fork() == 0) {
            freopen("/dev/null", "r", stdin);
            freopen("/dev/null", "w", stdout);
            freopen("/dev/null", "w", stderr);
            execl(cmd, cmd, "start", apps[i].name, "0", (char *)NULL);
        }
        _exit(0);
    }
    if (p > 0) waitpid(p, NULL, 0);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "/run/openmpc-page";
    if (argc > 2) home = argv[2];
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++) c = c & 1 ? 0xedb88320u ^ (c >> 1) : c >> 1;
        crc_table[i] = c;
    }
    signal(SIGPIPE, SIG_IGN);
    mkdir(dir, 0755);
    char sock_path[108], frame[300];
    snprintf(sock_path, sizeof sock_path, "%s/input.sock", dir);
    snprintf(frame, sizeof frame, "%s/frame.png", dir);
    int s = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    struct sockaddr_un a = {.sun_family = AF_UNIX};
    snprintf(a.sun_path, sizeof a.sun_path, "%s", sock_path);
    unlink(sock_path);
    if (s < 0 || bind(s, (struct sockaddr *)&a, sizeof a) != 0) { perror("bind"); return 1; }
    int down_row = -1;
    scan();
    draw();
    write_png(frame);
    for (;;) {
        struct pollfd p = {.fd = s, .events = POLLIN};
        int r = poll(&p, 1, 1000);
        int dirty = 0;
        if (starting >= 0 && time(NULL) - started_at > 25) { starting = -1; dirty = 1; } // back from the app
        if (r > 0) {
            char buf[64];
            ssize_t n;
            while ((n = recv(s, buf, sizeof buf - 1, MSG_DONTWAIT)) > 0) {
                buf[n] = 0;
                int x, y;
                if (!strcmp(buf, "enter")) { scan(); dirty = 1; }
                else if (sscanf(buf, "size %d %d", &x, &y) == 2) {
                    if (x > 0 && y > 0 && x <= MAXW && y <= MAXH) { W = x; H = y; dirty = 1; }
                } else if (sscanf(buf, "down %d %d", &x, &y) == 2) down_row = row_at(x, y);
                else if (sscanf(buf, "up %d %d", &x, &y) == 2) {
                    int i = row_at(x, y);
                    if (i >= 0 && i == down_row && starting < 0) { // a tap that began and ended on the same row
                        starting = i; started_at = time(NULL);
                        fprintf(stderr, "launcher-page: start %s\n", apps[i].name);
                        draw(); write_png(frame);
                        run(i);
                    }
                    down_row = -1;
                    dirty = 1;
                }
            }
        }
        if (dirty) { draw(); write_png(frame); }
    }
}
