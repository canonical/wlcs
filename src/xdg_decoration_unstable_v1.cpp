/*
 * Copyright © 2020 Canonical Ltd.
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
 */

#include "xdg_decoration_unstable_v1.h"

#include "generated/xdg-decoration-unstable-v1-client.h"
#include "in_process_server.h"
#include "version_specifier.h"

wlcs::ZxdgDecorationManagerV1::ZxdgDecorationManagerV1(Client& client) :
    manager{client.bind_if_supported<zxdg_decoration_manager_v1>(AnyVersion)}
{
}

wlcs::ZxdgDecorationManagerV1::~ZxdgDecorationManagerV1() = default;

wlcs::ZxdgDecorationManagerV1::operator zxdg_decoration_manager_v1*() const
{
    return manager;
}

wlcs::ZxdgToplevelDecorationV1::ZxdgToplevelDecorationV1(ZxdgDecorationManagerV1& manager, xdg_toplevel* toplevel) :
    version{zxdg_decoration_manager_v1_get_version(manager)},
    toplevel_decoration{zxdg_decoration_manager_v1_get_toplevel_decoration(manager, toplevel)}
{
    zxdg_toplevel_decoration_v1_set_user_data(toplevel_decoration, this);
    zxdg_toplevel_decoration_v1_add_listener(toplevel_decoration, &listener, this);
}

wlcs::ZxdgToplevelDecorationV1::~ZxdgToplevelDecorationV1() { zxdg_toplevel_decoration_v1_destroy(toplevel_decoration); }

void wlcs::ZxdgToplevelDecorationV1::set_mode(uint32_t mode)
{
    zxdg_toplevel_decoration_v1_set_mode(toplevel_decoration, mode);
}

void wlcs::ZxdgToplevelDecorationV1::unset_mode()
{
    zxdg_toplevel_decoration_v1_unset_mode(toplevel_decoration);
}

wlcs::ZxdgToplevelDecorationV1::operator zxdg_toplevel_decoration_v1*() const
{
    return toplevel_decoration;
}

zxdg_toplevel_decoration_v1_listener const wlcs::ZxdgToplevelDecorationV1::listener = {
    [](void* self, auto*, auto... args)
    {
        static_cast<ZxdgToplevelDecorationV1*>(self)->configure(args...);
    }};
