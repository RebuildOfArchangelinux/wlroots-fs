/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_TYPES_WLR_FRACTIONAL_SCALE_WRATH_V1_H
#define WLR_TYPES_WLR_FRACTIONAL_SCALE_WRATH_V1_H

#include <wayland-server-core.h>
#include <wlr/types/wlr_output.h>

struct wlr_surface;

struct wlr_fractional_scale_wrath_manager_v1 {
	struct wl_global *global;

	struct {
		struct wl_signal destroy;
	} events;

	struct wl_listener display_destroy;
};

void wlr_fractional_scale_wrath_v1_notify_scale(
		struct wlr_output *output);

struct wlr_fractional_scale_wrath_manager_v1 *wlr_fractional_scale_wrath_manager_v1_create(
		struct wl_display *display);

#endif

