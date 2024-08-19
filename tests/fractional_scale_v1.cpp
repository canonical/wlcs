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

#include "expect_protocol_error.h"
#include "generated/fractional-scale-v1-client.h"
#include "in_process_server.h"
#include "version_specifier.h"
#include "wl_handle.h"

using namespace wlcs;
using testing::Eq;

#define WL_INTERFACE_DESCRIPTOR_SPECIALIZATION(WAYLAND_TYPE)                                                           \
    template <> struct wlcs::WlInterfaceDescriptor<WAYLAND_TYPE>                                                       \
    {                                                                                                                  \
        static constexpr bool const has_specialisation = true;                                                         \
        static constexpr wl_interface const* const interface = &WAYLAND_TYPE##_interface;                              \
        static constexpr void (*const destructor)(WAYLAND_TYPE*) = &WAYLAND_TYPE##_destroy;                            \
    };

WL_INTERFACE_DESCRIPTOR_SPECIALIZATION(wp_fractional_scale_manager_v1)
WL_INTERFACE_DESCRIPTOR_SPECIALIZATION(wp_fractional_scale_v1)

class FractionalScaleV1Test : public wlcs::StartedInProcessServer
{
public:
    Client a_client{the_server()};
    Surface a_surface{a_client};
    WlHandle<wp_fractional_scale_manager_v1> fractional_scale_manager{
        a_client.bind_if_supported<wp_fractional_scale_manager_v1>(AnyVersion)};
};

TEST_F(FractionalScaleV1Test, fractional_scale_creation_does_not_throw)
{
    auto fractional_scale =
        wrap_wl_object(wp_fractional_scale_manager_v1_get_fractional_scale(fractional_scale_manager, a_surface));

    EXPECT_NO_THROW(a_client.roundtrip());
}

TEST_F(FractionalScaleV1Test, duplicate_fractional_scales_throw_fractional_scale_exists)
{
    auto fractional_scale1 =
        wrap_wl_object(wp_fractional_scale_manager_v1_get_fractional_scale(fractional_scale_manager, a_surface));
    auto fractional_scale2 =
        wrap_wl_object(wp_fractional_scale_manager_v1_get_fractional_scale(fractional_scale_manager, a_surface));

    EXPECT_PROTOCOL_ERROR(
        { a_client.roundtrip(); },
        &wp_fractional_scale_manager_v1_interface,
        WP_FRACTIONAL_SCALE_MANAGER_V1_ERROR_FRACTIONAL_SCALE_EXISTS);
}
