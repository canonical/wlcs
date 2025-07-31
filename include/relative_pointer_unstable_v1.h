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
 *
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#ifndef WLCS_RELATIVE_POINTER_UNSTABLE_V1_H
#define WLCS_RELATIVE_POINTER_UNSTABLE_V1_H

#include "generated/relative-pointer-unstable-v1-client.h"
#include "wl_interface_descriptor.h"
#include "wl_handle.h"

#include <gmock/gmock.h>

#include <memory>

struct wl_pointer;

namespace wlcs
{
WLCS_CREATE_INTERFACE_DESCRIPTOR(zwp_relative_pointer_manager_v1)

class Client;

class ZwpRelativePointerManagerV1
{
public:
    ZwpRelativePointerManagerV1(Client& client);
    ~ZwpRelativePointerManagerV1();

    operator zwp_relative_pointer_manager_v1*() const;

    friend void zwp_relative_pointer_manager_v1_destroy(ZwpRelativePointerManagerV1 const&) = delete;

private:
    WlHandle<zwp_relative_pointer_manager_v1> manager;
};

class ZwpRelativePointerV1
{
public:
    ZwpRelativePointerV1(ZwpRelativePointerManagerV1& manager, wl_pointer* pointer);
    ~ZwpRelativePointerV1();

    MOCK_METHOD(void, relative_motion,
                 (uint32_t utime_hi, uint32_t utime_lo,
                 wl_fixed_t dx, wl_fixed_t dy,
                 wl_fixed_t dx_unaccel, wl_fixed_t dy_unaccel));

    operator zwp_relative_pointer_v1*() const;

    friend void zwp_relative_pointer_v1_destroy(ZwpRelativePointerV1 const&) = delete;

private:
    zwp_relative_pointer_v1* const relative_pointer;
    uint32_t const version;
    static zwp_relative_pointer_v1_listener const listener;
};

}

#endif //WLCS_RELATIVE_POINTER_UNSTABLE_V1_H
