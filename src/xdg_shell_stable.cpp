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

#include "xdg_shell_stable.h"

using namespace testing;

// XdgSurfaceStable

wlcs::XdgSurfaceStable::XdgSurfaceStable(wlcs::Client& client, wlcs::Surface& surface)
{
    EXPECT_CALL(*this, configure).Times(AnyNumber());
    if (!client.xdg_shell_stable())
        throw std::runtime_error("XDG shell stable not supported by compositor");
    shell_surface = xdg_wm_base_get_xdg_surface(client.xdg_shell_stable(), surface);
    static struct xdg_surface_listener const listener = {
        [](void* data, auto, auto... args) { static_cast<XdgSurfaceStable*>(data)->configure(args...); }};
    xdg_surface_add_listener(shell_surface, &listener, this);
}

wlcs::XdgSurfaceStable::~XdgSurfaceStable()
{
    xdg_surface_destroy(shell_surface);
}

// XdgToplevelStable

wlcs::XdgToplevelStable::State::State(int32_t width, int32_t height, struct wl_array *states)
    : width{width},
      height{height},
      maximized{false},
      fullscreen{false},
      resizing{false},
      activated{false}
{
    if (!states)
        return;
    xdg_toplevel_state* item;
    for (item = static_cast<xdg_toplevel_state*>(states->data);
         (char*)item < static_cast<char*>(states->data) + states->size;
         item++)
    {
        switch (*item)
        {
            case XDG_TOPLEVEL_STATE_MAXIMIZED:
                maximized = true;
                break;
            case XDG_TOPLEVEL_STATE_FULLSCREEN:
                fullscreen = true;
                break;
            case XDG_TOPLEVEL_STATE_RESIZING:
                resizing = true;
                break;
            case XDG_TOPLEVEL_STATE_ACTIVATED:
                activated = true;
            case XDG_TOPLEVEL_STATE_TILED_LEFT:
            case XDG_TOPLEVEL_STATE_TILED_RIGHT:
            case XDG_TOPLEVEL_STATE_TILED_BOTTOM:
            case XDG_TOPLEVEL_STATE_TILED_TOP:
                break;
        }
    }
}

wlcs::XdgToplevelStable::XdgToplevelStable(XdgSurfaceStable& shell_surface_)
    : shell_surface{&shell_surface_}
{
    EXPECT_CALL(*this, configure).Times(AnyNumber());
    EXPECT_CALL(*this, close).Times(AnyNumber());
    EXPECT_CALL(*this, configure_bounds).Times(AnyNumber());
    EXPECT_CALL(*this, wm_capabilities).Times(AnyNumber());
    toplevel = xdg_surface_get_toplevel(*shell_surface);
    static struct xdg_toplevel_listener const listener = {
        [](void* data, auto, auto... args) { static_cast<XdgToplevelStable*>(data)->configure(args...); },
        [](void* data, auto, auto... args) { static_cast<XdgToplevelStable*>(data)->close(args...); },
        [](void* data, auto, auto... args) { static_cast<XdgToplevelStable*>(data)->configure_bounds(args...); },
        [](void* data, auto, auto... args) { static_cast<XdgToplevelStable*>(data)->wm_capabilities(args...); }};
    xdg_toplevel_add_listener(toplevel, &listener, this);
}

wlcs::XdgToplevelStable::~XdgToplevelStable()
{
    xdg_toplevel_destroy(toplevel);
}

wlcs::XdgPositionerStable::XdgPositionerStable(wlcs::Client& client)
    : positioner{xdg_wm_base_create_positioner(client.xdg_shell_stable())}
{
}

wlcs::XdgPositionerStable::~XdgPositionerStable()
{
    xdg_positioner_destroy(positioner);
}

auto wlcs::XdgPositionerStable::setup_default(std::pair<int, int> size) -> XdgPositionerStable&
{
    xdg_positioner_set_size(positioner, size.first, size.second);
    xdg_positioner_set_anchor_rect(positioner, 0, 0, 1, 1);
    xdg_positioner_set_anchor(positioner, ZXDG_POSITIONER_V6_ANCHOR_TOP | ZXDG_POSITIONER_V6_ANCHOR_LEFT);
    xdg_positioner_set_gravity(positioner, ZXDG_POSITIONER_V6_ANCHOR_BOTTOM | ZXDG_POSITIONER_V6_ANCHOR_RIGHT);
    return *this;
}

wlcs::XdgPopupStable::XdgPopupStable(
    XdgSurfaceStable& shell_surface_,
    std::optional<XdgSurfaceStable*> parent,
    XdgPositionerStable& positioner)
    : shell_surface{&shell_surface_},
      popup{xdg_surface_get_popup(
          *shell_surface,
          parent ? *parent.value() : (xdg_surface*)nullptr,
          positioner)}
{
    EXPECT_CALL(*this, configure).Times(AnyNumber());
    EXPECT_CALL(*this, done).Times(AnyNumber());
    EXPECT_CALL(*this, repositioned).Times(AnyNumber());
    static struct xdg_popup_listener const listener = {
        [](void* data, auto, auto... args) { static_cast<XdgPopupStable*>(data)->configure(args...); },
        [](void* data, auto, auto... args) { static_cast<XdgPopupStable*>(data)->done(args...); },
        [](void* data, auto, auto... args) { static_cast<XdgPopupStable*>(data)->repositioned(args...); }};
    xdg_popup_add_listener(popup, &listener, this);
}

wlcs::XdgPopupStable::~XdgPopupStable()
{
    xdg_popup_destroy(popup);
}
