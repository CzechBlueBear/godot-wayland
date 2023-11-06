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
#include "thirdparty/wayland/xdg-shell-client.h"
#include "thirdparty/wayland/zxdg-decoration-client.h"
#include "gl_manager_wayland_egl.h"

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

    EGLDisplay* egl_display = nullptr;

    // versions of interfaces we want from the server
    const uint32_t COMPOSITOR_API_VERSION = 4;
    const uint32_t SHM_API_VERSION = 1;
    const uint32_t SEAT_API_VERSION = 7;

    int screen_width, screen_height = 0;

    struct WindowData {
        // TODO
    };
    HashMap<WindowID, WindowData> windows;

    Error _wayland_connect();
    void _wayland_disconnect();
    void _register_global(char const* interface, uint32_t name);

#if defined(GLES3_ENABLED)
    GLManagerEGL_Wayland *gl_manager_egl = nullptr;
#endif

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

    virtual int get_screen_count() const override;
    virtual int get_primary_screen() const override;
    virtual int get_keyboard_focus_screen() const override;
    virtual Point2i screen_get_position(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;
    virtual Size2i screen_get_size(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;
    virtual Rect2i screen_get_usable_rect(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;
    virtual int screen_get_dpi(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;
    virtual float screen_get_refresh_rate(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;
    virtual Color screen_get_pixel(const Point2i &p_position) const override;
    virtual Ref<Image> screen_get_image(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;

    virtual Vector<DisplayServer::WindowID> get_window_list() const override;

    virtual WindowID create_sub_window(WindowMode p_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Rect2i &p_rect = Rect2i()) override;
    virtual void show_window(WindowID p_id) override;
    virtual void delete_sub_window(WindowID p_id) override;

    virtual WindowID window_get_active_popup() const override;
    virtual void window_set_popup_safe_rect(WindowID p_window, const Rect2i &p_rect) override;
    virtual Rect2i window_get_popup_safe_rect(WindowID p_window) const override;

    virtual WindowID get_window_at_screen_position(const Point2i &p_position) const override;

    virtual int64_t window_get_native_handle(HandleType p_handle_type, WindowID p_window = MAIN_WINDOW_ID) const override;

    virtual void window_attach_instance_id(ObjectID p_instance, WindowID p_window = MAIN_WINDOW_ID) override;
    virtual ObjectID window_get_attached_instance_id(WindowID p_window = MAIN_WINDOW_ID) const override;

    virtual void window_set_title(const String &p_title, WindowID p_window = MAIN_WINDOW_ID) override;
    virtual void window_set_mouse_passthrough(const Vector<Vector2> &p_region, WindowID p_window = MAIN_WINDOW_ID) override;

    virtual void window_set_rect_changed_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) override;
    virtual void window_set_window_event_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) override;
    virtual void window_set_input_event_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) override;
    virtual void window_set_input_text_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) override;
    virtual void window_set_drop_files_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) override;

    virtual int window_get_current_screen(WindowID p_window = MAIN_WINDOW_ID) const override;
    virtual void window_set_current_screen(int p_screen, WindowID p_window = MAIN_WINDOW_ID) override;

    virtual Point2i window_get_position(WindowID p_window = MAIN_WINDOW_ID) const override;
    virtual Point2i window_get_position_with_decorations(WindowID p_window = MAIN_WINDOW_ID) const override;
    virtual void window_set_position(const Point2i &p_position, WindowID p_window = MAIN_WINDOW_ID) override;

    virtual void window_set_max_size(const Size2i p_size, WindowID p_window = MAIN_WINDOW_ID) override;
    virtual Size2i window_get_max_size(WindowID p_window = MAIN_WINDOW_ID) const override;
    virtual void gl_window_make_current(DisplayServer::WindowID p_window_id) override;

    virtual void window_set_transient(WindowID p_window, WindowID p_parent) override;

    virtual void window_set_min_size(const Size2i p_size, WindowID p_window = MAIN_WINDOW_ID) override;
    virtual Size2i window_get_min_size(WindowID p_window = MAIN_WINDOW_ID) const override;

    virtual void window_set_size(const Size2i p_size, WindowID p_window = MAIN_WINDOW_ID) override;
    virtual Size2i window_get_size(WindowID p_window = MAIN_WINDOW_ID) const override;
    virtual Size2i window_get_size_with_decorations(WindowID p_window = MAIN_WINDOW_ID) const override;

    virtual void window_set_mode(WindowMode p_mode, WindowID p_window = MAIN_WINDOW_ID) override;
    virtual WindowMode window_get_mode(WindowID p_window = MAIN_WINDOW_ID) const override;

    virtual bool window_is_maximize_allowed(WindowID p_window = MAIN_WINDOW_ID) const override;

    virtual void window_set_flag(WindowFlags p_flag, bool p_enabled, WindowID p_window = MAIN_WINDOW_ID) override;
    virtual bool window_get_flag(WindowFlags p_flag, WindowID p_window = MAIN_WINDOW_ID) const override;

    virtual void window_request_attention(WindowID p_window = MAIN_WINDOW_ID) override;

    virtual void window_move_to_foreground(WindowID p_window = MAIN_WINDOW_ID) override;
    virtual bool window_is_focused(WindowID p_window = MAIN_WINDOW_ID) const override;

    virtual bool window_can_draw(WindowID p_window = MAIN_WINDOW_ID) const override;

    virtual bool can_any_window_draw() const override;

    virtual void process_events() override;

    static DisplayServer *create_func(const String &p_rendering_driver, WindowMode p_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution, int p_screen, Error &r_error);
    static Vector<String> get_rendering_drivers_func();
    static void register_wayland_driver();

    DisplayServerWayland(const String &p_rendering_driver, WindowMode p_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution, int p_screen, Error &r_error);
    ~DisplayServerWayland();
};

#endif
