/*
 * Copyright © 2018 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: William Wold <william.wold@canonical.com>
 */

#ifndef WLCS_XDG_SHELL_V6_
#define WLCS_XDG_SHELL_V6_

#include "in_process_server.h"
#include "generated/xdg-shell-unstable-v6-client.h"

namespace wlcs
{

class XdgSurfaceV6
{
public:
    XdgSurfaceV6(wlcs::Client& client, wlcs::Surface& surface);
    ~XdgSurfaceV6();

    void add_configure_notification(std::function<void(uint32_t)> notification)
    {
        configure_notifiers.push_back(notification);
    }

    operator zxdg_surface_v6*() const {return shell_surface;}

    wlcs::Client* const client;
    wlcs::Surface* const surface;
    zxdg_surface_v6* shell_surface;

    int configure_events_count{0};

    std::vector<std::function<void(uint32_t)>> configure_notifiers;

private:
    void configure(uint32_t serial);

    static void confgure_thunk(void *data, struct zxdg_surface_v6 *, uint32_t serial)
    {
        static_cast<XdgSurfaceV6*>(data)->configure(serial);
        for (auto& notifier : static_cast<XdgSurfaceV6*>(data)->configure_notifiers)
        {
            notifier(serial);
        }
    }
};

class XdgToplevelV6
{
public:
    struct State
    {
        State(int32_t width, int32_t height, struct wl_array *states);

        int width;
        int height;

        bool maximized;
        bool fullscreen;
        bool resizing;
        bool activated;
    };

    XdgToplevelV6(XdgSurfaceV6& shell_surface_);
    ~XdgToplevelV6();

    void add_configure_notification(std::function<void(int32_t, int32_t, struct wl_array *)> notification)
    {
        configure_notifiers.push_back(notification);
    }

    void add_close_notification(std::function<void(int32_t, int32_t, struct wl_array *)> notification)
    {
        configure_notifiers.push_back(notification);
    }

    operator zxdg_toplevel_v6*() const {return toplevel;}

    XdgSurfaceV6* const shell_surface;
    zxdg_toplevel_v6* toplevel;

    bool window_maximized{false};
    bool window_fullscreen{false};
    bool window_resizing{false};
    bool window_activated{false};

    int window_width{-1};
    int window_height{-1};

    bool window_should_close{false};

    std::vector<std::function<void(int32_t, int32_t, struct wl_array *)>> configure_notifiers;
    std::vector<std::function<void()>> close_notifiers;

private:
    void configure(int32_t width_, int32_t height_, struct wl_array *states);
    void close();

    static void configure_thunk(void *data, struct zxdg_toplevel_v6 *, int32_t width, int32_t height,
                                struct wl_array *states)
    {
        static_cast<XdgToplevelV6*>(data)->configure(width, height, states);
        for (auto& notifier : static_cast<XdgToplevelV6*>(data)->configure_notifiers)
        {
            notifier(width, height, states);
        }
    }

    static void close_thunk(void *data, struct zxdg_toplevel_v6 *)
    {
        static_cast<XdgToplevelV6*>(data)->close();
        for (auto& notifier : static_cast<XdgToplevelV6*>(data)->close_notifiers)
        {
            notifier();
        }
    }
};

}

#endif // WLCS_XDG_SHELL_V6_
