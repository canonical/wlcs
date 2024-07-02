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

#ifndef WLCS_XDG_DECORATION_UNSTABLE_V1_H
#define WLCS_XDG_DECORATION_UNSTABLE_V1_H

#include "generated/xdg-decoration-unstable-v1-client.h"
#include "wl_handle.h"
#include "wl_interface_descriptor.h"
#include "gmock/gmock.h"

namespace wlcs
{
WLCS_CREATE_INTERFACE_DESCRIPTOR(zxdg_decoration_manager_v1);

class Client;

class ZxdgDecorationManagerV1
{
public:
    ZxdgDecorationManagerV1(Client& client);
    ~ZxdgDecorationManagerV1();

    operator zxdg_decoration_manager_v1*() const;
    friend void zxdg_decoration_manager_v1_destroy(ZxdgDecorationManagerV1 const&) = delete;

private:
    WlHandle<zxdg_decoration_manager_v1> manager;
};

class ZxdgToplevelDecorationV1
{
public:
    ZxdgToplevelDecorationV1(ZxdgDecorationManagerV1& manager, xdg_toplevel* toplevel);
    ~ZxdgToplevelDecorationV1();

    MOCK_METHOD(void, configure, (uint32_t mode), ());

    void set_mode(uint32_t mode);
    void unset_mode();

    operator zxdg_toplevel_decoration_v1*() const;
    friend void zxdg_toplevel_manager_v1_destroy(ZxdgToplevelDecorationV1 const&) = delete;

private:
    uint32_t const version;
    zxdg_toplevel_decoration_v1* const toplevel_decoration;

    static zxdg_toplevel_decoration_v1_listener const listener;
};
}

#endif // WLCS_XDG_DECORATION_UNSTABLE_V1_H
