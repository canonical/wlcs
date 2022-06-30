/*
 * Copyright © 2022 Canonical Ltd.
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

#include "in_process_server.h"
#include "version_specifier.h"
#include "generated/wlr-virtual-pointer-unstable-v1-client.h"

#include "linux/input.h"
#include <gmock/gmock.h>

using namespace testing;

namespace wlcs
{
WLCS_CREATE_INTERFACE_DESCRIPTOR(zwlr_virtual_pointer_manager_v1)
WLCS_CREATE_INTERFACE_DESCRIPTOR(zwlr_virtual_pointer_v1)
}

namespace
{
class VirtualPointerV1Test: public wlcs::StartedInProcessServer
{
public:
    VirtualPointerV1Test()
        : client{the_server()},
          surface{client.create_visible_surface(surface_width, surface_height)},
          pointer{the_server().create_pointer()},
          manager{client.bind_if_supported<zwlr_virtual_pointer_manager_v1>(wlcs::AnyVersion)}
    {
        the_server().move_surface_to(surface, 0, 0);
        pointer.move_to(pointer_start_x, pointer_start_y);
        client.roundtrip();
    }

    wlcs::Client client;
    wlcs::Surface surface;
    wlcs::Pointer pointer;
    wlcs::WlHandle<zwlr_virtual_pointer_manager_v1> manager;

    static int const surface_width = 400;
    static int const surface_height = 400;
    static int const pointer_start_x = 20;
    static int const pointer_start_y = 30;
};
}

TEST_F(VirtualPointerV1Test, when_virutal_pointer_is_moved_client_sees_motion)
{
    auto const handle = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(manager, nullptr);
    int const motion_x = 7;
    int const motion_y = 22;
    zwlr_virtual_pointer_v1_motion(handle, 0, motion_x, motion_y);
    zwlr_virtual_pointer_v1_frame(handle);
    client.roundtrip();
    ASSERT_THAT(client.window_under_cursor(), Eq(surface.operator wl_surface*()));
    EXPECT_THAT(client.pointer_position(), Eq(std::make_pair(
        wl_fixed_from_int(pointer_start_x + motion_x),
        wl_fixed_from_int(pointer_start_y + motion_y))));
}

TEST_F(VirtualPointerV1Test, when_virutal_pointer_clicks_client_sees_button)
{
    auto const handle = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(manager, nullptr);
    StrictMock<MockFunction<void(uint32_t, uint32_t, bool)>> button_callback;
    client.add_pointer_button_notification([&](auto... args) { button_callback.Call(args...); return false; });
    EXPECT_CALL(button_callback, Call(_, BTN_LEFT, true));
    zwlr_virtual_pointer_v1_button(handle, 0, BTN_LEFT, WL_POINTER_BUTTON_STATE_PRESSED);
    zwlr_virtual_pointer_v1_frame(handle);
    client.roundtrip();
}
