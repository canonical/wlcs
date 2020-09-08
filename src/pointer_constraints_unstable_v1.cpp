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

#include "pointer_constraints_unstable_v1.h"

#include "in_process_server.h"
#include <version_specifier.h>

wlcs::ZwpPointerConstraintsV1::ZwpPointerConstraintsV1(Client& client) :
    manager{client.bind_if_supported<zwp_pointer_constraints_v1>(AnyVersion)}
{
}

wlcs::ZwpPointerConstraintsV1::~ZwpPointerConstraintsV1() = default;

wlcs::ZwpPointerConstraintsV1::operator zwp_pointer_constraints_v1*() const
{
    return manager;
}

wlcs::ZwpConfinedPointerV1::ZwpConfinedPointerV1(
    ZwpPointerConstraintsV1& manager,
    wl_surface* surface,
    wl_pointer* pointer,
    wl_region* region,
    uint32_t lifetime) :
    relative_pointer{zwp_pointer_constraints_v1_confine_pointer(manager, surface, pointer, region, lifetime)},
    version{zwp_confined_pointer_v1_get_version(relative_pointer)}
{
    zwp_confined_pointer_v1_set_user_data(relative_pointer, this);
    zwp_confined_pointer_v1_add_listener(relative_pointer, &listener, this);
}

wlcs::ZwpConfinedPointerV1::~ZwpConfinedPointerV1()
{
    zwp_confined_pointer_v1_destroy(relative_pointer);
}

wlcs::ZwpConfinedPointerV1::operator zwp_confined_pointer_v1*() const
{
    return relative_pointer;
}

zwp_confined_pointer_v1_listener const wlcs::ZwpConfinedPointerV1::listener =
    {
        [](void* self, auto*) { static_cast<ZwpConfinedPointerV1*>(self)->confined(); },
        [](void* self, auto*) { static_cast<ZwpConfinedPointerV1*>(self)->unconfined(); },
    };


wlcs::ZwpLockedPointerV1::ZwpLockedPointerV1(
    ZwpPointerConstraintsV1& manager,
    wl_surface* surface,
    wl_pointer* pointer,
    wl_region* region,
    uint32_t lifetime) :
    relative_pointer{zwp_pointer_constraints_v1_lock_pointer(manager, surface, pointer, region, lifetime)},
    version{zwp_locked_pointer_v1_get_version(relative_pointer)}
{
    zwp_locked_pointer_v1_set_user_data(relative_pointer, this);
    zwp_locked_pointer_v1_add_listener(relative_pointer, &listener, this);
}

wlcs::ZwpLockedPointerV1::~ZwpLockedPointerV1()
{
    zwp_locked_pointer_v1_destroy(relative_pointer);
}

wlcs::ZwpLockedPointerV1::operator zwp_locked_pointer_v1*() const
{
    return relative_pointer;
}

zwp_locked_pointer_v1_listener const wlcs::ZwpLockedPointerV1::listener =
    {
        [](void* self, auto*) { static_cast<ZwpLockedPointerV1*>(self)->locked(); },
        [](void* self, auto*) { static_cast<ZwpLockedPointerV1*>(self)->unlocked(); },
    };
