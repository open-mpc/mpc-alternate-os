// SPDX-License-Identifier: MIT
// The smallest alternate-OS app: it takes the screen over DRM, fills it with a colour band that moves, and leaves
// on SIGTERM, SIGINT or when --seconds runs out. The launcher (../launcher) stops MPC before and restores it after.
//   hello --self-test        checks it can run here without the hardware (what the launcher runs first)
//   hello --seconds N        runs N seconds (the launcher's time limit), else until stopped
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

static volatile sig_atomic_t stopping = 0;
static void on_signal(int sig) { (void)sig; stopping = 1; }

static double now_s(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }

int main(int argc, char** argv) {
    double seconds = 0;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--self-test")) { puts("hello: self-test OK"); return 0; }
        if (!strcmp(argv[i], "--seconds") && i + 1 < argc) seconds = atof(argv[++i]);
    }
    signal(SIGTERM, on_signal);
    signal(SIGINT, on_signal);

    int fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (fd < 0) { perror("hello: /dev/dri/card0"); return 1; }
    drmSetMaster(fd);
    drmModeRes* res = drmModeGetResources(fd);
    drmModeConnector* conn = NULL;
    for (int i = 0; res && i < res->count_connectors && !conn; ++i) {
        drmModeConnector* c = drmModeGetConnector(fd, res->connectors[i]);
        if (c && c->connection == DRM_MODE_CONNECTED && c->count_modes) conn = c; else drmModeFreeConnector(c);
    }
    if (!conn) { fputs("hello: no connected screen\n", stderr); return 1; }
    drmModeModeInfo mode = conn->modes[0];
    drmModeEncoder* enc = drmModeGetEncoder(fd, conn->encoder_id);
    uint32_t crtc_id = enc ? enc->crtc_id : res->crtcs[0];
    drmModeCrtc* previous = drmModeGetCrtc(fd, crtc_id); // put back on exit

    struct drm_mode_create_dumb create = {.width = mode.hdisplay, .height = mode.vdisplay, .bpp = 32};
    if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create)) { perror("hello: create buffer"); return 1; }
    uint32_t fb = 0;
    drmModeAddFB(fd, create.width, create.height, 24, 32, create.pitch, create.handle, &fb);
    struct drm_mode_map_dumb map = {.handle = create.handle};
    drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map);
    uint8_t* pixels = mmap(0, create.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, map.offset);
    if (pixels == MAP_FAILED) { perror("hello: map"); return 1; }
    if (drmModeSetCrtc(fd, crtc_id, fb, 0, 0, &conn->connector_id, 1, &mode)) { perror("hello: set mode"); return 1; }
    printf("hello: %ux%u screen, running\n", mode.hdisplay, mode.vdisplay);

    // The Live III's panel is 800 x 1280 portrait: the landscape screen's x runs down the buffer.
    const double start = now_s();
    while (!stopping && (seconds <= 0 || now_s() - start < seconds)) {
        const unsigned band = (unsigned)((now_s() - start) * 200) % create.height;
        for (uint32_t y = 0; y < create.height; ++y) {
            uint32_t* row = (uint32_t*)(pixels + (size_t)y * create.pitch);
            const uint32_t colour = (y >= band && y < band + 80) ? 0xffffb000u : 0xff202024u; // amber band on grey
            for (uint32_t x = 0; x < create.width; ++x) row[x] = colour;
        }
        drmModeClip clip = {0, 0, (uint16_t)create.width, (uint16_t)create.height};
        drmModeDirtyFB(fd, fb, &clip, 1);
        usleep(16000);
    }

    if (previous) drmModeSetCrtc(fd, previous->crtc_id, previous->buffer_id, previous->x, previous->y, &conn->connector_id, 1, &previous->mode);
    munmap(pixels, create.size);
    drmModeRmFB(fd, fb);
    drmDropMaster(fd);
    close(fd);
    puts("hello: done");
    return 0;
}
