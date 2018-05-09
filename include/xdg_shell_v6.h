/*
 * Copyright © 2017 Canonical Ltd.
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

    operator zxdg_surface_v6*() const {return shell_surface;}

    void attach_buffer(int width, int height)
    {
        buffers.push_back(wlcs::ShmBuffer{*client, width, height});
        wl_surface_attach(*surface, buffers.back(), 0, 0);
    }

    void dispatch_until_configure()
    {
        client->dispatch_until(
            [prev_count = configure_events_count, &current_count = configure_events_count]()
            {
                return current_count > prev_count;
            });
    }

    wlcs::Client* const client;
    wlcs::Surface* const surface;
    std::vector<wlcs::ShmBuffer> buffers;
    zxdg_surface_v6* shell_surface;

    int configure_events_count{0};

private:
    void configure(uint32_t serial);

    static void confgure_thunk(void *data, struct zxdg_surface_v6 *, uint32_t serial)
    {
        static_cast<XdgSurfaceV6*>(data)->configure(serial);
    }
};

class XdgToplevelV6
{
public:
    XdgToplevelV6(XdgSurfaceV6& shell_surface_);
    ~XdgToplevelV6();

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

private:
    void configure(int32_t width_, int32_t height_, struct wl_array *states);
    void close();

    static void configure_thunk(void *data, struct zxdg_toplevel_v6 *, int32_t width, int32_t height,
                                struct wl_array *states)
    {
        static_cast<XdgToplevelV6*>(data)->configure(width, height, states);
    }

    static void close_thunk(void *data, struct zxdg_toplevel_v6 *)
    {
        static_cast<XdgToplevelV6*>(data)->close();
    }
};

}

#endif // WLCS_XDG_SHELL_V6_
