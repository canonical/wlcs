/*
 * Copyright © 2018 Canonical Ltd.
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
 * Authored by: William Wold <williamwold@canonical.com>
 */

#include "helpers.h"
#include "in_process_server.h"

#include <gmock/gmock.h>

#include <vector>

using namespace testing;

enum RegionAction
{
    ADD_RECT,
    SUBTRACT_RECT
};

struct InputRegion
{
    struct Element
    {
        RegionAction action;
        int x, y, width, height;
    };

    std::string name;
    std::vector<Element> elements;
};

struct RegionAndMotion
{
    static int constexpr window_width = 181;
    static int constexpr window_height = 208;
    std::string name;
    InputRegion region;
    int initial_x, initial_y; // Relative to surface top-left
    int dx, dy; // pointer motion
};

std::ostream& operator<<(std::ostream& out, RegionAndMotion const& param)
{
    return out << "region: " << param.region.name << ", pointer: " << param.name;
}

class InputRegionPointerEnterTest :
    public wlcs::InProcessServer,
    public testing::WithParamInterface<RegionAndMotion>
{
};

TEST_P(InputRegionPointerEnterTest, pointer_enter_and_leave_input_region)
{
    using namespace testing;

    auto pointer = the_server().create_pointer();

    wlcs::Client client{the_server()};

    auto const params = GetParam();

    auto surface = client.create_visible_surface(
        params.window_width,
        params.window_height);

    int const top_left_x = 64, top_left_y = 7;
    the_server().move_surface_to(surface, top_left_x, top_left_y);

    auto const wl_surface = static_cast<struct wl_surface*>(surface);

    auto const wl_region = wl_compositor_create_region(client.compositor());

    for (auto const& e: params.region.elements)
    {
        switch(e.action)
        {
        case ADD_RECT:
            wl_region_add(wl_region, e.x, e.y, e.width, e.height);
            break;
        case SUBTRACT_RECT:
            wl_region_subtract(wl_region, e.x, e.y, e.width, e.height);
            break;
        default:
            FAIL() << "unknown RegionAction";
        }
    }

    wl_surface_set_input_region(wl_surface, wl_region);

    wl_region_destroy(wl_region);

    wl_surface_commit(wl_surface);

    pointer.move_to(top_left_x + params.initial_x, top_left_y + params.initial_y);

    client.roundtrip();

    EXPECT_THAT(client.focused_window(), Ne(wl_surface));

    /* move pointer; it should now be inside the surface */
    pointer.move_by(params.dx, params.dy);

    client.roundtrip();

    EXPECT_THAT(client.focused_window(), Eq(wl_surface));
    EXPECT_THAT(client.pointer_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(params.initial_x + params.dx),
                    wl_fixed_from_int(params.initial_y + params.dy))));

    /* move pointer back; it should now be outside the surface */
    pointer.move_by(-params.dx, -params.dy);

    client.roundtrip();
    EXPECT_THAT(client.focused_window(), Ne(wl_surface));
}

InputRegion const full_surface_region{"full surface", {
    {ADD_RECT, 0, 0, RegionAndMotion::window_width, RegionAndMotion::window_height}}};

INSTANTIATE_TEST_CASE_P(
    PointerCrossingRegionCorner,
    InputRegionPointerEnterTest,
    testing::Values(
        RegionAndMotion{"Top-left", full_surface_region, -1, -1, 1, 1},
        RegionAndMotion{"Bottom-left", full_surface_region, -1, RegionAndMotion::window_height, 1, -1},
        RegionAndMotion{"Bottom-right", full_surface_region, RegionAndMotion::window_width, RegionAndMotion::window_height, -1, -1},
        RegionAndMotion{"Top-right", full_surface_region, RegionAndMotion::window_width, -1, -1, 1}
    ));

INSTANTIATE_TEST_CASE_P(
    PointerCrossingSurfaceEdge,
    InputRegionPointerEnterTest,
    testing::Values(
        RegionAndMotion{
            "Centre-left", full_surface_region,
            -1, RegionAndMotion::window_height / 2,
            1, 0},
        RegionAndMotion{
            "Bottom-centre", full_surface_region,
            RegionAndMotion::window_width / 2, RegionAndMotion::window_height,
            0, -1},
        RegionAndMotion{
            "Centre-right", full_surface_region,
            RegionAndMotion::window_width, RegionAndMotion::window_height / 2,
            -1, 0},
        RegionAndMotion{
            "Top-centre", full_surface_region,
            RegionAndMotion::window_width / 2, -1,
            0, 1}
    ));
