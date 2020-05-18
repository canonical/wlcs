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
 * Authored by: Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#ifndef WLCS_WL_INTERFACE_DESCRIPTOR_H
#define WLCS_WL_INTERFACE_DESCRIPTOR_H

struct wl_interface;

namespace wlcs
{
/***
 * A specialisable struct containing the constants and types associated with a Wayland protocol
 *
 * \tparam WlType   The base Wayland object type (eg; wl_surface, xdg_wm_base, etc)
 */
template<typename WlType>
struct WlInterfaceDescriptor
{
    // Needed because apparently GCC < 10 can't compare a constexpr wl_interface const* const to nullptr in constexpr context?!
    static constexpr bool const has_specialisation = false;
    static constexpr wl_interface const* const interface = nullptr;
    static constexpr void (* const destructor)(WlType*) = nullptr;
};
}

/***
 * Declare a specialisation of WlInterfaceDescriptor for \param name
 *
 * This will use the standard Wayland conventions of
 * name - name_interface - name_destroy
 * (eg: wl_surface - wl_surface_interface - wl_surface_destroy)
 */
#define WLCS_CREATE_INTERFACE_DESCRIPTOR(name) \
    template<> \
    struct WlInterfaceDescriptor<name> \
    { \
        static constexpr bool const has_specialisation = true; \
        static constexpr wl_interface const* const interface = &name##_interface; \
        static constexpr void(* const destructor)(name*) = &name##_destroy; \
    };


#endif //WLCS_WL_INTERFACE_DESCRIPTOR_H
