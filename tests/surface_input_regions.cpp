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
#include <experimental/optional>

using namespace testing;

enum class RegionAction
{
    add,
    subtract,
};

struct Region
{
    struct Element
    {
        RegionAction action;
        std::pair<int, int> top_left;
        std::pair<int, int> size;
    };

    std::string name;
    std::pair<int, int> surface_size;
    std::vector<Element> elements;

    void apply_to_surface(wlcs::Client& client, wl_surface* surface) const
    {
        if (elements.empty())
            return;
        auto const wl_region = wl_compositor_create_region(client.compositor());
        for (auto const& e: elements)
        {
            switch(e.action)
            {
            case RegionAction::add:
                wl_region_add(wl_region, e.top_left.first, e.top_left.second, e.size.first, e.size.second);
                break;
            case RegionAction::subtract:
                wl_region_subtract(wl_region, e.top_left.first, e.top_left.second, e.size.first, e.size.second);
                break;
            }
        }
        wl_surface_set_input_region(surface, wl_region);
        wl_region_destroy(wl_region);
        wl_surface_commit(surface);
        client.roundtrip();
    }
};

struct RegionWithTestPoints
{
    RegionWithTestPoints()
        : name{"default constructed"},
          region{"default constructed", {0, 0}, {}},
          on_surface{0, 0},
          off_surface{0, 0}
    {
    }

    RegionWithTestPoints(
        std::string const& name,
        Region region,
        std::pair<int, int> on_surface,
        std::pair<int, int> delta)
        : name{name},
          region{region},
          on_surface{on_surface},
          off_surface{on_surface.first + delta.first, on_surface.second + delta.second}
    {
    }

    std::string name;
    Region region;
    std::pair<int, int> on_surface;
    std::pair<int, int> off_surface;
};

std::ostream& operator<<(std::ostream& out, RegionWithTestPoints const& param)
{
    return out << param.region.name << " " << param.name;
}

struct SurfaceBuilder
{
    SurfaceBuilder(std::string const& name) : name{name} {}
    virtual ~SurfaceBuilder() = default;

    virtual auto build(
        wlcs::Server& server,
        wlcs::Client& client,
        std::pair<int, int> position,
        std::pair<int, int> size) const -> std::unique_ptr<wlcs::Surface> = 0;

    std::string const name;
};

std::ostream& operator<<(std::ostream& out, std::shared_ptr<SurfaceBuilder> const& param)
{
    return out << param->name;
}

struct WlShellSurfaceBuilder : SurfaceBuilder
{
    WlShellSurfaceBuilder() : SurfaceBuilder{"wl_shell_surface"} {}

    auto build(
        wlcs::Server& server,
        wlcs::Client& client,
        std::pair<int, int> position,
        std::pair<int, int> size) const -> std::unique_ptr<wlcs::Surface> override
    {
        auto surface = std::make_unique<wlcs::Surface>(
            client.create_wl_shell_surface(size.first, size.second));
        server.move_surface_to(*surface, position.first, position.second);
        return surface;
    }
};

struct XdgV6SurfaceBuilder : SurfaceBuilder
{
    XdgV6SurfaceBuilder() : SurfaceBuilder{"zxdg_surface_v6"} {}

    auto build(
        wlcs::Server& server,
        wlcs::Client& client,
        std::pair<int, int> position,
        std::pair<int, int> size) const -> std::unique_ptr<wlcs::Surface> override
    {
        auto surface = std::make_unique<wlcs::Surface>(
            client.create_xdg_shell_v6_surface(size.first, size.second));
        server.move_surface_to(*surface, position.first, position.second);
        return surface;
    }
};

struct XdgStableSurfaceBuilder : SurfaceBuilder
{
    XdgStableSurfaceBuilder() : SurfaceBuilder{"xdg_surface (stable)"} {}

    auto build(
        wlcs::Server& server,
        wlcs::Client& client,
        std::pair<int, int> position,
        std::pair<int, int> size) const -> std::unique_ptr<wlcs::Surface> override
    {
        auto surface = std::make_unique<wlcs::Surface>(
            client.create_xdg_shell_stable_surface(size.first, size.second));
        server.move_surface_to(*surface, position.first, position.second);
        return surface;
    }
};

struct SubsurfaceBuilder : SurfaceBuilder
{
    SubsurfaceBuilder(std::pair<int, int> offset)
        : SurfaceBuilder{
            "subsurface (offset " +
            std::to_string(offset.first) +
            ", " +
            std::to_string(offset.second) +
            ")"},
          offset{offset}
    {
    }

    auto build(
        wlcs::Server& server,
        wlcs::Client& client,
        std::pair<int, int> position,
        std::pair<int, int> size) const -> std::unique_ptr<wlcs::Surface> override
    {
        auto main_surface = std::make_shared<wlcs::Surface>(
            client.create_visible_surface(80, 50));
        server.move_surface_to(
            *main_surface,
            position.first - offset.first,
            position.second - offset.second);
        client.run_on_destruction(
            [main_surface]() mutable
            {
                main_surface.reset();
            });
        auto subsurface = std::make_unique<wlcs::Subsurface>(wlcs::Subsurface::create_visible(
            *main_surface,
            offset.first, offset.second,
            size.first, size.second));
        // if subsurface is sync, tests would have to commit the parent to modify it
        // this is inconvenient to do in a generic way, so we make it desync
        wl_subsurface_set_desync(*subsurface);
        return subsurface;
    }

    std::pair<int, int> offset;
};

auto const all_surface_types = Values(
    std::make_shared<WlShellSurfaceBuilder>(),
    std::make_shared<XdgV6SurfaceBuilder>(),
    std::make_shared<XdgStableSurfaceBuilder>(),
    std::make_shared<SubsurfaceBuilder>(std::make_pair(0, 0)),
    std::make_shared<SubsurfaceBuilder>(std::make_pair(7, 12)));

// TODO: popup surfaces

struct InputType
{
    InputType(std::string const& name) : name{name} {}
    virtual ~InputType() = default;

    struct Device
    {
        virtual ~Device() = default;
        virtual void to_position(std::pair<int, int> position) = 0;
    };

    virtual auto create_device(wlcs::Server& server) -> std::unique_ptr<Device> = 0;
    virtual auto current_surface(wlcs::Client const& client) -> wl_surface* = 0;
    virtual auto position_on_surface(wlcs::Client const& client) -> std::pair<int, int> = 0;

    std::string const name;
};

std::ostream& operator<<(std::ostream& out, std::shared_ptr<InputType> const& param)
{
    return out << param->name;
}

struct PointerInput : InputType
{
    PointerInput() : InputType{"pointer"} {}

    struct Pointer : Device
    {
        Pointer(wlcs::Server& server)
            : pointer{server.create_pointer()}
        {
        }

        void to_position(std::pair<int, int> position) override
        {
            pointer.move_to(position.first, position.second);
        }

        wlcs::Pointer pointer;
    };

    auto create_device(wlcs::Server& server) -> std::unique_ptr<Device> override
    {
        return std::make_unique<Pointer>(server);
    }

    auto current_surface(wlcs::Client const& client) -> wl_surface* override
    {
        return client.window_under_cursor();
    }

    auto position_on_surface(wlcs::Client const& client) -> std::pair<int, int> override
    {
        auto const wl_fixed_position = client.pointer_position();
        return {
            wl_fixed_to_int(wl_fixed_position.first),
            wl_fixed_to_int(wl_fixed_position.second)};
    }
};

struct TouchInput : InputType
{
    TouchInput() : InputType{"touch"} {}

    struct Touch : Device
    {
        Touch(wlcs::Server& server)
            : touch{server.create_touch()}
        {
        }

        void to_position(std::pair<int, int> position) override
        {
            if (is_down)
            {
                touch.up();
            }

            touch.down_at(position.first, position.second);
            is_down = true;
        }

        wlcs::Touch touch;
        bool is_down = false;
    };

    auto create_device(wlcs::Server& server) -> std::unique_ptr<Device> override
    {
        return std::make_unique<Touch>(server);
    }

    auto current_surface(wlcs::Client const& client) -> wl_surface* override
    {
        return client.touched_window();
    }

    auto position_on_surface(wlcs::Client const& client) -> std::pair<int, int> override
    {
        auto const wl_fixed_position = client.touch_position();
        return {
            wl_fixed_to_int(wl_fixed_position.first),
            wl_fixed_to_int(wl_fixed_position.second)};
    }
};

auto const all_input_types = Values(
    std::make_shared<PointerInput>(),
    std::make_shared<TouchInput>());

class RegionSurfaceInputCombinations :
    public wlcs::InProcessServer,
    public testing::WithParamInterface<std::tuple<
        RegionWithTestPoints,
        std::shared_ptr<SurfaceBuilder>,
        std::shared_ptr<InputType>>>
{
};

auto const surface_size{std::make_pair(215, 108)};

Region const default_region{"default", surface_size, {}};

auto const default_edges = Values(
    RegionWithTestPoints{"left edge", default_region,
        {0, surface_size.second / 2},
        {-1, 0}},
    RegionWithTestPoints{"bottom edge", default_region,
        {surface_size.first / 2, surface_size.second - 1},
        {0, 1}},
    RegionWithTestPoints{"right edge", default_region,
        {surface_size.first - 1, surface_size.second / 2},
        {1, 0}},
    RegionWithTestPoints{"top edge", default_region,
        {surface_size.first / 2, 0},
        {0, -1}});

Region const full_surface_region{"explicitly specified full surface", surface_size, {
    {RegionAction::add, {0, 0}, surface_size}}};

auto const full_surface_edges = Values(
    RegionWithTestPoints{"left edge", full_surface_region,
        {0, surface_size.second / 2},
        {-1, 0}},
    RegionWithTestPoints{"bottom edge", full_surface_region,
        {surface_size.first / 2, surface_size.second - 1},
        {0, 1}},
    RegionWithTestPoints{"right edge", full_surface_region,
        {surface_size.first - 1, surface_size.second / 2},
        {1, 0}},
    RegionWithTestPoints{"top edge", full_surface_region,
        {surface_size.first / 2, 0},
        {0, -1}});

auto const region_inset = std::make_pair(12, 17);

Region const smaller_region{"smaller", surface_size, {
    {RegionAction::add, region_inset, {
        surface_size.first - region_inset.first * 2,
        surface_size.second - region_inset.second * 2}}}};

auto const smaller_region_edges = Values(
    RegionWithTestPoints{"left edge", smaller_region,
        {region_inset.first, surface_size.second / 2},
        {-1, 0}},
    RegionWithTestPoints{"bottom edge", smaller_region,
        {surface_size.first / 2, surface_size.second - region_inset.second - 1},
        {0, 1}},
    RegionWithTestPoints{"right edge", smaller_region,
        {surface_size.first - region_inset.first - 1, surface_size.second / 2},
        {1, 0}},
    RegionWithTestPoints{"top edge", smaller_region,
        {surface_size.first / 2, region_inset.second},
        {0, -1}});

// If a region is larger then the surface it should be clipped

auto const region_outset = std::make_pair(12, 17);

Region const larger_region{"larger", surface_size, {
    {RegionAction::add, {-region_outset.first, -region_outset.second}, {
        surface_size.first + region_inset.first * 2,
        surface_size.second + region_inset.second * 2}}}};

auto const larger_region_edges = Values(
    RegionWithTestPoints{"left edge", larger_region,
        {0, surface_size.second / 2},
        {-1, 0}},
    RegionWithTestPoints{"bottom edge", larger_region,
        {surface_size.first / 2, surface_size.second - 1},
        {0, 1}},
    RegionWithTestPoints{"right edge", larger_region,
        {surface_size.first - 1, surface_size.second / 2},
        {1, 0}},
    RegionWithTestPoints{"top edge", larger_region,
        {surface_size.first / 2, 0},
        {0, -1}});

int const small_rect_inset = 16;

// Looks something like this:
// (dotted line is real surface, solid line is input region rectangles)
//   _______A_______
//  |               |
// B|               |C
//  |_D___________E_|
//  :   |       |   :
//  :  F|       |G  :
//  '---|---H---|---'
//      |_______|
//          I
Region const multi_rect_region{"multi-rect", surface_size, {
    // upper rect
    {RegionAction::add,
        {0, 0},
        {surface_size.first, surface_size.second / 2}},
    // lower rect
    {RegionAction::add,
        {small_rect_inset, surface_size.second / 2},
        {surface_size.first - small_rect_inset * 2, surface_size.second / 2 + 20}}}};

auto const multi_rect_edges = Values(
    RegionWithTestPoints{"top region edge at surface top edge", multi_rect_region, // A in diagram
        {surface_size.first / 2, 0},
        {0, -1}},
    RegionWithTestPoints{"right region edge at surface right edge", multi_rect_region, // C in diagram
        {surface_size.first - 1, surface_size.second / 4},
        {1, 0}},
    RegionWithTestPoints{"left region edge inside surface", multi_rect_region, // F in diagram
        {small_rect_inset, surface_size.second * 3 / 4},
        {-1, 0}},
    RegionWithTestPoints{"step edge", multi_rect_region,  // D in diagram
        {small_rect_inset / 2, surface_size.second / 2 - 1},
        {0, 1}},
    RegionWithTestPoints{"bottom clipped edge", multi_rect_region,  // I in diagram
        {surface_size.first / 2, surface_size.second - 1},
        {0, 1}});

auto const multi_rect_corners = Values(
    RegionWithTestPoints{
        "top-left corner", multi_rect_region, // AxB in diagram
        {0, 0},
        {-1, -1}},
    RegionWithTestPoints{
        "top-right corner", multi_rect_region, // AxC in diagram
        {surface_size.first - 1, 0},
        {1, -1}},
    RegionWithTestPoints{
        "bottom-left corner", multi_rect_region, // HxF in diagram
        {small_rect_inset, surface_size.second - 1},
        {-1, 1}},
    RegionWithTestPoints{
        "bottom-right corner", multi_rect_region, // HxG in diagram
        {surface_size.first - small_rect_inset - 1, surface_size.second - 1},
        {1, 1}},
    RegionWithTestPoints{
        "left interior corner", multi_rect_region, // HxF in diagram
        {small_rect_inset, surface_size.second / 2 - 1},
        {-1, 1}},
    RegionWithTestPoints{
        "right interior corner", multi_rect_region, // HxG in diagram
        {surface_size.first - small_rect_inset - 1, surface_size.second / 2 - 1},
        {1, 1}});

// TODO: test subtract
// TODO: test empty region
// TODO: test default region

TEST_P(RegionSurfaceInputCombinations, input_inside_region_seen)
{
    auto const top_left = std::make_pair(64, 49);
    wlcs::Client client{the_server()};
    RegionWithTestPoints region;
    std::shared_ptr<SurfaceBuilder> builder;
    std::shared_ptr<InputType> input;

    std::tie(region, builder, input) = GetParam();

    auto surface = builder->build(
        the_server(),
        client,
        top_left,
        region.region.surface_size);
    region.region.apply_to_surface(client, *surface);
    struct wl_surface* const wl_surface = *surface;

    auto const device = input->create_device(the_server());
    device->to_position({
        top_left.first + region.on_surface.first,
        top_left.second + region.on_surface.second});
    client.roundtrip();

    EXPECT_THAT(input->current_surface(client), Eq(wl_surface))
        << input << " not seen by "<< builder << " when inside " << region.name << " of " << region.region.name << " region";

    if (input->current_surface(client) == wl_surface)
    {
        EXPECT_THAT(input->position_on_surface(client), Eq(region.on_surface))
            << input << " in the wrong place over " << builder << " while testing " << region;
    }
}

TEST_P(RegionSurfaceInputCombinations, input_not_seen_after_leaving_region)
{
    auto const top_left = std::make_pair(64, 49);
    wlcs::Client client{the_server()};
    RegionWithTestPoints region;
    std::shared_ptr<SurfaceBuilder> builder;
    std::shared_ptr<InputType> input;
    std::tie(region, builder, input) = GetParam();

    auto surface = builder->build(
        the_server(),
        client,
        top_left,
        region.region.surface_size);
    region.region.apply_to_surface(client, *surface);
    struct wl_surface* const wl_surface = *surface;

    auto const device = input->create_device(the_server());
    device->to_position({
        top_left.first + region.on_surface.first,
        top_left.second + region.on_surface.second});
    client.roundtrip();
    device->to_position({
        top_left.first + region.off_surface.first,
        top_left.second + region.off_surface.second});
    client.roundtrip();

    EXPECT_THAT(input->current_surface(client), Ne(wl_surface))
        << input << " seen by " << builder << " when outside " << region.name << " of " << region.region.name << " region";
}

class SurfaceInputCombinations :
    public wlcs::InProcessServer,
    public testing::WithParamInterface<std::tuple<
        std::shared_ptr<SurfaceBuilder>,
        std::shared_ptr<InputType>>>
{
};

TEST_P(SurfaceInputCombinations, input_not_seen_in_region_after_null_buffer_committed)
{
    auto const top_left = std::make_pair(64, 49);
    wlcs::Client client{the_server()};
    std::shared_ptr<SurfaceBuilder> builder;
    std::shared_ptr<InputType> input;
    std::tie(builder, input) = GetParam();

    auto const region = full_surface_region;

    auto surface = builder->build(
        the_server(),
        client,
        top_left,
        region.surface_size);
    region.apply_to_surface(client, *surface);
    struct wl_surface* const wl_surface = *surface;
    wl_surface_attach(wl_surface, nullptr, 0, 0);
    wl_surface_commit(wl_surface);
    client.roundtrip();

    auto const device = input->create_device(the_server());
    device->to_position(top_left);
    client.roundtrip();

    EXPECT_THAT(input->current_surface(client), Ne(wl_surface))
        << input << " seen by " << builder << " after null buffer committed";
}

TEST_P(SurfaceInputCombinations, input_not_seen_in_surface_without_region_after_null_buffer_committed)
{
    auto const top_left = std::make_pair(64, 49);
    wlcs::Client client{the_server()};
    std::shared_ptr<SurfaceBuilder> builder;
    std::shared_ptr<InputType> input;
    std::tie(builder, input) = GetParam();

    auto surface = builder->build(
        the_server(),
        client,
        top_left,
        surface_size);
    struct wl_surface* const wl_surface = *surface;
    wl_surface_attach(wl_surface, nullptr, 0, 0);
    wl_surface_commit(wl_surface);
    client.roundtrip();

    auto const device = input->create_device(the_server());
    device->to_position(top_left);
    client.roundtrip();

    EXPECT_THAT(input->current_surface(client), Ne(wl_surface))
        << input << " seen by " << builder << " after null buffer committed";
}

TEST_P(SurfaceInputCombinations, input_not_seen_over_empty_region)
{
    auto const top_left = std::make_pair(64, 49);
    wlcs::Client client{the_server()};
    std::shared_ptr<SurfaceBuilder> builder;
    std::shared_ptr<InputType> input;
    std::tie(builder, input) = GetParam();

    auto surface = builder->build(
        the_server(),
        client,
        top_left,
        surface_size);
    struct wl_surface* const wl_surface = *surface;

    auto const wl_region = wl_compositor_create_region(client.compositor());
    wl_surface_set_input_region(*surface, wl_region);
    wl_region_destroy(wl_region);
    wl_surface_commit(*surface);
    client.roundtrip();

    auto const device = input->create_device(the_server());
    device->to_position({top_left.first + 4, top_left.second +4});
    client.roundtrip();

    EXPECT_THAT(input->current_surface(client), Ne(wl_surface))
        << input << " seen by " << builder << " with empty input region";
}

// There's way too many region edges/surface type/input device combinations, so we don't run all of them
// multi_rect_edges covers most cases, so we test it against all surface type/input device combinations
// We test the rest against just XDG toplevel

INSTANTIATE_TEST_SUITE_P(
    MultiRectEdges,
    RegionSurfaceInputCombinations,
    Combine(multi_rect_edges, all_surface_types, all_input_types));

INSTANTIATE_TEST_SUITE_P(
    DefaultEdges,
    RegionSurfaceInputCombinations,
    Combine(default_edges, all_surface_types, all_input_types));

INSTANTIATE_TEST_SUITE_P(
    FullSurface,
    RegionSurfaceInputCombinations,
    Combine(full_surface_edges, Values(std::make_shared<XdgStableSurfaceBuilder>()), all_input_types));

INSTANTIATE_TEST_SUITE_P(
    SmallerRegion,
    RegionSurfaceInputCombinations,
    Combine(smaller_region_edges, Values(std::make_shared<XdgStableSurfaceBuilder>()), all_input_types));

INSTANTIATE_TEST_SUITE_P(
    ClippedLargerRegion,
    RegionSurfaceInputCombinations,
    Combine(larger_region_edges, Values(std::make_shared<XdgStableSurfaceBuilder>()), all_input_types));

INSTANTIATE_TEST_SUITE_P(
    MultiRectCorners,
    RegionSurfaceInputCombinations,
    Combine(multi_rect_corners, Values(std::make_shared<XdgStableSurfaceBuilder>()), all_input_types));

INSTANTIATE_TEST_SUITE_P(
    SurfaceInputRegions,
    SurfaceInputCombinations,
    Combine(all_surface_types, all_input_types));
