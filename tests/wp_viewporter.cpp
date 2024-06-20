/*
 * Copyright © Canonical Ltd.
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
#include "version_specifier.h"
#include "wayland-util.h"

#include "generated/viewporter-client.h"

#include <utility>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace testing;
using namespace wlcs;

class WpViewporterTest : public wlcs::InProcessServer
{
public:
    WpViewporterTest() = default;

    auto surface_has_size(wlcs::Client& client, wlcs::Surface& surface, int32_t width, int32_t height) -> bool
    {
        the_server().move_surface_to(surface, 100, 100);

        auto pointer = the_server().create_pointer();

        bool pointer_entered = false;
        bool motion_received = false;
        client.add_pointer_enter_notification(
            [&](auto entered_surface, auto, auto)
            {
                pointer_entered = entered_surface == surface;
                return false;
            });

        pointer.move_to(100, 100);
        client.dispatch_until([&pointer_entered]() { return pointer_entered; });

        // Should be on the top left of the surface
        if (client.window_under_cursor() != surface)
        {
            ADD_FAILURE() << "Surface is not mapped at expected location?";
            return false;
        }
        if (client.pointer_position() != std::make_pair(wl_fixed_from_int(0), wl_fixed_from_int(0)))
        {
            BOOST_THROW_EXCEPTION((std::runtime_error{"Surface at unexpected location (test harness bug?)"}));
        }

        client.add_pointer_motion_notification(
            [&](auto, auto)
            {
                motion_received = true;
                return false;
            });

        pointer.move_by(width - 1, height - 1);
        client.dispatch_until([&motion_received]() { return motion_received; });

        // Should now be at bottom corner of surface
        if (client.window_under_cursor() != surface)
        {
            ADD_FAILURE() << "Surface size too small";
            return false;
        }
        if (client.pointer_position() != std::make_pair(wl_fixed_from_int(width - 1), wl_fixed_from_int(height - 1)))
        {
            auto const [x, y] = client.pointer_position();
            ADD_FAILURE() << "Surface coordinate system incorrect; expected (" << width - 1 << ", " << height - 1 << ") got ("
                << wl_fixed_to_double(x) << ", " << wl_fixed_to_double(y) << ")";
            return false;
        }

        // Moving any further should take us out of the surface
        motion_received = false;

        client.add_pointer_leave_notification(
            [&](auto)
            {
                pointer_entered = false;
                return false;
            });
        client.add_pointer_motion_notification(
            [&](auto, auto)
            {
                motion_received = true;
                return false;
            });

        pointer.move_by(1, 1);
        client.dispatch_until([&]() { return !pointer_entered || motion_received; });

        if (client.window_under_cursor() == surface)
        {
            ADD_FAILURE() << "Surface size too large";
            return false;
        }

        return true;
    }
};

WLCS_CREATE_INTERFACE_DESCRIPTOR(wp_viewporter)


TEST_F(WpViewporterTest, set_destination_sets_output_size)
{
    wlcs::Client client{the_server()};

    int const buffer_width{100}, buffer_height{100}, display_width{83}, display_height{20};

    auto surface = client.create_visible_surface(buffer_width, buffer_height);
    auto buffer = ShmBuffer(client, buffer_width, buffer_height);

    auto viewporter = client.bind_if_supported<wp_viewporter>(wlcs::AnyVersion);
    auto viewport = wp_viewporter_get_viewport(viewporter, surface);

    bool committed = false;

    wp_viewport_set_destination(viewport, display_width, display_height);
    // Oh! I think we handle changing surface properties incorrectly!
    wl_surface_attach(surface, buffer, 0, 0);
    surface.add_frame_callback([&committed](auto) { committed = true; });
    wl_surface_commit(surface);

    client.dispatch_until([&committed]() { return committed;} );

    EXPECT_TRUE(surface_has_size(client, surface, display_width, display_height));
}

TEST_F(WpViewporterTest, committing_new_destination_without_new_buffer_still_changes_size)
{
    wlcs::Client client{the_server()};

    int const buffer_width{100}, buffer_height{100}, display_width{83}, display_height{20};

    auto surface = client.create_visible_surface(buffer_width, buffer_height);

    auto viewporter = client.bind_if_supported<wp_viewporter>(wlcs::AnyVersion);
    auto viewport = wp_viewporter_get_viewport(viewporter, surface);

    bool committed = false;

    wp_viewport_set_destination(viewport, display_width, display_height);
    surface.add_frame_callback([&committed](auto) { committed = true; });
    wl_surface_commit(surface);

    client.dispatch_until([&committed]() { return committed;} );

    EXPECT_TRUE(surface_has_size(client, surface, display_width, display_height));
}

TEST_F(WpViewporterTest, when_source_but_no_destination_set_window_has_src_size)
{
    wlcs::Client client{the_server()};

    int const buffer_width{100}, buffer_height{100}, display_width{83}, display_height{20};

    auto surface = client.create_visible_surface(buffer_width, buffer_height);
    auto buffer = ShmBuffer(client, buffer_width, buffer_height);

    auto viewporter = client.bind_if_supported<wp_viewporter>(wlcs::AnyVersion);
    auto viewport = wp_viewporter_get_viewport(viewporter, surface);

    bool committed = false;

    wp_viewport_set_source(viewport, 0, 0, wl_fixed_from_int(display_width), wl_fixed_from_int(display_height));
    // Oh! I think we handle changing surface properties incorrectly!
    wl_surface_attach(surface, buffer, 0, 0);
    surface.add_frame_callback([&committed](auto) { committed = true; });
    wl_surface_commit(surface);

    client.dispatch_until([&committed]() { return committed;} );

    EXPECT_TRUE(surface_has_size(client, surface, display_width, display_height));
}

TEST_F(WpViewporterTest, when_buffer_is_scaled_destination_is_in_scaled_coordinates)
{
    wlcs::Client client{the_server()};


    int const buffer_width{100}, buffer_height{100}, display_width{82}, display_height{20};
    int const scale{2};

    auto surface = client.create_visible_surface(buffer_width, buffer_height);
    auto buffer = ShmBuffer(client, buffer_width, buffer_height);

    auto viewporter = client.bind_if_supported<wp_viewporter>(wlcs::AnyVersion);
    auto viewport = wp_viewporter_get_viewport(viewporter, surface);

    bool committed = false;

    wl_surface_set_buffer_scale(surface, scale);
    wp_viewport_set_destination(viewport, display_width, display_height);
    surface.add_frame_callback([&committed](auto) { committed = true; });
    wl_surface_commit(surface);

    client.dispatch_until([&committed]() { return committed;} );

    EXPECT_TRUE(surface_has_size(client, surface, display_width, display_height));
}

TEST_F(WpViewporterTest, when_buffer_is_scaled_source_is_in_scaled_coordinates)
{
    /* TODO: It would be nice to check that the surface is *actually* sampling from
     * the appropriate source rectangle, rather than relying on the indirect “does it
     * end up the right size” method here.
     *
     * That would require sampling-from-rendering support in WLCS and associated
     * compositors-under-test.
     */

    wlcs::Client client{the_server()};

    int const buffer_width{100}, buffer_height{100}, display_width{82}, display_height{20};
    int const scale{2};

    auto surface = client.create_visible_surface(buffer_width, buffer_height);
    auto buffer = ShmBuffer(client, buffer_width, buffer_height);

    auto viewporter = client.bind_if_supported<wp_viewporter>(wlcs::AnyVersion);
    auto viewport = wp_viewporter_get_viewport(viewporter, surface);

    bool committed = false;

    wl_surface_set_buffer_scale(surface, scale);
    wp_viewport_set_source(viewport, wl_fixed_from_int(0), wl_fixed_from_int(0), wl_fixed_from_int(display_width), wl_fixed_from_int(display_height));
    surface.add_frame_callback([&committed](auto) { committed = true; });
    wl_surface_commit(surface);

    client.dispatch_until([&committed]() { return committed;} );

    EXPECT_TRUE(surface_has_size(client, surface, display_width, display_height));
}
