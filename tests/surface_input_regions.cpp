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
 * Authored by: William Wold <william.wold@canonical.com>
 */

#include "helpers.h"
#include "in_process_server.h"

#include <gmock/gmock.h>

#include <vector>
#include <memory>

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

    void apply_to_surface(wlcs::Client& client, wl_surface* surface) const
    {
        auto const wl_region = wl_compositor_create_region(client.compositor());
        for (auto const& e: elements)
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
        wl_surface_set_input_region(surface, wl_region);
        wl_region_destroy(wl_region);
        wl_surface_commit(surface);
        client.roundtrip();
    }
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

class SurfaceWithInputRegions :
    public wlcs::InProcessServer,
    public testing::WithParamInterface<RegionAndMotion>
{
};

TEST_P(SurfaceWithInputRegions, pointer_seen_entering_and_leaving_input_region)
{
    wlcs::Client client{the_server()};
    auto const params = GetParam();
    int const top_left_x = 64, top_left_y = 7;
    auto surface = client.create_visible_surface(
        params.window_width,
        params.window_height);
    the_server().move_surface_to(surface, top_left_x, top_left_y);
    auto const wl_surface = static_cast<struct wl_surface*>(surface);

    params.region.apply_to_surface(client, wl_surface);

    auto pointer = the_server().create_pointer();
    pointer.move_to(top_left_x + params.initial_x, top_left_y + params.initial_y);
    client.roundtrip();

    EXPECT_THAT(client.window_under_cursor(), Ne(wl_surface))
        << "pointer over surface when it should have been outside input region";

    pointer.move_by(params.dx, params.dy);
    client.roundtrip();

    ASSERT_THAT(client.window_under_cursor(), Eq(wl_surface))
        << "pointer not over surface when it should have been in input region";
    EXPECT_THAT(client.pointer_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(params.initial_x + params.dx),
                    wl_fixed_from_int(params.initial_y + params.dy))))
        << "pointer in the wrong place over surface";

    pointer.move_by(-params.dx, -params.dy);
    client.roundtrip();

    EXPECT_THAT(client.window_under_cursor(), Ne(wl_surface))
        << "pointer over surface when it should have been outside input region";
}

InputRegion const full_surface_region{"full surface", {
    {RegionAction::add, 0, 0, RegionAndMotion::window_width, RegionAndMotion::window_height}}};

INSTANTIATE_TEST_CASE_P(
    NormalRegion,
    SurfaceWithInputRegions,
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
    SurfaceWithInputRegions,
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
    SurfaceWithInputRegions,
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
     RegionAndMotion::window_width - small_rect_inset * 2, RegionAndMotion::window_height / 2 + 20}}};

INSTANTIATE_TEST_CASE_P(
    MultiRectRegionEdges,
    SurfaceWithInputRegions,
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
    SurfaceWithInputRegions,
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

struct SurfaceCreator
{
    using SurfaceMaker = std::function<std::unique_ptr<wlcs::Surface>(
        wlcs::InProcessServer& server, wlcs::Client& client,
        int x, int y, int width, int height,
        InputRegion const& input_region)>;

    std::string name;
    SurfaceMaker func;
};

struct SurfaceTypeWithRegionParams
{
    SurfaceCreator surface_creator;
    RegionAndMotion region_and_motion;
};

std::ostream& operator<<(std::ostream& out, SurfaceTypeWithRegionParams const& param)
{
    return out << "surface: " << param.surface_creator.name << ", " << param.region_and_motion;
}

class SurfaceTypesWithInputRegion :
    public wlcs::InProcessServer,
    public testing::WithParamInterface<SurfaceTypeWithRegionParams>
{
};

TEST_P(SurfaceTypesWithInputRegion, pointer_seen_entering_and_leaving_input_region)
{
    wlcs::Client client{the_server()};
    int const top_left_x = 64, top_left_y = 7;
    auto const& params = GetParam().region_and_motion;

    auto surface = GetParam().surface_creator.func(
        *this, client, top_left_x, top_left_y, params.window_width,
        params.window_height, params.region);
    auto const wl_surface = static_cast<struct wl_surface*>(*surface);

    auto pointer = the_server().create_pointer();
    pointer.move_to(top_left_x + params.initial_x, top_left_y + params.initial_y);
    client.roundtrip();

    EXPECT_THAT(client.window_under_cursor(), Ne(wl_surface))
        << "pointer over surface when it should have been outside input region";

    pointer.move_by(params.dx, params.dy);
    client.roundtrip();

    ASSERT_THAT(client.window_under_cursor(), Eq(wl_surface))
        << "pointer not over surface when it should have been in input region";
    EXPECT_THAT(client.pointer_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(params.initial_x + params.dx),
                    wl_fixed_from_int(params.initial_y + params.dy))))
        << "pointer in the wrong place over surface";

    pointer.move_by(-params.dx, -params.dy);
    client.roundtrip();

    EXPECT_THAT(client.window_under_cursor(), Ne(wl_surface))
        << "pointer over surface when it should have been outside input region";
}

TEST_P(SurfaceTypesWithInputRegion, touch_inside_input_region_is_seen)
{
    wlcs::Client client{the_server()};
    int const top_left_x = 64, top_left_y = 7;
    auto const& params = GetParam().region_and_motion;

    auto surface = GetParam().surface_creator.func(
        *this, client, top_left_x, top_left_y, params.window_width,
        params.window_height, params.region);
    auto const wl_surface = static_cast<struct wl_surface*>(*surface);

    auto touch = the_server().create_touch();
    int touch_x = top_left_x + params.initial_x;
    int touch_y = top_left_y + params.initial_y;

    touch.down_at(touch_x + params.dx, touch_y + params.dy);
    client.roundtrip();
    ASSERT_THAT(client.touched_window(), Eq(wl_surface))
        << "touch was not sent to surface when it should have been in input region";
    EXPECT_THAT(client.touch_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(params.initial_x + params.dx),
                    wl_fixed_from_int(params.initial_y + params.dy))))
        << "touch was not where it should have been";
    touch.up();
    client.roundtrip();
}

TEST_P(SurfaceTypesWithInputRegion, touch_outside_input_region_not_seen)
{
    wlcs::Client client{the_server()};
    int const top_left_x = 64, top_left_y = 7;
    auto const& params = GetParam().region_and_motion;

    auto surface = GetParam().surface_creator.func(
        *this, client, top_left_x, top_left_y, params.window_width,
        params.window_height, params.region);
    auto const wl_surface = static_cast<struct wl_surface*>(*surface);

    auto touch = the_server().create_touch();
    int touch_x = top_left_x + params.initial_x;
    int touch_y = top_left_y + params.initial_y;

    touch.down_at(touch_x, touch_y);
    client.roundtrip();
    EXPECT_THAT(client.touched_window(), Ne(wl_surface))
        << "touch was sent to surface when it should have been outside input region";
    touch.up();
    client.roundtrip();
}

auto region_param_values_for_surface_creator(SurfaceCreator creator) {
    return testing::Values(
        SurfaceTypeWithRegionParams{
            creator,
            RegionAndMotion{
                "bottom-outside-surface", multi_rect_region,
                RegionAndMotion::window_width / 2, RegionAndMotion::window_height,
                0, -1}},
        SurfaceTypeWithRegionParams{
            creator,
            RegionAndMotion{
                "Top-left-edge", multi_rect_region,
                -1, RegionAndMotion::window_height / 4,
                1, 0}},
        SurfaceTypeWithRegionParams{
            creator,
            RegionAndMotion{
                "Bottom-right-edge", multi_rect_region,
                RegionAndMotion::window_width - small_rect_inset, RegionAndMotion::window_height * 3 / 4,
                -1, 0}},
        SurfaceTypeWithRegionParams{
            creator,
            RegionAndMotion{
                "Step-bottom-edge", multi_rect_region,
                small_rect_inset / 2, RegionAndMotion::window_height / 2,
                0, -1}},
        SurfaceTypeWithRegionParams{
            creator,
            RegionAndMotion{
                "Step-corner-right", multi_rect_region,
                RegionAndMotion::window_width, RegionAndMotion::window_height / 2,
                -1, -1}},
        SurfaceTypeWithRegionParams{
            creator,
            RegionAndMotion{
                "Step-inside-horiz-right", multi_rect_region,
                RegionAndMotion::window_width - small_rect_inset, RegionAndMotion::window_height / 2,
                -1, 0}},
        SurfaceTypeWithRegionParams{
            creator,
            RegionAndMotion{
                "Step-inside-diag-left", multi_rect_region,
                small_rect_inset - 1, RegionAndMotion::window_height / 2,
                1, -1}});
}

INSTANTIATE_TEST_CASE_P(
    WlShellSurface,
    SurfaceTypesWithInputRegion,
    region_param_values_for_surface_creator(
        SurfaceCreator{
            "wl-shell-surface",
            [](wlcs::InProcessServer& server, wlcs::Client& client,
               int x, int y, int width, int height,
               InputRegion const& input_region) -> std::unique_ptr<wlcs::Surface>
            {
                auto surface = std::make_unique<wlcs::Surface>(client.create_wl_shell_surface(width, height));
                server.the_server().move_surface_to(*surface, x, y);
                input_region.apply_to_surface(client, *surface);
                return surface;
            }}));

INSTANTIATE_TEST_CASE_P(
    XdgShellV6Surface,
    SurfaceTypesWithInputRegion,
    region_param_values_for_surface_creator(
        SurfaceCreator{
            "xdg-shell-v6-surface",
            [](wlcs::InProcessServer& server, wlcs::Client& client,
               int x, int y, int width, int height,
               InputRegion const& input_region) -> std::unique_ptr<wlcs::Surface>
            {
                auto surface = std::make_unique<wlcs::Surface>(client.create_xdg_shell_v6_surface(width, height));
                server.the_server().move_surface_to(*surface, x, y);
                input_region.apply_to_surface(client, *surface);
                return surface;
            }}));

INSTANTIATE_TEST_CASE_P(
    XdgShellStableSurface,
    SurfaceTypesWithInputRegion,
    region_param_values_for_surface_creator(
        SurfaceCreator{
            "xdg-shell-stable-surface",
            [](wlcs::InProcessServer& server, wlcs::Client& client,
               int x, int y, int width, int height,
               InputRegion const& input_region) -> std::unique_ptr<wlcs::Surface>
            {
                auto surface = std::make_unique<wlcs::Surface>(client.create_xdg_shell_stable_surface(width, height));
                server.the_server().move_surface_to(*surface, x, y);
                input_region.apply_to_surface(client, *surface);
                return surface;
            }}));

INSTANTIATE_TEST_CASE_P(
    Subsurface,
    SurfaceTypesWithInputRegion,
    region_param_values_for_surface_creator(
        SurfaceCreator{
            "subsurface",
            [](wlcs::InProcessServer& server, wlcs::Client& client,
               int x, int y, int width, int height,
               InputRegion const& input_region) -> std::unique_ptr<wlcs::Surface>
            {
                auto main_surface = std::make_shared<wlcs::Surface>(client.create_visible_surface(width, height));
                server.the_server().move_surface_to(*main_surface, x, y);
                auto subsurface = std::make_unique<wlcs::Surface>(wlcs::Subsurface::create_visible(
                    *main_surface, 0, 0, width, height));
                client.run_on_destruction(
                    [main_surface]() mutable
                    {
                        main_surface.reset();
                    });
                input_region.apply_to_surface(client, *subsurface);
                wl_surface_commit(*main_surface);
                client.roundtrip();
                return subsurface;
            }}));

// TODO: surface with empty input region
