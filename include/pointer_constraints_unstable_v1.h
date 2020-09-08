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

#ifndef WLCS_POINTER_CONSTRAINTS_UNSTABLE_V1_H
#define WLCS_POINTER_CONSTRAINTS_UNSTABLE_V1_H

#include "generated/pointer-constraints-unstable-v1-client.h"
#include "wl_interface_descriptor.h"
#include "wl_handle.h"

#include <gmock/gmock.h>

#include <memory>

struct wl_pointer;

namespace wlcs
{
WLCS_CREATE_INTERFACE_DESCRIPTOR(zwp_pointer_constraints_v1)

class Client;

class ZwpPointerConstraintsV1
{
public:
    ZwpPointerConstraintsV1(Client& client);
    ~ZwpPointerConstraintsV1();

    operator zwp_pointer_constraints_v1*() const;

    friend void zwp_pointer_constraints_v1_destroy(ZwpPointerConstraintsV1 const&) = delete;

private:
    WlHandle<zwp_pointer_constraints_v1> const manager;
};

class ZwpConfinedPointerV1
{
public:
    ZwpConfinedPointerV1(
        ZwpPointerConstraintsV1& manager,
        wl_surface* surface,
        wl_pointer* pointer,
        wl_region* region,
        uint32_t lifetime);

    ~ZwpConfinedPointerV1();

    MOCK_METHOD0(confined, void());
    MOCK_METHOD0(unconfined, void());

    operator zwp_confined_pointer_v1*() const;

    friend void zwp_confined_pointer_v1_destroy(ZwpConfinedPointerV1 const&) = delete;

private:
    zwp_confined_pointer_v1* const relative_pointer;
    uint32_t const version;
    static zwp_confined_pointer_v1_listener const listener;
};

class ZwpLockedPointerV1
{
public:
    ZwpLockedPointerV1(
        ZwpPointerConstraintsV1& manager,
        wl_surface* surface,
        wl_pointer* pointer,
        wl_region* region,
        uint32_t lifetime);

    ~ZwpLockedPointerV1();

    MOCK_METHOD0(locked, void());
    MOCK_METHOD0(unlocked, void());

    operator zwp_locked_pointer_v1*() const;

    friend void zwp_locked_pointer_v1_destroy(ZwpLockedPointerV1 const&) = delete;

private:
    zwp_locked_pointer_v1* const relative_pointer;
    uint32_t const version;
    static zwp_locked_pointer_v1_listener const listener;
};

}

#endif //WLCS_POINTER_CONSTRAINTS_UNSTABLE_V1_H
