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

#include "xdg_shell_v6.h"

// XdgSurfaceV6

wlcs::XdgSurfaceV6::XdgSurfaceV6(wlcs::Client& client, wlcs::Surface& surface)
    : client{&client},
      surface{&surface}
{
    if (!client.xdg_shell_v6())
        throw std::runtime_error("XDG shell unstable V6 not supported by compositor");
    shell_surface = zxdg_shell_v6_get_xdg_surface(client.xdg_shell_v6(), surface);
    static struct zxdg_surface_v6_listener const listener = {configure_thunk};
    zxdg_surface_v6_add_listener(shell_surface, &listener, this);
}

wlcs::XdgSurfaceV6::~XdgSurfaceV6()
{
    zxdg_surface_v6_destroy(shell_surface);
}

// XdgToplevelV6

wlcs::XdgToplevelV6::State::State(int32_t width, int32_t height, struct wl_array *states)
    : width{width},
      height{height},
      maximized{false},
      fullscreen{false},
      resizing{false},
      activated{false}
{
    zxdg_toplevel_v6_state* item;
    for (item = static_cast<zxdg_toplevel_v6_state*>(states->data);
         (char*)item < static_cast<char*>(states->data) + states->size;
         item++)
    {
        switch (*item)
        {
            case ZXDG_TOPLEVEL_V6_STATE_MAXIMIZED:
                maximized = true;
                break;
            case ZXDG_TOPLEVEL_V6_STATE_FULLSCREEN:
                fullscreen = true;
                break;
            case ZXDG_TOPLEVEL_V6_STATE_RESIZING:
                resizing = true;
                break;
            case ZXDG_TOPLEVEL_V6_STATE_ACTIVATED:
                activated = true;
                break;
        }
    }
}

wlcs::XdgToplevelV6::XdgToplevelV6(XdgSurfaceV6& shell_surface_)
    : shell_surface{&shell_surface_}
{
    toplevel = zxdg_surface_v6_get_toplevel(*shell_surface);
    static struct zxdg_toplevel_v6_listener const listener = {configure_thunk, close_thunk};
    zxdg_toplevel_v6_add_listener(toplevel, &listener, this);
}

wlcs::XdgToplevelV6::~XdgToplevelV6()
{
    zxdg_toplevel_v6_destroy(toplevel);
}
