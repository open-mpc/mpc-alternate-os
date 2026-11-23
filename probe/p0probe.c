// Phase 0 probe for the MPC Live III with the MPC app stopped.
//   p0probe tone [seconds] [first_channel]   sine on two channels of hw:III (30 ch, S32_LE, 44.1 kHz, 128 frames)
//   p0probe card                             test card into /dev/fb0 (800 x 1280, drawn as 1280 x 800 landscape)
//   p0probe drm [seconds]                    test card through DRM (mode set, dumb buffer), held for the time given
//   p0probe touch [seconds]                  print touches from the ILI210x input device in landscape coordinates
// Build: see build.sh (arm64 Debian, against libasound).
#include <alsa/asoundlib.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

static double now(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

static int tone(double seconds, int first) {
    const unsigned channels = 30, rate = 44100;
    snd_pcm_uframes_t period = 128, buffer = 384;
    snd_pcm_t *pcm;
    int err = snd_pcm_open(&pcm, "hw:III", SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) { printf("tone: open hw:III: %s\n", snd_strerror(err)); return 1; }
    snd_pcm_hw_params_t *hw;
    snd_pcm_hw_params_alloca(&hw);
    snd_pcm_hw_params_any(pcm, hw);
    snd_pcm_hw_params_set_access(pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(pcm, hw, SND_PCM_FORMAT_S32_LE);
    snd_pcm_hw_params_set_channels(pcm, hw, channels);
    unsigned r = rate;
    snd_pcm_hw_params_set_rate_near(pcm, hw, &r, 0);
    snd_pcm_hw_params_set_period_size_near(pcm, hw, &period, 0);
    snd_pcm_hw_params_set_buffer_size_near(pcm, hw, &buffer);
    if ((err = snd_pcm_hw_params(pcm, hw)) < 0) { printf("tone: hw_params: %s\n", snd_strerror(err)); return 1; }
    printf("tone: rate %u period %lu buffer %lu channels %d+%d\n", r, period, buffer, first, first + 1);
    int32_t *buf = calloc(period * channels, sizeof(int32_t));
    double phase = 0, step = 2 * M_PI * 440.0 / r, end = now() + seconds;
    long blocks = 0, xruns = 0;
    while (now() < end) {
        memset(buf, 0, period * channels * sizeof(int32_t));
        for (snd_pcm_uframes_t i = 0; i < period; i++) {
            // -18 dBFS, a short fade in and out each half second so the start is easy to hear
            double env = fabs(sin(M_PI * fmod(blocks * period + i, r / 2.0) / (r / 2.0)));
            int32_t v = (int32_t)(sin(phase) * env * 0.125 * 2147483647.0);
            phase += step;
            if (first >= 0 && first + 1 < (int)channels) {
                buf[i * channels + first] = v;
                buf[i * channels + first + 1] = v;
            }
        }
        snd_pcm_sframes_t n = snd_pcm_writei(pcm, buf, period);
        if (n < 0) { xruns++; snd_pcm_recover(pcm, (int)n, 1); }
        blocks++;
    }
    snd_pcm_drain(pcm);
    snd_pcm_close(pcm);
    printf("tone: %ld blocks, %ld xruns\n", blocks, xruns);
    return 0;
}

static int card(void) {
    int fd = open("/dev/fb0", O_RDWR);
    if (fd < 0) { perror("card: /dev/fb0"); return 1; }
    struct fb_var_screeninfo v;
    struct fb_fix_screeninfo f;
    ioctl(fd, FBIOGET_VSCREENINFO, &v);
    ioctl(fd, FBIOGET_FSCREENINFO, &f);
    printf("card: %ux%u %u bpp stride %u\n", v.xres, v.yres, v.bits_per_pixel, f.line_length);
    size_t size = (size_t)f.line_length * v.yres;
    uint8_t *fb = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (fb == MAP_FAILED) { perror("card: mmap"); return 1; }
    // Landscape picture W x H; the panel is portrait, so landscape (x, y) -> portrait (px, py).
    const int W = 1280, H = 800;
    const uint32_t bars[8] = {0xffffff, 0xffff00, 0x00ffff, 0x00ff00, 0xff00ff, 0xff0000, 0x0000ff, 0x202020};
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            uint32_t c = bars[x * 8 / W];
            if (y > H * 2 / 3) c = (uint32_t)(x * 255 / W) * 0x010101;  // grey ramp along the bottom third
            if (x < 120 && y < 120) c = 0xff0000;                         // red square: landscape top left
            if (x > W - 120 && y > H - 120) c = 0x00ff00;                 // green square: bottom right
            if (x == 0 || y == 0 || x == W - 1 || y == H - 1) c = 0xffffff;
            // Try the quarter turn one way; the user reports where the red square appears.
            int px = y, py = (W - 1) - x;
            if (px < (int)v.xres && py < (int)v.yres)
                *(uint32_t *)(fb + (size_t)py * f.line_length + (size_t)px * 4) = c;
        }
    munmap(fb, size);
    close(fd);
    printf("card: drawn\n");
    return 0;
}

// The DRM way: own the display, set the panel's mode with a dumb buffer, draw the test card, hold it.
static int drm(double seconds) {
    int fd = -1;
    drmModeRes *res = NULL;
    char path[32];
    for (int c = 0; c < 4 && !res; c++) {
        snprintf(path, sizeof path, "/dev/dri/card%d", c);
        fd = open(path, O_RDWR | O_CLOEXEC);
        if (fd < 0) continue;
        res = drmModeGetResources(fd);
        if (!res || res->count_connectors == 0) { if (res) drmModeFreeResources(res); res = NULL; close(fd); }
    }
    if (!res) { printf("drm: no card with connectors\n"); return 1; }
    printf("drm: %s, master %s\n", path, drmSetMaster(fd) == 0 ? "taken" : "not taken (already ours or busy)");
    drmModeConnector *conn = NULL;
    for (int i = 0; i < res->count_connectors && !conn; i++) {
        drmModeConnector *c = drmModeGetConnector(fd, res->connectors[i]);
        if (c && c->connection == DRM_MODE_CONNECTED && c->count_modes > 0) conn = c;
        else if (c) drmModeFreeConnector(c);
    }
    if (!conn) { printf("drm: no connected connector\n"); return 1; }
    drmModeModeInfo mode = conn->modes[0];
    uint32_t crtc = 0;
    if (conn->encoder_id) {
        drmModeEncoder *e = drmModeGetEncoder(fd, conn->encoder_id);
        if (e) { crtc = e->crtc_id; drmModeFreeEncoder(e); }
    }
    for (int i = 0; !crtc && i < conn->count_encoders; i++) {
        drmModeEncoder *e = drmModeGetEncoder(fd, conn->encoders[i]);
        for (int j = 0; e && j < res->count_crtcs; j++)
            if (e->possible_crtcs & (1u << j)) { crtc = res->crtcs[j]; break; }
        if (e) drmModeFreeEncoder(e);
    }
    printf("drm: connector %u mode %s %ux%u@%u crtc %u\n", conn->connector_id, mode.name, mode.hdisplay, mode.vdisplay,
           mode.vrefresh, crtc);
    struct drm_mode_create_dumb cd = {.width = mode.hdisplay, .height = mode.vdisplay, .bpp = 32};
    if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &cd)) { perror("drm: create dumb"); return 1; }
    uint32_t fb_id;
    if (drmModeAddFB(fd, cd.width, cd.height, 24, 32, cd.pitch, cd.handle, &fb_id)) { perror("drm: addfb"); return 1; }
    struct drm_mode_map_dumb md = {.handle = cd.handle};
    drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &md);
    uint8_t *px = mmap(0, cd.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, md.offset);
    if (px == MAP_FAILED) { perror("drm: mmap"); return 1; }
    const int W = 1280, H = 800;
    const uint32_t bars[8] = {0xffffff, 0xffff00, 0x00ffff, 0x00ff00, 0xff00ff, 0xff0000, 0x0000ff, 0x202020};
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            uint32_t c = bars[x * 8 / W];
            if (y > H * 2 / 3) c = (uint32_t)(x * 255 / W) * 0x010101;
            if (x < 120 && y < 120) c = 0xff0000;
            if (x > W - 120 && y > H - 120) c = 0x00ff00;
            if (x == 0 || y == 0 || x == W - 1 || y == H - 1) c = 0xffffff;
            int qx = y, qy = (W - 1) - x;
            if (qx < (int)cd.width && qy < (int)cd.height) *(uint32_t *)(px + (size_t)qy * cd.pitch + (size_t)qx * 4) = c;
        }
    int err = drmModeSetCrtc(fd, crtc, fb_id, 0, 0, &conn->connector_id, 1, &mode);
    printf("drm: set crtc %s\n", err ? strerror(-err) : "ok");
    double t0 = now();
    while (now() - t0 < seconds) usleep(100000);
    drmModeRmFB(fd, fb_id);
    close(fd);
    printf("drm: released\n");
    return 0;
}

static int open_touch(char *name, size_t len) {
    for (int i = 0; i < 16; i++) {
        char path[32];
        snprintf(path, sizeof path, "/dev/input/event%d", i);
        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;
        name[0] = 0;
        ioctl(fd, EVIOCGNAME(len), name);
        if (strstr(name, "Touch") || strstr(name, "ILI")) return fd;
        close(fd);
    }
    return -1;
}

static int touch(double seconds) {
    char name[64] = "";
    int fd = open_touch(name, sizeof name);
    if (fd < 0) { printf("touch: no touchscreen found\n"); return 1; }
    struct input_absinfo ax, ay;
    ioctl(fd, EVIOCGABS(ABS_MT_POSITION_X), &ax);
    ioctl(fd, EVIOCGABS(ABS_MT_POSITION_Y), &ay);
    printf("touch: %s x %d..%d y %d..%d now %d,%d\n", name, ax.minimum, ax.maximum, ay.minimum, ay.maximum, ax.value, ay.value);
    int slot = 0, x[10] = {0}, y[10] = {0}, down[10] = {0};
    long events = 0;
    double end = now() + seconds;
    while (now() < end) {
        struct input_event e;
        ssize_t n = read(fd, &e, sizeof e);
        if (n != sizeof e) { usleep(2000); continue; }
        events++;
        if (e.type == EV_ABS) {
            if (e.code == ABS_MT_SLOT && e.value >= 0 && e.value < 10) slot = e.value;
            else if (e.code == ABS_MT_POSITION_X) x[slot] = e.value;
            else if (e.code == ABS_MT_POSITION_Y) y[slot] = e.value;
            else if (e.code == ABS_MT_TRACKING_ID) down[slot] = e.value >= 0;
        } else if (e.type == EV_SYN && e.code == SYN_REPORT) {
            for (int s = 0; s < 10; s++)
                if (down[s]) printf("touch: slot %d raw %d,%d\n", s, x[s], y[s]);
        }
    }
    printf("touch: %ld events\n", events);
    close(fd);
    return 0;
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (argc > 1 && !strcmp(argv[1], "tone")) return tone(argc > 2 ? atof(argv[2]) : 3, argc > 3 ? atoi(argv[3]) : 0);
    if (argc > 1 && !strcmp(argv[1], "card")) return card();
    if (argc > 1 && !strcmp(argv[1], "drm")) return drm(argc > 2 ? atof(argv[2]) : 10);
    if (argc > 1 && !strcmp(argv[1], "touch")) return touch(argc > 2 ? atof(argv[2]) : 10);
    fprintf(stderr, "usage: p0probe tone [s] [first_channel] | card | drm [s] | touch [s]\n");
    return 2;
}
