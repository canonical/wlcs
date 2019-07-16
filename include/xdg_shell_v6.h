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
    XdgSurfaceV6(XdgSurfaceV6 const&) = delete;
    XdgSurfaceV6& operator=(XdgSurfaceV6 const&) = delete;
    ~XdgSurfaceV6();

    void add_configure_notification(std::function<void(uint32_t)> notification)
    {
        configure_notifiers.push_back(notification);
    }

    operator zxdg_surface_v6*() const {return shell_surface;}

    std::vector<std::function<void(uint32_t)>> configure_notifiers;

private:
    static void configure_thunk(void *data, struct zxdg_surface_v6 *, uint32_t serial)
    {
        for (auto& notifier : static_cast<XdgSurfaceV6*>(data)->configure_notifiers)
        {
            notifier(serial);
        }
    }

    zxdg_surface_v6* shell_surface;
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
    XdgToplevelV6(XdgToplevelV6 const&) = delete;
    XdgToplevelV6& operator=(XdgToplevelV6 const&) = delete;
    ~XdgToplevelV6();

    void add_configure_notification(std::function<void(int32_t, int32_t, struct wl_array *)> notification)
    {
        configure_notifiers.push_back(notification);
    }

    void add_close_notification(std::function<void()> notification)
    {
        close_notifiers.push_back(notification);
    }

    operator zxdg_toplevel_v6*() const {return toplevel;}

    XdgSurfaceV6* const shell_surface;
    zxdg_toplevel_v6* toplevel;

    std::vector<std::function<void(int32_t, int32_t, struct wl_array *)>> configure_notifiers;
    std::vector<std::function<void()>> close_notifiers;

private:
    static void configure_thunk(void *data, struct zxdg_toplevel_v6 *, int32_t width, int32_t height,
                                struct wl_array *states)
    {
        for (auto& notifier : static_cast<XdgToplevelV6*>(data)->configure_notifiers)
        {
            notifier(width, height, states);
        }
    }

    static void close_thunk(void *data, struct zxdg_toplevel_v6 *)
    {
        for (auto& notifier : static_cast<XdgToplevelV6*>(data)->close_notifiers)
        {
            notifier();
        }
    }
};

class XdgPositionerV6
{
public:
    XdgPositionerV6(wlcs::Client& client);
    ~XdgPositionerV6();
    operator zxdg_positioner_v6*() const {return positioner;}

private:
    zxdg_positioner_v6* const positioner;
};

class XdgPopupV6
{
public:
    XdgPopupV6(XdgSurfaceV6& shell_surface_, XdgSurfaceV6& parent, XdgPositionerV6& positioner);
    XdgPopupV6(XdgPopupV6 const&) = delete;
    XdgPopupV6& operator=(XdgPopupV6 const&) = delete;
    ~XdgPopupV6();

    void add_configure_notification(std::function<void(int32_t, int32_t, int32_t, int32_t)> notification)
    {
        configure_notifiers.push_back(notification);
    }

    void add_close_notification(std::function<void()> notification)
    {
        popup_done_notifiers.push_back(notification);
    }

    operator zxdg_popup_v6*() const {return popup;}

    XdgSurfaceV6* const shell_surface;
    zxdg_popup_v6* const popup;

    std::vector<std::function<void(int32_t, int32_t, int32_t, int32_t)>> configure_notifiers;
    std::vector<std::function<void()>> popup_done_notifiers;

private:
    static void configure_thunk(void *data, struct zxdg_popup_v6 *, int32_t x, int32_t y,
                                int32_t width, int32_t height)
    {
        for (auto& notifier : static_cast<XdgPopupV6*>(data)->configure_notifiers)
        {
            notifier(x, y, width, height);
        }
    }

    static void popup_done_thunk(void *data, struct zxdg_popup_v6 *)
    {
        for (auto& notifier : static_cast<XdgPopupV6*>(data)->popup_done_notifiers)
        {
            notifier();
        }
    }
};

}

#endif // WLCS_XDG_SHELL_V6_
