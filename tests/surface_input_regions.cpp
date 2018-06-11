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

// No tests use subtract as that is not yet implemented in Mir

enum class RegionAction
{
    add,
    subtract,
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

class SurfaceInputRegion :
    public wlcs::InProcessServer,
    public testing::WithParamInterface<RegionAndMotion>
{
};

TEST_P(SurfaceInputRegion, pointer_enter_leave_surface)
{
    using namespace testing;

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
        case RegionAction::add:
            wl_region_add(wl_region, e.x, e.y, e.width, e.height);
            break;
        case RegionAction::subtract:
            wl_region_subtract(wl_region, e.x, e.y, e.width, e.height);
            break;
        }
    }
    wl_surface_set_input_region(wl_surface, wl_region);
    wl_region_destroy(wl_region);
    wl_surface_commit(wl_surface);
    client.roundtrip();

    auto pointer = the_server().create_pointer();
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

TEST_P(SurfaceInputRegion, touch_enter_leave_surface)
{
    using namespace testing;

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
        case RegionAction::add:
            wl_region_add(wl_region, e.x, e.y, e.width, e.height);
            break;
        case RegionAction::subtract:
            wl_region_subtract(wl_region, e.x, e.y, e.width, e.height);
            break;
        }
    }
    wl_surface_set_input_region(wl_surface, wl_region);
    wl_region_destroy(wl_region);
    wl_surface_commit(wl_surface);
    client.roundtrip();

    auto touch = the_server().create_touch();
    int touch_x = top_left_x + params.initial_x;
    int touch_y = top_left_y + params.initial_y;

    touch.down_at(touch_x, touch_y);
    client.roundtrip();
    EXPECT_THAT(client.touched_window(), Ne(wl_surface));
    touch.up();

    touch.down_at(touch_x + params.dx, touch_y + params.dy);
    client.roundtrip();
    EXPECT_THAT(client.touched_window(), Eq(wl_surface));
    EXPECT_THAT(client.touch_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(params.initial_x + params.dx),
                    wl_fixed_from_int(params.initial_y + params.dy))));
    touch.up();

    touch.down_at(touch_x, touch_y);
    client.roundtrip();
    EXPECT_THAT(client.touched_window(), Ne(wl_surface));
    touch.up();

    client.roundtrip();
}

TEST_P(SurfaceInputRegion, pointer_enter_leave_subsurface)
{
    using namespace testing;

    wlcs::Client client{the_server()};

    auto const params = GetParam();

    auto main_surface = client.create_visible_surface(
        params.window_width + 20,
        params.window_height + 20);

    int const subsurface_offset_x = 10, subsurface_offset_y = 10;

    auto surface = wlcs::Subsurface::create_visible(
        main_surface,
        subsurface_offset_x,
        subsurface_offset_y,
        params.window_width,
        params.window_height);

    int const top_left_x = 65, top_left_y = 200;
    the_server().move_surface_to(main_surface, top_left_x, top_left_y);

    auto const wl_surface = static_cast<struct wl_surface*>(surface);

    auto const wl_region = wl_compositor_create_region(client.compositor());
    for (auto const& e: params.region.elements)
    {
        switch(e.action)
        {
        case RegionAction::add:
            wl_region_add(wl_region, e.x, e.y, e.width, e.height);
            break;
        case RegionAction::subtract:
            wl_region_subtract(wl_region, e.x, e.y, e.width, e.height);
            break;
        }
    }
    wl_surface_set_input_region(wl_surface, wl_region);
    wl_region_destroy(wl_region);
    wl_surface_commit(wl_surface);
    wl_surface_commit(main_surface);
    client.roundtrip();

    auto pointer = the_server().create_pointer();
    pointer.move_to(top_left_x + params.initial_x + subsurface_offset_x, top_left_y + params.initial_y + subsurface_offset_y);

    client.roundtrip();

    EXPECT_THAT(client.focused_window(), Ne(wl_surface));

    /* move pointer; it should now be inside the surface */
    pointer.move_by(params.dx, params.dy);

    client.roundtrip();

    EXPECT_THAT(client.focused_window(), Ne((struct wl_surface*)main_surface))
        << "main surface focused instead of subsurface";
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
    {RegionAction::add, 0, 0, RegionAndMotion::window_width, RegionAndMotion::window_height}}};

INSTANTIATE_TEST_CASE_P(
    NormalRegion,
    SurfaceInputRegion,
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

int const region_inset_x = 12;
int const region_inset_y = 17;

InputRegion const smaller_region{"smaller", {{
    RegionAction::add,
    region_inset_x,
    region_inset_y,
    RegionAndMotion::window_width - region_inset_x * 2,
    RegionAndMotion::window_height - region_inset_y * 2}}};

INSTANTIATE_TEST_CASE_P(
    SmallerRegion,
    SurfaceInputRegion,
    testing::Values(
        RegionAndMotion{
            "Centre-left", smaller_region,
            region_inset_x - 1, RegionAndMotion::window_height / 2,
            1, 0},
        RegionAndMotion{
            "Bottom-centre", smaller_region,
            RegionAndMotion::window_width / 2, RegionAndMotion::window_height - region_inset_y,
            0, -1},
        RegionAndMotion{
            "Centre-right", smaller_region,
            RegionAndMotion::window_width - region_inset_x, RegionAndMotion::window_height / 2,
            -1, 0},
        RegionAndMotion{
            "Top-centre", smaller_region,
            RegionAndMotion::window_width / 2, region_inset_y - 1,
            0, 1}
    ));

// If a region is larger then the surface it should be clipped

int const region_outset_x = 12;
int const region_outset_y = 17;

InputRegion const larger_region{"larger", {{
    RegionAction::add,
    - region_outset_x,
    - region_outset_y,
    RegionAndMotion::window_width + region_outset_x * 2,
    RegionAndMotion::window_height + region_outset_y * 2}}};

INSTANTIATE_TEST_CASE_P(
    ClippedLargerRegion,
    SurfaceInputRegion,
    testing::Values(
        RegionAndMotion{
            "Centre-left", larger_region,
            -1, RegionAndMotion::window_height / 2,
            10, 0},
        RegionAndMotion{
            "Bottom-centre", larger_region,
            RegionAndMotion::window_width / 2, RegionAndMotion::window_height,
            0, -1},
        RegionAndMotion{
            "Centre-right", larger_region,
            RegionAndMotion::window_width, RegionAndMotion::window_height / 2,
            -1, 0},
        RegionAndMotion{
            "Top-centre", larger_region,
            RegionAndMotion::window_width / 2, -1,
            0, 1}
    ));

int const small_rect_inset = 16;

InputRegion const multi_rect_region{"multi rect", {
    {RegionAction::add, 0, 0,
     RegionAndMotion::window_width, RegionAndMotion::window_height / 2},
    {RegionAction::add, small_rect_inset, RegionAndMotion::window_height / 2,
     RegionAndMotion::window_width - small_rect_inset * 2, RegionAndMotion::window_height / 2}}};

INSTANTIATE_TEST_CASE_P(
    MultiRectRegionEdges,
    SurfaceInputRegion,
    testing::Values(
        RegionAndMotion{
            "Top-left-edge", multi_rect_region,
            -1, RegionAndMotion::window_height / 4,
            1, 0},
        RegionAndMotion{
            "Top-right-edge", multi_rect_region,
            RegionAndMotion::window_width, RegionAndMotion::window_height / 4,
            -1, 0},
        RegionAndMotion{
            "Bottom-left-edge", multi_rect_region,
            small_rect_inset - 1, RegionAndMotion::window_height * 3 / 4,
            1, 0},
        RegionAndMotion{
            "Bottom-right-edge", multi_rect_region,
            RegionAndMotion::window_width - small_rect_inset, RegionAndMotion::window_height * 3 / 4,
            -1, 0},
        RegionAndMotion{
            "Step-bottom-edge", multi_rect_region,
            small_rect_inset / 2, RegionAndMotion::window_height / 2,
            0, -1}
    ));

INSTANTIATE_TEST_CASE_P(
    MultiRectRegionCorners,
    SurfaceInputRegion,
    testing::Values(
        RegionAndMotion{
            "Top-left", multi_rect_region,
            -1, -1,
            1, 1},
        RegionAndMotion{
            "Bottom-left", multi_rect_region,
            small_rect_inset -1, RegionAndMotion::window_height,
            1, -1},
        RegionAndMotion{
            "Bottom-right", multi_rect_region,
            RegionAndMotion::window_width - small_rect_inset, RegionAndMotion::window_height,
            -1, -1},
        RegionAndMotion{
            "Top-right", multi_rect_region,
            RegionAndMotion::window_width, -1,
            -1, 1},
        RegionAndMotion{
            "Step-corner-left", multi_rect_region,
            -1, RegionAndMotion::window_height / 2,
            1, -1},
        RegionAndMotion{
            "Step-corner-right", multi_rect_region,
            RegionAndMotion::window_width, RegionAndMotion::window_height / 2,
            -1, -1},
        RegionAndMotion{
            "Step-inside-horiz-left", multi_rect_region,
            small_rect_inset - 1, RegionAndMotion::window_height / 2,
            1, 0},
        RegionAndMotion{
            "Step-inside-horiz-right", multi_rect_region,
            RegionAndMotion::window_width - small_rect_inset, RegionAndMotion::window_height / 2,
            -1, 0},
        RegionAndMotion{
            "Step-inside-diag-left", multi_rect_region,
            small_rect_inset - 1, RegionAndMotion::window_height / 2,
            1, -1},
        RegionAndMotion{
            "Step-inside-diag-right", multi_rect_region,
            RegionAndMotion::window_width - small_rect_inset, RegionAndMotion::window_height / 2,
            -1, -1}
    ));

// TODO: surface with empty input region

// TODO: input on subsurface

// TODO: XDG shell and wl_shell

// TODO: touch input on surface

// TODO: touch input on subsurface
