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
#include "expect_protocol_error.h"
#include "version_specifier.h"
#include "wayland-util.h"

#include "generated/viewporter-client.h"

#include <utility>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace testing;
using namespace wlcs;

namespace wlcs
{
WLCS_CREATE_INTERFACE_DESCRIPTOR(wp_viewporter)
WLCS_CREATE_INTERFACE_DESCRIPTOR(wp_viewport)
}


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

        // First ensure we are *not* on the surface...
        pointer.move_to(0, 0);
        // ...then move onto the surface, so our enter notification fires
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
        client.add_pointer_leave_notification(
            [&](auto)
            {
                pointer_entered = false;
                return false;
            });

        pointer.move_by(width - 1, height - 1);
        client.dispatch_until([&]() { return motion_received || !pointer_entered; });

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

TEST_F(WpViewporterTest, set_destination_sets_output_size)
{
    wlcs::Client client{the_server()};

    int const buffer_width{100}, buffer_height{100}, display_width{83}, display_height{20};

    auto surface = client.create_visible_surface(buffer_width, buffer_height);
    auto buffer = ShmBuffer(client, buffer_width, buffer_height);

    auto viewporter = client.bind_if_supported<wp_viewporter>(wlcs::AnyVersion);
    auto viewport = wrap_wl_object(wp_viewporter_get_viewport(viewporter, surface));

    bool committed = false;

    wp_viewport_set_destination(viewport, display_width, display_height);
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
    auto viewport = wrap_wl_object(wp_viewporter_get_viewport(viewporter, surface));

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
    auto viewport = wrap_wl_object(wp_viewporter_get_viewport(viewporter, surface));

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
    auto viewport = wrap_wl_object(wp_viewporter_get_viewport(viewporter, surface));

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

    int const buffer_width{200}, buffer_height{100}, display_width{82}, display_height{20};
    int const scale{2};

    auto surface = client.create_visible_surface(buffer_width, buffer_height);
    auto buffer = ShmBuffer(client, buffer_width, buffer_height);

    auto viewporter = client.bind_if_supported<wp_viewporter>(wlcs::AnyVersion);
    auto viewport = wrap_wl_object(wp_viewporter_get_viewport(viewporter, surface));

    bool committed = false;

    wl_surface_set_buffer_scale(surface, scale);
    wp_viewport_set_source(viewport, wl_fixed_from_int(0), wl_fixed_from_int(0), wl_fixed_from_int(display_width), wl_fixed_from_int(display_height));
    surface.add_frame_callback([&committed](auto) { committed = true; });
    wl_surface_commit(surface);

    client.dispatch_until([&committed]() { return committed;} );

    EXPECT_TRUE(surface_has_size(client, surface, display_width, display_height));
}

TEST_F(WpViewporterTest, when_destination_is_not_set_source_must_have_integer_size)
{
    wlcs::Client client{the_server()};

    auto surface = client.create_visible_surface(200, 100);

    auto viewporter = client.bind_if_supported<wp_viewporter>(wlcs::AnyVersion);
    auto viewport = wrap_wl_object(wp_viewporter_get_viewport(viewporter, surface));

    bool committed = false;

    wp_viewport_set_source(viewport, wl_fixed_from_int(0), wl_fixed_from_int(0), wl_fixed_from_double(23.2f), wl_fixed_from_int(100));
    surface.add_frame_callback([&committed](auto) { committed = true; });
    wl_surface_commit(surface);

    EXPECT_PROTOCOL_ERROR(
        {
            client.dispatch_until([&committed]() { return committed;} );
        },
        &wp_viewport_interface,
        WP_VIEWPORT_ERROR_BAD_SIZE
    );
}

TEST_F(WpViewporterTest, source_rectangle_out_of_buffer_bounds_raises_protocol_error)
{
    wlcs::Client client{the_server()};

    auto surface = client.create_visible_surface(200, 100);

    auto viewporter = client.bind_if_supported<wp_viewporter>(wlcs::AnyVersion);
    auto viewport = wrap_wl_object(wp_viewporter_get_viewport(viewporter, surface));

    bool committed = false;

    // Set the surface scale to test that interaction with surface coördinates
    wl_surface_set_buffer_scale(surface, 2);
    // Set a source viewport outside the (transformed) buffer coordinates - this corresponds to the rectangle with corners (100, 0) -> (201, 100)
    wp_viewport_set_source(viewport, wl_fixed_from_int(50), wl_fixed_from_int(0), wl_fixed_from_double(50.5), wl_fixed_from_int(50));
    surface.add_frame_callback([&committed](auto) { committed = true; });
    wl_surface_commit(surface);

    EXPECT_PROTOCOL_ERROR(
        {
            client.dispatch_until([&committed]() { return committed;} );
        },
        &wp_viewport_interface,
        WP_VIEWPORT_ERROR_OUT_OF_BUFFER
    );
}

TEST_F(WpViewporterTest, assigning_a_viewport_to_a_surface_with_an_existing_viewport_is_an_error)
{
    wlcs::Client client{the_server()};

    auto surface = client.create_visible_surface(200, 100);

    auto viewporter = client.bind_if_supported<wp_viewporter>(wlcs::AnyVersion);
    wp_viewporter_get_viewport(viewporter, surface);

    EXPECT_PROTOCOL_ERROR(
        {
            wp_viewporter_get_viewport(viewporter, surface);
            client.roundtrip();
        },
        &wp_viewporter_interface,
        WP_VIEWPORTER_ERROR_VIEWPORT_EXISTS
    );
}

TEST_F(WpViewporterTest, setting_destination_after_surface_has_been_destroyed_is_an_error)
{
    wlcs::Client client{the_server()};

    int const buffer_width{100}, buffer_height{100};

    auto viewporter = client.bind_if_supported<wp_viewporter>(wlcs::AnyVersion);
    struct wp_viewport* viewport = nullptr;

    {
        auto surface = client.create_visible_surface(buffer_width, buffer_height);
        viewport = wp_viewporter_get_viewport(viewporter, surface);
    }

    EXPECT_PROTOCOL_ERROR(
        {
            wp_viewport_set_destination(viewport, 10, 10);
            client.roundtrip();
        },
        &wp_viewport_interface,
        WP_VIEWPORT_ERROR_NO_SURFACE
    );
}

TEST_F(WpViewporterTest, setting_source_after_surface_has_been_destroyed_is_an_error)
{
    wlcs::Client client{the_server()};

    int const buffer_width{100}, buffer_height{100};

    auto viewporter = client.bind_if_supported<wp_viewporter>(wlcs::AnyVersion);
    struct wp_viewport* viewport = nullptr;

    {
        auto surface = client.create_visible_surface(buffer_width, buffer_height);
        viewport = wp_viewporter_get_viewport(viewporter, surface);
    }

    EXPECT_PROTOCOL_ERROR(
        {
            wp_viewport_set_source(viewport, wl_fixed_from_int(0), wl_fixed_from_int(0), wl_fixed_from_int(10), wl_fixed_from_int(10));
            client.roundtrip();
        },
        &wp_viewport_interface,
        WP_VIEWPORT_ERROR_NO_SURFACE
    );
}

TEST_F(WpViewporterTest, can_destroy_viewport_after_surface_is_destroyed)
{
    wlcs::Client client{the_server()};

    int const buffer_width{100}, buffer_height{100};

    auto viewporter = client.bind_if_supported<wp_viewporter>(wlcs::AnyVersion);
    struct wp_viewport* viewport = nullptr;

    {
        auto surface = client.create_visible_surface(buffer_width, buffer_height);
        viewport = wp_viewporter_get_viewport(viewporter, surface);
        client.roundtrip();
    }

    wp_viewport_destroy(viewport);
    client.roundtrip();
}

class WpViewporterSrcParamsTest : public WpViewporterTest, public testing::WithParamInterface<std::tuple<double, double, double, double, char const*>>
{
};

TEST_P(WpViewporterSrcParamsTest, raises_protocol_error_on_invalid_value)
{
    wlcs::Client client{the_server()};

    auto surface = client.create_visible_surface(200, 100);

    auto viewporter = client.bind_if_supported<wp_viewporter>(wlcs::AnyVersion);
    auto viewport = wrap_wl_object(wp_viewporter_get_viewport(viewporter, surface));

    auto const [x, y, width, height, _] = GetParam();

    EXPECT_PROTOCOL_ERROR(
        {
            wp_viewport_set_source(viewport, wl_fixed_from_double(x), wl_fixed_from_double(y), wl_fixed_from_double(width), wl_fixed_from_double(height));
            client.roundtrip();
        },
        &wp_viewport_interface,
        WP_VIEWPORT_ERROR_BAD_VALUE
    );
}

INSTANTIATE_TEST_SUITE_P(
    ,
    WpViewporterSrcParamsTest,
    Values(
        std::make_tuple(0, 0, -1, 0, "src_width_must_be_non_negative"),
        std::make_tuple(0, 0, 0, -1, "src_height_must_be_non_negative"),
        std::make_tuple(0, 0, 1, 0, "src_height_must_be_positive"),
        std::make_tuple(0, 0, 0, 1, "src_width_must_be_positive"),
        std::make_tuple(-1, 0, 0, 0, "src_x_must_be_non_negative"),
        std::make_tuple(0, -1, 0, 0, "src_y_must_be_non_negative")
    ),
    [](testing::TestParamInfo<WpViewporterSrcParamsTest::ParamType> const& info) -> std::string
    {
        return std::get<4>(info.param);
    });

TEST_F(WpViewporterTest, all_minus_one_source_unsets_source_rect)
{
    wlcs::Client client{the_server()};

    int const buffer_width{640}, buffer_height{480}, display_width{320}, display_height{200};

    auto surface = client.create_visible_surface(buffer_width, buffer_height);
    auto buffer = ShmBuffer(client, buffer_width, buffer_height);

    auto viewporter = client.bind_if_supported<wp_viewporter>(wlcs::AnyVersion);
    auto viewport = wrap_wl_object(wp_viewporter_get_viewport(viewporter, surface));

    bool committed = false;

    // First set the source viewport, and assert that we get the right surface size...
    wp_viewport_set_source(viewport, 0, 0, wl_fixed_from_int(display_width), wl_fixed_from_int(display_height));
    surface.add_frame_callback([&committed](auto) { committed = true; });
    wl_surface_commit(surface);

    client.dispatch_until([&committed]() { return committed;} );

    ASSERT_TRUE(surface_has_size(client, surface, display_width, display_height));

    // Now, set the source viewport to all -1, and expect that we go back to un-viewported size
    committed = false;
    wp_viewport_set_source(viewport, wl_fixed_from_int(-1), wl_fixed_from_int(-1), wl_fixed_from_int(-1), wl_fixed_from_int(-1));
    surface.add_frame_callback([&committed](auto) { committed = true; });
    wl_surface_commit(surface);

    client.dispatch_until([&committed]() { return committed;} );

    EXPECT_TRUE(surface_has_size(client, surface, buffer_width, buffer_height));
}

class WpViewporterDestParamsTest : public WpViewporterTest, public testing::WithParamInterface<std::tuple<int32_t, int32_t, char const*>>
{
};

TEST_P(WpViewporterDestParamsTest, raises_protocol_error_on_invalid_value)
{
    wlcs::Client client{the_server()};

    auto surface = client.create_visible_surface(200, 100);

    auto viewporter = client.bind_if_supported<wp_viewporter>(wlcs::AnyVersion);
    auto viewport = wrap_wl_object(wp_viewporter_get_viewport(viewporter, surface));

    auto const [width, height, _] = GetParam();

    EXPECT_PROTOCOL_ERROR(
        {
            wp_viewport_set_destination(viewport, width, height);
            client.roundtrip();
        },
        &wp_viewport_interface,
        WP_VIEWPORT_ERROR_BAD_VALUE
    );
}

INSTANTIATE_TEST_SUITE_P(
    ,
    WpViewporterDestParamsTest,
    Values(
        std::make_tuple(-1, 0, "width_must_be_non_negative"),
        std::make_tuple(0, -1, "height_must_be_non_negative"),
        std::make_tuple(1, 0, "height_must_be_positive"),
        std::make_tuple(0, 1, "width_must_be_positive")
    ),
    [](testing::TestParamInfo<WpViewporterDestParamsTest::ParamType> const& info) -> std::string
    {
        return std::get<2>(info.param);
    });

TEST_F(WpViewporterTest, all_minus_one_destination_unsets_destination_viewport)
{
    wlcs::Client client{the_server()};

    int const buffer_width{640}, buffer_height{480}, display_width{320}, display_height{200};

    auto surface = client.create_visible_surface(buffer_width, buffer_height);
    auto buffer = ShmBuffer(client, buffer_width, buffer_height);

    auto viewporter = client.bind_if_supported<wp_viewporter>(wlcs::AnyVersion);
    auto viewport = wrap_wl_object(wp_viewporter_get_viewport(viewporter, surface));

    bool committed = false;

    // First set the destination viewport, and assert that we get the right surface size...
    wp_viewport_set_destination(viewport, display_width, display_height);
    surface.add_frame_callback([&committed](auto) { committed = true; });
    wl_surface_commit(surface);

    client.dispatch_until([&committed]() { return committed;} );

    ASSERT_TRUE(surface_has_size(client, surface, display_width, display_height));

    // Now, set the source viewport to all -1, and expect that we go back to un-viewported size
    committed = false;
    wp_viewport_set_destination(viewport, -1, -1);
    surface.add_frame_callback([&committed](auto) { committed = true; });
    wl_surface_commit(surface);

    client.dispatch_until([&committed]() { return committed;} );

    EXPECT_TRUE(surface_has_size(client, surface, buffer_width, buffer_height));
}
