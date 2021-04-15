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

#include "relative_pointer_unstable_v1.h"

#include "in_process_server.h"
#include <version_specifier.h>

wlcs::ZwpRelativePointerManagerV1::ZwpRelativePointerManagerV1(Client& client) :
    manager{client.bind_if_supported<zwp_relative_pointer_manager_v1>(AnyVersion)}
{

}

wlcs::ZwpRelativePointerManagerV1::~ZwpRelativePointerManagerV1() = default;

wlcs::ZwpRelativePointerManagerV1::operator zwp_relative_pointer_manager_v1*() const
{
    return manager;
}

wlcs::ZwpRelativePointerV1::ZwpRelativePointerV1(wlcs::ZwpRelativePointerManagerV1& manager, wl_pointer* pointer) :
    relative_pointer{zwp_relative_pointer_manager_v1_get_relative_pointer(manager, pointer)},
    version{zwp_relative_pointer_v1_get_version(relative_pointer)}
{
    zwp_relative_pointer_v1_set_user_data(relative_pointer, this);
    zwp_relative_pointer_v1_add_listener(relative_pointer, &listener, this);
}

wlcs::ZwpRelativePointerV1::~ZwpRelativePointerV1()
{
    zwp_relative_pointer_v1_destroy(relative_pointer);
}

wlcs::ZwpRelativePointerV1::operator zwp_relative_pointer_v1*() const
{
    return relative_pointer;
}

zwp_relative_pointer_v1_listener const wlcs::ZwpRelativePointerV1::listener =
    {
        [](void* self, auto*, auto... args) { static_cast<ZwpRelativePointerV1*>(self)->relative_motion(args...); }
    };
