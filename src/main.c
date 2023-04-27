#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdbool.h>
#include "aids.h"

#include "xdg-shell-client-protocol.h"


typedef struct state {
    struct wl_display *wl_display;
    struct wl_registry *wl_registry;
    struct wl_compositor *wl_compositor;
    struct wl_shm *wl_shm;

    struct xdg_wm_base *xdg_wm_base;
    struct wl_surface *wl_surface;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;
    
    uint8_t *pix_data;
    struct wl_buffer *buffer;
    size_t width;
    size_t height;
    size_t stride;
;

    bool is_closed;
} State;

static void randname(char *buf) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    long r = ts.tv_nsec;
    for (size_t i = 0; i < 6; i += 1) {
        buf[i] = 'A' + (r & 15) + (r & 16) * 2;
        r >>= 5;
    }
}

static int create_shm_file(void) {
    int retries = 100;

    do {
        char name[] = "/wl_shm-XXXXXX";
        randname(name + sizeof(name) - 7);
        retries -= 1;

        int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);

        if (fd >= 0) {
            shm_unlink(name);
            return fd;
        }
    } while (retries > 0 && errno == EEXIST);

    return -1;
}

int allocate_shm_file(size_t size) {
    int fd = create_shm_file();

    if (fd < 0) {
        return -1;
    }

    int ret;
    do {
        ret = ftruncate(fd, size);
    } while (ret < 0 && errno == EINTR);

    if (ret < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

static void registry_handle_global(
    void *data,
    struct wl_registry *registry,
    uint32_t name,
    const char *interface,
    uint32_t version
) {

    State *state = data;

    if (strcmp(interface, wl_shm_interface.name) == 0) {
        state->wl_shm = wl_registry_bind(state->wl_registry, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, wl_compositor_interface.name) == 0) {
        state->wl_compositor = wl_registry_bind(state->wl_registry, name, &wl_compositor_interface, 4);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        state->xdg_wm_base = wl_registry_bind(state->wl_registry, name, &xdg_wm_base_interface, 1);
    }
}

static void registry_handle_global_remove(
    void *data,
    struct wl_registry *registry,
    uint32_t name
) {
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_handle_global,
    .global_remove = registry_handle_global_remove,
};

static void wl_buffer_release(void *data, struct wl_buffer *wl_buffer) {
    wl_buffer_destroy(wl_buffer);
}

static const struct wl_buffer_listener wl_buffer_listener = {
    .release = wl_buffer_release,
};

void draw(State *state) {
    for (size_t y = 0; y < state->height; y += 1) {
        for (size_t x = 0; x < state->width; x += 1) {
            size_t offset = x * 4;

            state->pix_data[y * state->stride + offset + 0] = LERP(0, 0xFF, INVLERPANG(0, state->width, x)); // B
            state->pix_data[y * state->stride + offset + 1] = LERP(0, 0xFF, INVLERPANG(0, state->height, y)); // G
            state->pix_data[y * state->stride + offset + 2] = 0x00; // R
            state->pix_data[y * state->stride + offset + 3] = 0xFF; // Alpha
        }
    }

    wl_surface_attach(state->wl_surface, state->buffer, 0, 0);
    wl_surface_damage_buffer(state->wl_surface, 0, 0, state->width, state->height);
    wl_surface_commit(state->wl_surface);
}

void resize(State *state) {
    const size_t size = state->height * state->stride;

    int fd = allocate_shm_file(size);

    if (fd == -1) {
        fprintf(stderr, "[ERROR]: allocate_shm_file failed");
        return;
    }
    
    printf("\n[INFO(resize)]: SIZE: %ld", size);
    fflush(stdout);
    state->pix_data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (state->pix_data == MAP_FAILED) {
        fprintf(stderr, "\n[ERROR]: mmap failed %d", fd);
        perror("\n");
        close(fd);
        return;
    }

    struct wl_shm_pool *pool = wl_shm_create_pool(state->wl_shm, fd, size);
    state->buffer = wl_shm_pool_create_buffer(
        pool,
        0,
        state->width,
        state->height,
        state->stride,
        WL_SHM_FORMAT_ARGB8888
    );
    wl_shm_pool_destroy(pool);
    close(fd);
}

static void xdg_surface_configure(
    void *data,
    struct xdg_surface *xdg_surface,
    uint32_t serial
) {
    State *state = data;
    xdg_surface_ack_configure(xdg_surface, serial);

    printf("\n[INFO(xdg_surface_configure)]\n");
    fflush(stdout);
    if (! state->pix_data) {
        resize(state);
    }

    draw(state);
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

static void wl_surface_frame_done(void *data, struct wl_callback *cb, uint32_t time);

static const struct wl_callback_listener wl_surface_frame_listener = {
    .done = wl_surface_frame_done,
};

static void wl_surface_frame_done(
    void *data,
    struct wl_callback *cb, uint32_t time
) {
    wl_callback_destroy(cb);

    State *state = data;
    cb = wl_surface_frame(state->wl_surface);
    wl_callback_add_listener(cb, &wl_surface_frame_listener, state);

    draw(state);
    printf("[DEBUG]: Drawing\n");
    fflush(stdout);
}

void toplevel_configure(
    void *data,
    struct xdg_toplevel *top,
    int32_t nw,
    int32_t nh, struct wl_array *stat
) {
    if (! nw && ! nh) {
        return;
    }
    
    State *state = data;
    if (state->width != nw || state->height != nh) {
        munmap(state->pix_data, state->stride * state->height);
        state->width = nw;
        state->height = nh;
        state->stride = nw * 4;
        printf("\n[INFO(resize)]: width: %d, height: %d", nw, nh);
        fflush(stdout);

        resize(state);
    }
}

void toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel) {
    State *state = data;
    state->is_closed = true;
}

struct xdg_toplevel_listener toplevel_listener = {
    .close = toplevel_close,
    .configure = toplevel_configure,
};

int main() {
    State state = {
        .is_closed = false,
        .width = 1367,
        .height = 768,
        .stride = 1367 * 4,
    };

    state.wl_display = wl_display_connect(NULL);

    if (! state.wl_display) {
        fprintf(stderr, "Failed to connect to wayland display!");
        exit(EXIT_FAILURE);
    }

    state.wl_registry = wl_display_get_registry(state.wl_display);
    wl_registry_add_listener(state.wl_registry, &registry_listener, &state);
    wl_display_roundtrip(state.wl_display);

    state.wl_surface = wl_compositor_create_surface(state.wl_compositor);
    struct wl_callback *cb = wl_surface_frame(state.wl_surface);
    wl_callback_add_listener(cb, &wl_surface_frame_listener, &state);

    state.xdg_surface = xdg_wm_base_get_xdg_surface(state.xdg_wm_base, state.wl_surface);
    xdg_surface_add_listener(state.xdg_surface, &xdg_surface_listener, &state);
    state.xdg_toplevel = xdg_surface_get_toplevel(state.xdg_surface);
    xdg_toplevel_add_listener(state.xdg_toplevel, &toplevel_listener, &state);
    xdg_toplevel_set_title(state.xdg_toplevel, "Wayland Pepega");
    wl_surface_commit(state.wl_surface);

    while (wl_display_dispatch(state.wl_display) != -1) {
        if (state.is_closed) {
            break;
        }
    }

    xdg_toplevel_destroy(state.xdg_toplevel);
    xdg_surface_destroy(state.xdg_surface);
    wl_surface_destroy(state.wl_surface);
    wl_display_disconnect(state.wl_display);
    
    return 0;
}
