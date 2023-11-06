/**************************************************************************/
/*  display_server_wayland.h                                              */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#ifndef DISPLAY_SERVER_WAYLAND_H
#define DISPLAY_SERVER_WAYLAND_H

#include <wayland-client.h>
#include "thirdparty/glad/glad/egl.h"
#include "thirdparty/wayland/xdg-shell-client-protocol.h"
#include "thirdparty/wayland/zxdg-decoration-client-protocol.h"

#include "core/os/thread_safe.h"
#include "servers/display_server.h"

class DisplayServerWayland : public DisplayServer {
    _THREAD_SAFE_CLASS_

    // Wayland globals
    wl_display* wayland_display = nullptr;
    wl_registry* wayland_registry = nullptr;
    wl_compositor* wayland_compositor = nullptr;
    xdg_wm_base* wayland_xdg_wm_base = nullptr;
    wl_seat* wayland_seat = nullptr;

    // Wayland objects
    wl_surface* wayland_surface = nullptr;
    xdg_surface* wayland_xdg_surface = nullptr;
    xdg_toplevel* wayland_xdg_toplevel = nullptr;
    wl_shm_pool *wayland_shm_pool = nullptr;

    EGLDisplay* egl_display = nullptr;

    // versions of interfaces we want from the server
    const uint32_t COMPOSITOR_API_VERSION = 4;
    const uint32_t SHM_API_VERSION = 1;
    const uint32_t SEAT_API_VERSION = 7;

    Error _wayland_connect();
    void _wayland_disconnect();
    void _register_global(char const* interface, uint32_t name);

public:

    // callbacks/listeners for various Wayland events
    static void _on_xdg_wm_base_ping(void *data, xdg_wm_base *xdg_wm_base, uint32_t serial);
    static void _on_registry_global(void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version);
    static void _on_registry_global_remove(void *data, struct wl_registry *registry, uint32_t name);
    static void _on_buffer_release(void* data, wl_buffer* buffer);
    static void _on_xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial);
    static void _on_xdg_toplevel_configure(void* data, struct xdg_toplevel* xdg_toplevel, int32_t width,
                    int32_t height, struct wl_array *states);
    static void _on_xdg_toplevel_close(void* data, struct xdg_toplevel *xdg_toplevel);
    static void _on_seat_handle_capabilities(void *data, struct wl_seat *seat, uint32_t capabilities);
    static void _on_seat_name(void* data, struct wl_seat* seat, char const* name);
    static void _on_pointer_button(void* data, struct wl_pointer *pointer,
                    uint32_t serial, uint32_t time, uint32_t button, uint32_t state);
    static void _on_pointer_enter(void* data, struct wl_pointer* pointer,
                    uint32_t serial, wl_surface*, wl_fixed_t x, wl_fixed_t y);
    static void _on_pointer_leave(void* data, struct wl_pointer* pointer,
                    uint32_t serial, wl_surface*);
    static void _on_pointer_motion(void *data, struct wl_pointer *wl_pointer,
               uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y);
    static void _on_pointer_frame(void* data, struct wl_pointer* wl_pointer);

public:

    virtual bool has_feature(Feature p_feature) const override;
    virtual String get_name() const override;

    virtual int64_t window_get_native_handle(HandleType p_handle_type, WindowID p_window = MAIN_WINDOW_ID) const override;

    static DisplayServer *create_func(const String &p_rendering_driver, WindowMode p_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution, int p_screen, Error &r_error);
    static Vector<String> get_rendering_drivers_func();
    static void register_wayland_driver();

    DisplayServerWayland(const String &p_rendering_driver, WindowMode p_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution, int p_screen, Error &r_error);
    ~DisplayServerWayland();
};

#endif
