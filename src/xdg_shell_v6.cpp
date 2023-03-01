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

using namespace testing;

// XdgSurfaceV6

wlcs::XdgSurfaceV6::XdgSurfaceV6(wlcs::Client& client, wlcs::Surface& surface)
{
    EXPECT_CALL(*this, configure).Times(AnyNumber());
    if (!client.xdg_shell_v6())
        throw std::runtime_error("XDG shell unstable V6 not supported by compositor");
    shell_surface = zxdg_shell_v6_get_xdg_surface(client.xdg_shell_v6(), surface);
    static struct zxdg_surface_v6_listener const listener = {
        [](void* data, auto, auto... args) { static_cast<XdgSurfaceV6*>(data)->configure(args...); }};
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
    if (!states)
        return;
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
    EXPECT_CALL(*this, configure).Times(AnyNumber());
    EXPECT_CALL(*this, close).Times(AnyNumber());
    toplevel = zxdg_surface_v6_get_toplevel(*shell_surface);
    static struct zxdg_toplevel_v6_listener const listener = {
        [](void* data, auto, auto... args) { static_cast<XdgToplevelV6*>(data)->configure(args...); },
        [](void* data, auto, auto... args) { static_cast<XdgToplevelV6*>(data)->close(args...); }};
    zxdg_toplevel_v6_add_listener(toplevel, &listener, this);
}

wlcs::XdgToplevelV6::~XdgToplevelV6()
{
    zxdg_toplevel_v6_destroy(toplevel);
}

wlcs::XdgPositionerV6::XdgPositionerV6(wlcs::Client& client)
    : positioner{zxdg_shell_v6_create_positioner(client.xdg_shell_v6())}
{
}

wlcs::XdgPositionerV6::~XdgPositionerV6()
{
    zxdg_positioner_v6_destroy(positioner);
}

wlcs::XdgPopupV6::XdgPopupV6(XdgSurfaceV6& shell_surface_, XdgSurfaceV6& parent, XdgPositionerV6& positioner)
    : shell_surface{&shell_surface_},
      popup{zxdg_surface_v6_get_popup(*shell_surface, parent, positioner)}
{
    EXPECT_CALL(*this, configure).Times(AnyNumber());
    EXPECT_CALL(*this, done).Times(AnyNumber());
    static struct zxdg_popup_v6_listener const listener = {
        [](void* data, auto, auto... args) { static_cast<XdgPopupV6*>(data)->configure(args...); },
        [](void* data, auto, auto... args) { static_cast<XdgPopupV6*>(data)->done(args...); }};
    zxdg_popup_v6_add_listener(popup, &listener, this);
}

wlcs::XdgPopupV6::~XdgPopupV6()
{
    zxdg_popup_v6_destroy(popup);
}
