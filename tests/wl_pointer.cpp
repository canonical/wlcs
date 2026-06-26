/*
 * Copyright © 2024 Canonical Ltd.
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

#include "in_process_server.h"
#include "expect_protocol_error.h"

#include <gmock/gmock.h>

using namespace testing;

namespace
{
class WlPointerTest : public wlcs::StartedInProcessServer
{
public:
    WlPointerTest()
        : client{the_server()},
          pointer{the_server().create_pointer()},
          surface{client.create_visible_surface(surface_width, surface_height)}
    {
        the_server().move_surface_to(surface, surface_x, surface_y);
    }

    /// Moves the pointer onto the test surface and returns the serial of the
    /// wl_pointer.enter event this produces. Many wl_pointer requests, most
    /// notably set_cursor, must be issued with a valid enter serial.
    auto move_pointer_to_surface() -> uint32_t
    {
        std::optional<uint32_t> enter_serial;
        client.add_pointer_enter_notification(
            [&](auto, auto, auto)
            {
                // Inside the enter handler the client's latest serial is, by
                // definition, the enter serial.
                enter_serial = client.latest_serial();
                return false;
            });
        pointer.move_to(surface_x + pointer_offset_x, surface_y + pointer_offset_y);
        client.dispatch_until([&]{ return enter_serial.has_value(); });
        return enter_serial.value();
    }

    /// A role-less surface suitable for use as a cursor.
    auto make_cursor_surface() -> wlcs::Surface
    {
        wlcs::Surface cursor{client};
        cursor.attach_buffer(cursor_size, cursor_size);
        return cursor;
    }

    wlcs::Client client;
    wlcs::Pointer pointer;
    wlcs::Surface surface;

    static int const surface_width = 200;
    static int const surface_height = 200;
    static int const surface_x = 100;
    static int const surface_y = 100;
    static int const pointer_offset_x = 50;
    static int const pointer_offset_y = 50;
    static int const cursor_size = 24;
    static int const cursor_hotspot_x = 4;
    static int const cursor_hotspot_y = 4;
};
}

TEST_F(WlPointerTest, set_cursor_with_a_role_less_surface_is_accepted)
{
    auto const enter_serial = move_pointer_to_surface();

    auto cursor = make_cursor_surface();

    EXPECT_NO_THROW({
        wl_pointer_set_cursor(
            client.the_pointer(), enter_serial, cursor, cursor_hotspot_x, cursor_hotspot_y);
        wl_surface_commit(cursor);
        client.roundtrip();
    });
}

TEST_F(WlPointerTest, set_cursor_with_null_surface_hides_the_cursor)
{
    auto const enter_serial = move_pointer_to_surface();

    EXPECT_NO_THROW({
        wl_pointer_set_cursor(client.the_pointer(), enter_serial, nullptr, 0, 0);
        client.roundtrip();
    });
}

TEST_F(WlPointerTest, set_cursor_with_null_surface_hides_a_previously_set_cursor)
{
    auto const enter_serial = move_pointer_to_surface();

    auto cursor = make_cursor_surface();

    EXPECT_NO_THROW({
        wl_pointer_set_cursor(
            client.the_pointer(), enter_serial, cursor, cursor_hotspot_x, cursor_hotspot_y);
        wl_surface_commit(cursor);
        client.roundtrip();
        wl_pointer_set_cursor(client.the_pointer(), enter_serial, nullptr, 0, 0);
        client.roundtrip();
    });
}

TEST_F(WlPointerTest, set_cursor_may_reassign_the_cursor_role_to_the_same_surface)
{
    auto const enter_serial = move_pointer_to_surface();

    auto cursor = make_cursor_surface();

    // The Wayland spec explicitly allows giving a surface the cursor role
    // again; this must not raise a protocol error.
    EXPECT_NO_THROW({
        wl_pointer_set_cursor(
            client.the_pointer(), enter_serial, cursor, cursor_hotspot_x, cursor_hotspot_y);
        wl_surface_commit(cursor);
        client.roundtrip();
        wl_pointer_set_cursor(
            client.the_pointer(), enter_serial, cursor, cursor_hotspot_x + 1, cursor_hotspot_y + 1);
        wl_surface_commit(cursor);
        client.roundtrip();
    });
}

TEST_F(WlPointerTest, set_cursor_on_a_surface_with_an_xdg_role_is_a_protocol_error)
{
    auto const enter_serial = move_pointer_to_surface();

    // A mapped xdg_toplevel surface already holds the toplevel role.
    auto roled_surface = client.create_visible_surface(surface_width, surface_height);

    EXPECT_PROTOCOL_ERROR({
        wl_pointer_set_cursor(
            client.the_pointer(), enter_serial, roled_surface, cursor_hotspot_x, cursor_hotspot_y);
        client.roundtrip();
    }, &wl_pointer_interface, WL_POINTER_ERROR_ROLE);
}

TEST_F(WlPointerTest, set_cursor_on_a_subsurface_is_a_protocol_error)
{
    auto const enter_serial = move_pointer_to_surface();

    // Creating a subsurface gives the wl_surface the subsurface role.
    wlcs::Subsurface subsurface{surface};

    EXPECT_PROTOCOL_ERROR({
        wl_pointer_set_cursor(
            client.the_pointer(), enter_serial, subsurface, cursor_hotspot_x, cursor_hotspot_y);
        client.roundtrip();
    }, &wl_pointer_interface, WL_POINTER_ERROR_ROLE);
}

TEST_F(WlPointerTest, set_cursor_with_a_stale_serial_is_ignored)
{
    auto const enter_serial = move_pointer_to_surface();

    // A mapped xdg_toplevel surface already holds the toplevel role, so it
    // would be rejected if the serial were honoured. The spec requires the
    // request to be ignored when the serial does not match the latest enter,
    // so no protocol error must be raised.
    auto roled_surface = client.create_visible_surface(surface_width, surface_height);

    EXPECT_NO_THROW({
        wl_pointer_set_cursor(
            client.the_pointer(), enter_serial + 1, roled_surface, cursor_hotspot_x, cursor_hotspot_y);
        client.roundtrip();
    });
}

TEST_F(WlPointerTest, a_cursor_surface_cannot_be_given_a_different_role)
{
    if (!client.xdg_shell_stable())
        GTEST_SKIP() << "Compositor does not support xdg_wm_base";

    auto const enter_serial = move_pointer_to_surface();

    auto cursor = make_cursor_surface();

    wl_pointer_set_cursor(
        client.the_pointer(), enter_serial, cursor, cursor_hotspot_x, cursor_hotspot_y);
    wl_surface_commit(cursor);
    client.roundtrip();

    // The surface now has the cursor role, so trying to give it the xdg
    // surface role must fail.
    EXPECT_PROTOCOL_ERROR({
        auto xdg_surface = xdg_wm_base_get_xdg_surface(client.xdg_shell_stable(), cursor);
        client.roundtrip();
        // (Unreachable on conformant servers, but keep the proxy tidy.)
        xdg_surface_destroy(xdg_surface);
    }, &xdg_wm_base_interface, XDG_WM_BASE_ERROR_ROLE);
}
