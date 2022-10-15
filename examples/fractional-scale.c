#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include <linux/input-event-codes.h>

#include "xdg-shell-client-protocol.h"
#include "viewporter-client-protocol.h"
#include "wp-fractional-scale-v1-client-protocol.h"

static void randname(char *buf) {
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	long r = ts.tv_nsec;
	for (int i = 0; i < 6; ++i) {
		buf[i] = 'A'+(r&15)+(r&16)*2;
		r >>= 5;
	}
}

static int anonymous_shm_open(void) {
	char name[] = "/hello-wayland-XXXXXX";
	int retries = 100;

	do {
		randname(name + strlen(name) - 6);

		--retries;
		// shm_open guarantees that O_CLOEXEC is set
		int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
		if (fd >= 0) {
			shm_unlink(name);
			return fd;
		}
	} while (retries > 0 && errno == EEXIST);

	return -1;
}

static int create_shm_file(off_t size) {
	int fd = anonymous_shm_open();
	if (fd < 0) {
		return fd;
	}

	if (ftruncate(fd, size) < 0) {
		close(fd);
		return -1;
	}

	return fd;
}

static const int w = 256;
static const int h = 256;

static bool running = true;

static struct wl_shm *shm = NULL;
static struct wl_compositor *compositor = NULL;
static struct xdg_wm_base *xdg_wm_base = NULL;
static struct wp_viewporter *viewporter = NULL;
static struct wp_fractional_scale_manager_v1 *surface_scale_manager = NULL;

static void *shm_data = NULL;
static struct wl_surface *surface = NULL;
static struct xdg_toplevel *xdg_toplevel = NULL;
static struct wp_viewport *viewport = NULL;
static struct wl_buffer *buffer = NULL;
static struct wl_display *display = NULL;

static void xdg_surface_handle_configure(void *data,
		struct xdg_surface *xdg_surface, uint32_t serial) {
	xdg_surface_ack_configure(xdg_surface, serial);
	wl_surface_commit(surface);
}

static const struct xdg_surface_listener xdg_surface_listener = {
	.configure = xdg_surface_handle_configure,
};

static void xdg_toplevel_handle_configure(void *data, struct xdg_toplevel
		*toplevel, int32_t width, int32_t size, struct wl_array *states) {
}

static void xdg_toplevel_handle_close(void *data,
		struct xdg_toplevel *xdg_toplevel) {
	running = false;
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
	.configure = xdg_toplevel_handle_configure,
	.close = xdg_toplevel_handle_close,
};

static void handle_global(void *data, struct wl_registry *registry,
		uint32_t name, const char *interface, uint32_t version) {
	if (strcmp(interface, wl_shm_interface.name) == 0) {
		shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
	} else if (strcmp(interface, wl_compositor_interface.name) == 0) {
		compositor = wl_registry_bind(registry, name,
			&wl_compositor_interface, 4);
	} else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
		xdg_wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 2);
	} else if (strcmp(interface, wp_fractional_scale_manager_v1_interface.name)
			== 0) {
		surface_scale_manager = wl_registry_bind(registry, name,
				&wp_fractional_scale_manager_v1_interface, 1);
	} else if (strcmp(interface, wp_viewporter_interface.name) == 0) {
		viewporter = wl_registry_bind(registry, name, &wp_viewporter_interface,
				1);
	}
}

static void handle_global_remove(void *data, struct wl_registry *registry,
		uint32_t name) {
	// Who cares
}

static const struct wl_registry_listener registry_listener = {
	.global = handle_global,
	.global_remove = handle_global_remove,
};

static bool render_cnt = false;
static struct wl_buffer *create_buffer(double scale) {
	int width = round(((double)w * scale));
	int height = round(((double)h * scale));
	int stride = width * 4;
	int size = stride * height;

	fprintf(stderr, "%lf scale, %d width, %d height, %d size\n", scale, width,
			height, size);
	int fd = create_shm_file(size);
	if (fd < 0) {
		fprintf(stderr, "creating a buffer file for %d B failed: %m\n", size);
		return NULL;
	}

	shm_data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (shm_data == MAP_FAILED) {
		fprintf(stderr, "mmap failed: %m\n");
		close(fd);
		return NULL;
	}

	struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
	struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, width, height,
		stride, WL_SHM_FORMAT_ARGB8888);
	wl_shm_pool_destroy(pool);

	uint32_t fill_color = render_cnt ? 0xFFFF0000 : 0xFFFFFFFF;
	render_cnt = !render_cnt;

	// Draw sections of bars of various types for debugging
	//
	// 1. One that always has 1 physical px features, to make blur/misalignment
	// clear. This one violates scale
	// 2. One that has 2 logical px features.
	// 3. One that has 4 logical px features.
	// 4. One that has 8 logical px features.
	bool fill = true;
	int row = 0;

	// Calibration grid, 1px feature size
	for (; round((h/4)*scale) >= row; row++) {
		fill = !fill;
		int col = 0;
		for (; col < width/2; col++) {
			((uint32_t*)shm_data)[row * width + col] = fill ? 0xFFFFFFFF : 0xFF000000;
		}
		bool colfill = false;
		for (; col < width; col++) {
			colfill = !colfill;
			((uint32_t*)shm_data)[row * width + col] = colfill ? 0xFFFFFFFF : 0xFF000000;
		}
	}

	fill = true;
	bool growing = false;
	double last_remainder = -1000;
	for (; round((h/4)*2*scale) >= row; row++) {
		double remainder = fmod(row, scale*8);
		if (remainder < last_remainder) {
			growing = false;
		} else if (remainder > last_remainder && !growing) {
			growing = true;
			fill = !fill;
		}
		last_remainder = remainder;
		for (int col = 0; col < width; col++) {
			((uint32_t*)shm_data)[row * width + col] = fill ? 0xFFFFFFFF : 0xFF000000;
		}
	}
	fill = true;
	growing = false;
	last_remainder = -1000;
	for (; round((h/4)*3*scale) >= row; row++) {
		double remainder = fmod(row, scale*16);
		if (remainder < last_remainder) {
			growing = false;
		} else if (remainder > last_remainder && !growing) {
			growing = true;
			fill = !fill;
		}
		last_remainder = remainder;
		for (int col = 0; col < width; col++) {
			((uint32_t*)shm_data)[row * width + col] = fill ? 0xFFFFFFFF : 0xFF000000;
		}
	}
	fill = true;
	growing = false;
	last_remainder = -1000;
	for (; row < height; row++) {
		double remainder = fmod(row, scale*32);
		if (remainder < last_remainder) {
			growing = false;
		} else if (remainder > last_remainder && !growing) {
			growing = true;
			fill = !fill;
		}
		last_remainder = remainder;
		for (int col = 0; col < width; col++) {
			((uint32_t*)shm_data)[row * width + col] = fill ? fill_color : 0xFF000000;
		}
	}
	return buffer;
}

static void surface_scale_handle_preferred_scale(void *data, struct
		wp_fractional_scale_v1 *info, wl_fixed_t scale) {
	double dscale = wl_fixed_to_double(scale);
	fprintf(stderr, "Preferred scale: %lf, %d\n", dscale,
			(int32_t)scale);
	// leaking them buffers
	buffer = create_buffer(dscale);
	wl_surface_attach(surface, buffer, 0, 0);
	wl_surface_damage_buffer(surface, 0, 0, 0xFFFF, 0xFFFF);
	wp_viewport_set_destination(viewport, w, h);
	wl_surface_commit(surface);
}

static const struct wp_fractional_scale_v1_listener surface_scale_listener = {
	.preferred_scale = surface_scale_handle_preferred_scale,
};

int main(int argc, char *argv[]) {
	display = wl_display_connect(NULL);
	if (display == NULL) {
		fprintf(stderr, "failed to create display\n");
		return EXIT_FAILURE;
	}

	struct wl_registry *registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	wl_display_roundtrip(display);

	if (shm == NULL || compositor == NULL || xdg_wm_base == NULL ||
			surface_scale_manager == NULL || viewporter == NULL) {
		fprintf(stderr, "no wl_shm, wl_compositor or xdg_wm_base support\n");
		return EXIT_FAILURE;
	}

	buffer = create_buffer(1);
	if (buffer == NULL) {
		return EXIT_FAILURE;
	}

	surface = wl_compositor_create_surface(compositor);

	struct xdg_surface *xdg_surface =
		xdg_wm_base_get_xdg_surface(xdg_wm_base, surface);
	xdg_toplevel = xdg_surface_get_toplevel(xdg_surface);

	xdg_surface_add_listener(xdg_surface, &xdg_surface_listener, NULL);
	xdg_toplevel_add_listener(xdg_toplevel, &xdg_toplevel_listener, NULL);

	wl_surface_commit(surface);
	wl_display_roundtrip(display);
	struct wp_fractional_scale_v1 *info =
		wp_fractional_scale_manager_v1_get_fractional_scale(surface_scale_manager,
				surface);
	wp_fractional_scale_v1_add_listener(info, &surface_scale_listener, NULL);

	viewport = wp_viewporter_get_viewport(viewporter, surface);
	wp_viewport_set_destination(viewport, w, h);
	wl_surface_attach(surface, buffer, 0, 0);
	wl_surface_commit(surface);

	while (wl_display_dispatch(display) != -1 && running) {
		// This space intentionally left blank
	}

	xdg_toplevel_destroy(xdg_toplevel);
	xdg_surface_destroy(xdg_surface);
	wl_surface_destroy(surface);
	wl_buffer_destroy(buffer);

	return EXIT_SUCCESS;
}

