#include "display_server_wayland.h"

#include "wayland_shm_file.h"

//#define _POSIX_C_SOURCE 200112L // or higher
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <cassert>
#include <cstring>

// coherent naming of listeners (to preserve our sanity):
// if structure is A_B_C_listener, then instance is A_B_C_listener_info,
// and callbacks are called on_A_B_C_something().

static const struct wl_registry_listener wl_registry_listener_info = {
    .global = DisplayServerWayland::_on_registry_global,
    .global_remove = DisplayServerWayland::_on_registry_global_remove,
};

static const struct xdg_surface_listener xdg_surface_listener_info = {
    .configure = DisplayServerWayland::_on_xdg_surface_configure,
};

static const struct xdg_wm_base_listener xdg_wm_base_listener_info = {
    .ping = DisplayServerWayland::_on_xdg_wm_base_ping,
};

static const struct wl_buffer_listener wl_buffer_listener_info = {
    .release = DisplayServerWayland::_on_buffer_release,
};

static const xdg_toplevel_listener xdg_toplevel_listener_info = {
    .configure = DisplayServerWayland::_on_xdg_toplevel_configure,
    .close = DisplayServerWayland::_on_xdg_toplevel_close,
};

static const struct wl_pointer_listener pointer_listener = {
    .enter = DisplayServerWayland::_on_pointer_enter,
    .leave = DisplayServerWayland::_on_pointer_leave,
    .motion = DisplayServerWayland::_on_pointer_motion,
    .button = DisplayServerWayland::_on_pointer_button,
    .frame = DisplayServerWayland::_on_pointer_frame,
    //.axis = noop, // TODO
};

static const struct wl_seat_listener wl_seat_listener_info = {
    .capabilities = DisplayServerWayland::_on_seat_handle_capabilities,
    .name = DisplayServerWayland::_on_seat_name,
};

Error DisplayServerWayland::_wayland_connect() {
    wayland_display = wl_display_connect(nullptr);
    if (!wayland_display) {
        ERR_PRINT("Wayland display is not available");
        return ERR_UNAVAILABLE;
    }

    // errors in the following part are unlikely but must be checked for anyway
    // as the alternative is segfaulting on null pointer dereference

    // if something like this happens, we just disconnect and don't bother
    // cleaning up all resources, this should not be a problem in such case

    // the registry holds IDs of the most important objects
    wayland_registry = wl_display_get_registry(wayland_display);
    if (!wayland_registry) {
        ERR_PRINT("wayland: wl_display_get_registry() failed, compositor bug?");
        wl_display_disconnect(wayland_display);
        return ERR_UNAVAILABLE;
    }

    wl_registry_add_listener(wayland_registry, &wl_registry_listener_info, nullptr);

    // during this roundtrip, the server should send us IDs of many globals,
    // including the compositor, SHM and XDG windowmanager base, and seat;
    // for each global, the on_registry_global() callback is called automatically
    wl_display_roundtrip(wayland_display);

    // check if we have all the needed globals
    if (!wayland_compositor || !wayland_xdg_wm_base || !wayland_seat) {
        ERR_PRINT("wayland: missing one of compositor/shm/xdg_wm_base/seat interfaces, compositor bug?");
        wl_display_disconnect(wayland_display);
        return ERR_UNAVAILABLE;
    }

    wl_seat_add_listener(wayland_seat, &wl_seat_listener_info, nullptr);
    xdg_wm_base_add_listener(wayland_xdg_wm_base, &xdg_wm_base_listener_info, nullptr);

    wayland_surface = wl_compositor_create_surface(wayland_compositor);
    if (!wayland_surface) {
        ERR_PRINT("wayland: wl_compositor_create_surface() failed, compositor bug?");
        wl_display_disconnect(wayland_display);
        return ERR_UNAVAILABLE;
    }

    wayland_xdg_surface = xdg_wm_base_get_xdg_surface(wayland_xdg_wm_base, wayland_surface);
    if (!wayland_xdg_surface) {
        ERR_PRINT("wayland: xdg_wm_base_get_xdg_surface() failed, compositor bug?");
        wl_display_disconnect(wayland_display);
        return ERR_UNAVAILABLE;
    }

    xdg_surface_add_listener(wayland_xdg_surface, &xdg_surface_listener_info, nullptr);

    wayland_xdg_toplevel = xdg_surface_get_toplevel(wayland_xdg_surface);
    if (!wayland_xdg_toplevel) {
        ERR_PRINT("wayland: xdg_surface_get_toplevel() failed, compositor bug?");
        wl_display_disconnect(wayland_display);
    }

    xdg_toplevel_set_title(wayland_xdg_toplevel, "Godot");
    xdg_toplevel_add_listener(wayland_xdg_toplevel, &xdg_toplevel_listener_info, nullptr);

    // at this point, we have a functional window but Wayland will not show it yet;
    // it will appear after the first frame is drawn

    return OK;
}

void DisplayServerWayland::_wayland_disconnect() {
    if (wayland_display) {
        wl_display_disconnect(wayland_display);
        wayland_display = nullptr;
    }
}

void DisplayServerWayland::_register_global(char const* interface, uint32_t name) {
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        wayland_compositor = (wl_compositor*)(wl_registry_bind(wayland_registry, name, &wl_compositor_interface, COMPOSITOR_API_VERSION));
    }
    else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        wayland_xdg_wm_base = (xdg_wm_base*) wl_registry_bind(wayland_registry, name, &xdg_wm_base_interface, 1);
    }
    else if (strcmp(interface, wl_seat_interface.name) == 0) {
        wayland_seat = (wl_seat*) wl_registry_bind(wayland_registry, name, &wl_seat_interface, SEAT_API_VERSION);
    }
    else {
        // server may inform us about potentially many other interfaces we don't use
        // these can be simply ignored (we don't need to confirm)
    }
}

void DisplayServerWayland::_on_xdg_wm_base_ping(void *data, xdg_wm_base *xdg_wm_base, uint32_t serial) {
    xdg_wm_base_pong(xdg_wm_base, serial);
}

void DisplayServerWayland::_on_registry_global(void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
    ((DisplayServerWayland*)get_singleton())->_register_global(interface, name);
}

void DisplayServerWayland::_on_registry_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
    // TODO
}

void DisplayServerWayland::_on_buffer_release(void* data, wl_buffer* buffer) {
    wl_buffer_destroy(buffer);
}

void DisplayServerWayland::_on_xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial) {
    // TODO
}

void DisplayServerWayland::_on_xdg_toplevel_configure(void* data, struct xdg_toplevel* xdg_toplevel, int32_t width,
    int32_t height, struct wl_array *states) {
    // TODO
}

void DisplayServerWayland::_on_xdg_toplevel_close(void* data, struct xdg_toplevel *xdg_toplevel) {
    // TODO
}

void DisplayServerWayland::_on_seat_handle_capabilities(void *data, struct wl_seat *seat, uint32_t capabilities)
{
    if (capabilities & WL_SEAT_CAPABILITY_POINTER) {
        struct wl_pointer *pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(pointer, &pointer_listener, seat);
    }
}

void DisplayServerWayland::_on_seat_name(void* data, struct wl_seat* seat, char const* name) {
    // TODO
}

void DisplayServerWayland::_on_pointer_button(void* data, struct wl_pointer *pointer,
    uint32_t serial, uint32_t time, uint32_t button, uint32_t state) {
    // TODO
}

void DisplayServerWayland::_on_pointer_enter(void* data, struct wl_pointer* pointer, uint32_t, wl_surface*, wl_fixed_t x, wl_fixed_t y) {
    // TODO
}

void DisplayServerWayland::_on_pointer_leave(void* data, struct wl_pointer* pointer, uint32_t, wl_surface*) {
    // TODO
}

void DisplayServerWayland::_on_pointer_motion(void *data, struct wl_pointer *wl_pointer,
               uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y) {
    // TODO
}

void DisplayServerWayland::_on_pointer_frame(void* data, struct wl_pointer* pointer) {
    // TODO
}

DisplayServerWayland::DisplayServerWayland(
    const String &p_rendering_driver,
    WindowMode p_mode,
    VSyncMode p_vsync_mode,
    uint32_t p_flags,
    const Vector2i *p_position,
    const Vector2i &p_resolution,
    int p_screen,
    Error &r_error
) {
    r_error = _wayland_connect();
}

DisplayServerWayland::~DisplayServerWayland() {
    _wayland_disconnect();
}

bool DisplayServerWayland::has_feature(Feature p_feature) const {
    switch (p_feature) {
        case DisplayServer::FEATURE_MOUSE:
        case DisplayServer::FEATURE_HIDPI:
            return true;

        case DisplayServer::FEATURE_GLOBAL_MENU:
        case DisplayServer::FEATURE_SUBWINDOWS:
        case DisplayServer::FEATURE_TOUCHSCREEN:
        case DisplayServer::FEATURE_MOUSE_WARP:
        case DisplayServer::FEATURE_CLIPBOARD:
        case DisplayServer::FEATURE_VIRTUAL_KEYBOARD:
        case DisplayServer::FEATURE_CURSOR_SHAPE:
        case DisplayServer::FEATURE_CUSTOM_CURSOR_SHAPE:
        case DisplayServer::FEATURE_NATIVE_DIALOG:
        case DisplayServer::FEATURE_IME:
        case DisplayServer::FEATURE_WINDOW_TRANSPARENCY:
        case DisplayServer::FEATURE_ICON:
        case DisplayServer::FEATURE_NATIVE_ICON:
        case DisplayServer::FEATURE_ORIENTATION:
        case DisplayServer::FEATURE_SWAP_BUFFERS:
        case DisplayServer::FEATURE_KEEP_SCREEN_ON:
        case DisplayServer::FEATURE_CLIPBOARD_PRIMARY:
        case DisplayServer::FEATURE_TEXT_TO_SPEECH:
        case DisplayServer::FEATURE_EXTEND_TO_TITLE:
        case DisplayServer::FEATURE_SCREEN_CAPTURE:
            return false;
    }
}

String DisplayServerWayland::get_name() const {
    return "wayland";
}

int64_t DisplayServerWayland::window_get_native_handle(HandleType p_handle_type, WindowID p_window) const {
    if (p_handle_type == HandleType::DISPLAY_HANDLE) {
        return reinterpret_cast<int64_t>(wayland_display);
    }
    else if (p_handle_type == HandleType::WINDOW_HANDLE) {
        if (p_window == MAIN_WINDOW_ID) {
            return reinterpret_cast<int64_t>(wayland_surface);
        }
        return 0;
    }
    return 0;
}

int DisplayServerWayland::get_screen_count() const {
    return 0;   // TODO
}

int DisplayServerWayland::get_primary_screen() const {
    return 0;   // TODO
}

int DisplayServerWayland::get_keyboard_focus_screen() const {
    return 0;   // TODO
}

Point2i DisplayServerWayland::screen_get_position(int p_screen) const {
    return Point2i(0, 0);   // TODO
}

Size2i DisplayServerWayland::screen_get_size(int p_screen) const {
    return Size2i(screen_width, screen_height);
}

void DisplayServerWayland::gl_window_make_current(DisplayServer::WindowID p_window_id) {
#if defined(GLES3_ENABLED)
    if (gl_manager_egl) {
        gl_manager_egl->window_make_current(p_window_id);
    }
#endif
}

bool DisplayServerWayland::window_can_draw(WindowID p_window) const {
    // FIXME: not much sure what this means, copied this from X11
    return window_get_mode(p_window) != WINDOW_MODE_MINIMIZED;
}

bool DisplayServerWayland::can_any_window_draw() const {
    _THREAD_SAFE_METHOD_

    // FIXME: see window_can_draw() - I don't really know what should be done here
    for (const KeyValue<WindowID, WindowData> &E : windows) {
        if (window_get_mode(E.key) != WINDOW_MODE_MINIMIZED) {
            return true;
        }
    }

    return false;
}

void DisplayServerWayland::process_events() {
    wl_display_dispatch(wayland_display);
}

Vector<String> DisplayServerWayland::get_rendering_drivers_func() {
    Vector<String> drivers;

#ifdef VULKAN_ENABLED
    drivers.push_back("vulkan");
#endif
#ifdef GLES3_ENABLED
    drivers.push_back("opengl3");
    drivers.push_back("opengl3_es");
#endif

    return drivers;
}

DisplayServer *DisplayServerWayland::create_func(const String &p_rendering_driver, WindowMode p_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution, int p_screen, Error &r_error) {
    DisplayServer *ds = memnew(DisplayServerWayland(p_rendering_driver, p_mode, p_vsync_mode, p_flags, p_position, p_resolution, p_screen, r_error));
    if (r_error != OK) {
        if (p_rendering_driver == "vulkan") {
            String executable_name = OS::get_singleton()->get_executable_path().get_file();
            OS::get_singleton()->alert(
                    vformat("Your video card drivers seem not to support the required Vulkan version.\n\n"
                            "If possible, consider updating your video card drivers or using the OpenGL 3 driver.\n\n"
                            "You can enable the OpenGL 3 driver by starting the engine from the\n"
                            "command line with the command:\n\n    \"%s\" --rendering-driver opengl3\n\n"
                            "If you recently updated your video card drivers, try rebooting.",
                            executable_name),
                    "Unable to initialize Vulkan video driver");
        } else {
            OS::get_singleton()->alert(
                    "Your video card drivers seem not to support the required OpenGL 3.3 version.\n\n"
                    "If possible, consider updating your video card drivers.\n\n"
                    "If you recently updated your video card drivers, try rebooting.",
                    "Unable to initialize OpenGL video driver");
        }
    }
    return ds;
}

void DisplayServerWayland::register_wayland_driver() {
    register_create_function("wayland", create_func, get_rendering_drivers_func);
}