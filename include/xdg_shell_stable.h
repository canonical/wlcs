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

#ifndef WLCS_XDG_SHELL_STABLE_H
#define WLCS_XDG_SHELL_STABLE_H

#include "in_process_server.h"
#include "generated/xdg-shell-client.h"
#include "wl_interface_descriptor.h"
#include "wl_handle.h"

#include <gmock/gmock.h>

namespace wlcs
{
WLCS_CREATE_INTERFACE_DESCRIPTOR(xdg_wm_base)

class XdgSurfaceStable
{
public:
    XdgSurfaceStable(wlcs::Client& client, wlcs::Surface& surface);
    XdgSurfaceStable(XdgSurfaceStable const&) = delete;
    XdgSurfaceStable& operator=(XdgSurfaceStable const&) = delete;
    ~XdgSurfaceStable();

    MOCK_METHOD(void, configure, (uint32_t serial));

    operator xdg_surface*() const {return shell_surface;}

private:
    xdg_surface* shell_surface;
};

class XdgToplevelStable
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

    XdgToplevelStable(XdgSurfaceStable& shell_surface_);
    XdgToplevelStable(XdgToplevelStable const&) = delete;
    XdgToplevelStable& operator=(XdgToplevelStable const&) = delete;
    ~XdgToplevelStable();

    MOCK_METHOD(void, configure, (int32_t width, int32_t height, wl_array* states));
    MOCK_METHOD(void, close, ());

    operator xdg_toplevel*() const {return toplevel;}

    XdgSurfaceStable* const shell_surface;
    xdg_toplevel* toplevel;
};

class XdgPositionerStable
{
public:
    XdgPositionerStable(wlcs::Client& client);
    ~XdgPositionerStable();
    operator xdg_positioner*() const {return positioner;}

private:
    xdg_positioner* const positioner;
};

class XdgPopupStable
{
public:
    XdgPopupStable(
        XdgSurfaceStable& shell_surface_,
        std::optional<XdgSurfaceStable*> parent,
        XdgPositionerStable& positioner);
    XdgPopupStable(XdgPopupStable const&) = delete;
    XdgPopupStable& operator=(XdgPopupStable const&) = delete;
    ~XdgPopupStable();

    MOCK_METHOD(void, configure, (int32_t x, int32_t y, int32_t width, int32_t height));
    MOCK_METHOD(void, done, ());

    operator xdg_popup*() const {return popup;}

    XdgSurfaceStable* const shell_surface;
    xdg_popup* const popup;

    std::vector<std::function<void(int32_t, int32_t, int32_t, int32_t)>> configure_notifiers;
    std::vector<std::function<void()>> popup_done_notifiers;
};

}

#endif // WLCS_XDG_SHELL_STABLE_H
