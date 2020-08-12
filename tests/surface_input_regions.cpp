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
#include "surface_builder.h"
#include "input_method.h"

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

auto const all_surface_types = ValuesIn(wlcs::SurfaceBuilder::all_surface_types());
auto const toplevel_surface_types = ValuesIn(wlcs::SurfaceBuilder::toplevel_surface_types());
auto const xdg_stable_surface_type = Values(
    std::static_pointer_cast<wlcs::SurfaceBuilder>(std::make_shared<wlcs::XdgStableSurfaceBuilder>()));

auto const all_input_types = ValuesIn(wlcs::InputMethod::all_input_methods());

class RegionSurfaceInputCombinations :
    public wlcs::InProcessServer,
    public testing::WithParamInterface<std::tuple<
        RegionWithTestPoints,
        std::shared_ptr<wlcs::SurfaceBuilder>,
        std::shared_ptr<wlcs::InputMethod>>>
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
    std::shared_ptr<wlcs::SurfaceBuilder> builder;
    std::shared_ptr<wlcs::InputMethod> input;

    std::tie(region, builder, input) = GetParam();

    auto surface = builder->build(
        the_server(),
        client,
        top_left,
        region.region.surface_size);
    region.region.apply_to_surface(client, *surface);
    struct wl_surface* const wl_surface = *surface;

    auto const device = input->create_device(the_server());
    device->move_to({
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
    std::shared_ptr<wlcs::SurfaceBuilder> builder;
    std::shared_ptr<wlcs::InputMethod> input;
    std::tie(region, builder, input) = GetParam();

    auto surface = builder->build(
        the_server(),
        client,
        top_left,
        region.region.surface_size);
    region.region.apply_to_surface(client, *surface);
    struct wl_surface* const wl_surface = *surface;

    auto const device = input->create_device(the_server());
    device->move_to({
        top_left.first + region.on_surface.first,
        top_left.second + region.on_surface.second});
    client.roundtrip();
    device->move_to({
        top_left.first + region.off_surface.first,
        top_left.second + region.off_surface.second});
    client.roundtrip();

    EXPECT_THAT(input->current_surface(client), Ne(wl_surface))
        << input << " seen by " << builder << " when outside " << region.name << " of " << region.region.name << " region";
}

class SurfaceInputCombinations :
    public wlcs::InProcessServer,
    public testing::WithParamInterface<std::tuple<
        std::shared_ptr<wlcs::SurfaceBuilder>,
        std::shared_ptr<wlcs::InputMethod>>>
{
};

TEST_P(SurfaceInputCombinations, input_not_seen_in_region_after_null_buffer_committed)
{
    auto const top_left = std::make_pair(64, 49);
    wlcs::Client client{the_server()};
    std::shared_ptr<wlcs::SurfaceBuilder> builder;
    std::shared_ptr<wlcs::InputMethod> input;
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
    device->move_to(top_left);
    client.roundtrip();

    EXPECT_THAT(input->current_surface(client), Ne(wl_surface))
        << input << " seen by " << builder << " after null buffer committed";
}

TEST_P(SurfaceInputCombinations, input_not_seen_in_surface_without_region_after_null_buffer_committed)
{
    auto const top_left = std::make_pair(64, 49);
    wlcs::Client client{the_server()};
    std::shared_ptr<wlcs::SurfaceBuilder> builder;
    std::shared_ptr<wlcs::InputMethod> input;
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
    device->move_to(top_left);
    client.roundtrip();

    EXPECT_THAT(input->current_surface(client), Ne(wl_surface))
        << input << " seen by " << builder << " after null buffer committed";
}

TEST_P(SurfaceInputCombinations, input_not_seen_over_empty_region)
{
    auto const top_left = std::make_pair(64, 49);
    wlcs::Client client{the_server()};
    std::shared_ptr<wlcs::SurfaceBuilder> builder;
    std::shared_ptr<wlcs::InputMethod> input;
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
    device->move_to({top_left.first + 4, top_left.second + 4});
    client.roundtrip();

    EXPECT_THAT(input->current_surface(client), Ne(wl_surface))
        << input << " seen by " << builder << " with empty input region";
}

TEST_P(SurfaceInputCombinations, input_hits_parent_after_falling_through_subsurface)
{
    auto const top_left = std::make_pair(64, 49);
    auto const input_offset = std::make_pair(4, 4);
    wlcs::Client client{the_server()};
    std::shared_ptr<wlcs::SurfaceBuilder> builder;
    std::shared_ptr<wlcs::InputMethod> input;
    std::tie(builder, input) = GetParam();

    auto parent = builder->build(
        the_server(),
        client,
        top_left,
        surface_size);
    struct wl_surface* const parent_wl_surface = *parent;
    auto subsurface = std::make_unique<wlcs::Subsurface>(wlcs::Subsurface::create_visible(
        *parent,
        0, 0,
        surface_size.first, surface_size.second));
    wl_subsurface_set_desync(*subsurface);
    struct wl_surface* const sub_wl_surface = *subsurface;

    Region const region{"region", surface_size, {{RegionAction::add, {0, 0}, {1, 1}}}};
    region.apply_to_surface(client, sub_wl_surface);

    auto const device = input->create_device(the_server());
    device->move_to({top_left.first + input_offset.first, top_left.second + input_offset.second});
    client.roundtrip();

    EXPECT_THAT(input->current_surface(client), Ne(sub_wl_surface))
        << input << " seen by subsurface when not over region";

    EXPECT_THAT(input->current_surface(client), Eq(parent_wl_surface))
        << input << " not seen by " << builder << " when it should have fallen through the subsurface input region";

    EXPECT_THAT(input->position_on_surface(client), Eq(input_offset))
        << input << " seen in the wrong place";
}

TEST_P(SurfaceInputCombinations, unmapping_parent_stops_subsurface_getting_input)
{
    auto const top_left = std::make_pair(64, 49);
    wlcs::Client client{the_server()};
    std::shared_ptr<wlcs::SurfaceBuilder> builder;
    std::shared_ptr<wlcs::InputMethod> input;
    std::tie(builder, input) = GetParam();

    auto parent = builder->build(
        the_server(),
        client,
        top_left,
        surface_size);
    struct wl_surface* const parent_wl_surface = *parent;
    auto subsurface = std::make_unique<wlcs::Subsurface>(wlcs::Subsurface::create_visible(
        *parent,
        0, 0,
        surface_size.first, surface_size.second));
    wl_subsurface_set_desync(*subsurface);
    struct wl_surface* const sub_wl_surface = *subsurface;
    client.roundtrip();

    wl_surface_attach(parent_wl_surface, nullptr, 0, 0);
    wl_surface_commit(parent_wl_surface);
    client.roundtrip();

    auto const device = input->create_device(the_server());
    device->move_to({top_left.first + 4, top_left.second + 4});
    client.roundtrip();

    EXPECT_THAT(input->current_surface(client), Ne(parent_wl_surface))
        << input << " seen by " << builder << " after it was unmapped";

    EXPECT_THAT(input->current_surface(client), Ne(sub_wl_surface))
        << input << " seen by subsurface after parent " << builder << " was unmapped";
}

TEST_P(SurfaceInputCombinations, input_falls_through_subsurface_when_unmapped)
{
    auto const top_left = std::make_pair(200, 49);
    wlcs::Client client{the_server()};
    std::shared_ptr<wlcs::SurfaceBuilder> builder;
    std::shared_ptr<wlcs::InputMethod> input;
    std::tie(builder, input) = GetParam();

    auto lower = client.create_visible_surface(surface_size.first, surface_size.second);
    the_server().move_surface_to(lower, top_left.first - 100, top_left.second);
    struct wl_surface* const lower_wl_surface = lower;

    auto parent = builder->build(
        the_server(),
        client,
        top_left,
        surface_size);
    ASSERT_THAT(surface_size.first, Gt(100));
    struct wl_surface* const parent_wl_surface = *parent;
    auto subsurface = std::make_unique<wlcs::Subsurface>(wlcs::Subsurface::create_visible(
        *parent,
        -100, 0,
        surface_size.first, surface_size.second));
    wl_subsurface_set_desync(*subsurface);
    struct wl_surface* const sub_wl_surface = *subsurface;
    client.roundtrip();

    wl_surface_attach(sub_wl_surface, nullptr, 0, 0);
    wl_surface_commit(sub_wl_surface);
    client.roundtrip();

    auto const device = input->create_device(the_server());
    device->move_to({top_left.first - 90, top_left.second + 10});
    client.roundtrip();

    EXPECT_THAT(input->current_surface(client), Ne(sub_wl_surface))
        << input << " seen by subsurface after it was unmapped";

    EXPECT_THAT(input->current_surface(client), Ne(parent_wl_surface))
        << input << " seen by " << builder << " even through it shouldn't have been over it's input region";

    EXPECT_THAT(input->current_surface(client), Eq(lower_wl_surface))
        << input << " not seen by lower surface";
}

TEST_P(SurfaceInputCombinations, input_falls_through_subsurface_when_parent_unmapped)
{
    auto const top_left = std::make_pair(200, 49);
    wlcs::Client client{the_server()};
    std::shared_ptr<wlcs::SurfaceBuilder> builder;
    std::shared_ptr<wlcs::InputMethod> input;
    std::tie(builder, input) = GetParam();

    auto lower = client.create_visible_surface(surface_size.first, surface_size.second);
    the_server().move_surface_to(lower, top_left.first - 100, top_left.second);
    struct wl_surface* const lower_wl_surface = lower;

    auto parent = builder->build(
        the_server(),
        client,
        top_left,
        surface_size);
    ASSERT_THAT(surface_size.first, Gt(100));
    struct wl_surface* const parent_wl_surface = *parent;
    auto subsurface = std::make_unique<wlcs::Subsurface>(wlcs::Subsurface::create_visible(
        *parent,
        -100, 0,
        surface_size.first, surface_size.second));
    wl_subsurface_set_desync(*subsurface);
    struct wl_surface* const sub_wl_surface = *subsurface;
    client.roundtrip();

    wl_surface_attach(parent_wl_surface, nullptr, 0, 0);
    wl_surface_commit(parent_wl_surface);
    client.roundtrip();

    auto const device = input->create_device(the_server());
    device->move_to({top_left.first - 90, top_left.second + 10});
    client.roundtrip();

    EXPECT_THAT(input->current_surface(client), Ne(sub_wl_surface))
        << input << " seen by subsurface after parent was unmapped";

    EXPECT_THAT(input->current_surface(client), Ne(parent_wl_surface))
        << input << " seen by " << builder << " after being unmapped (also input should have gone to subsurface)";

    EXPECT_THAT(input->current_surface(client), Eq(lower_wl_surface))
        << input << " not seen by lower surface";
}

TEST_P(SurfaceInputCombinations, input_seen_after_surface_unmapped_and_remapped)
{
    auto const top_left = std::make_pair(200, 49);
    auto const input_offset = std::make_pair(4, 4);
    wlcs::Client client{the_server()};
    std::shared_ptr<wlcs::SurfaceBuilder> builder;
    std::shared_ptr<wlcs::InputMethod> input;
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

    surface->attach_visible_buffer(surface_size.first, surface_size.second);

    auto const device = input->create_device(the_server());
    device->move_to({top_left.first + input_offset.first, top_left.second + input_offset.second});
    client.roundtrip();

    EXPECT_THAT(input->current_surface(client), Eq(wl_surface))
        << input << " not seen by " << builder << " after it was unmapped and remapped";

    EXPECT_THAT(input->position_on_surface(client), Eq(input_offset))
        << input << " seen in the wrong place";
}

TEST_P(SurfaceInputCombinations, input_seen_by_subsurface_after_parent_unmapped_and_remapped)
{
    auto const top_left = std::make_pair(200, 49);
    auto const input_offset = std::make_pair(-90, 10);
    auto const subsurface_offset = std::make_pair(-100, 0);
    wlcs::Client client{the_server()};
    std::shared_ptr<wlcs::SurfaceBuilder> builder;
    std::shared_ptr<wlcs::InputMethod> input;
    std::tie(builder, input) = GetParam();

    auto parent = builder->build(
        the_server(),
        client,
        top_left,
        surface_size);
    ASSERT_THAT(surface_size.first, Gt(100));
    struct wl_surface* const parent_wl_surface = *parent;
    auto subsurface = std::make_unique<wlcs::Subsurface>(wlcs::Subsurface::create_visible(
        *parent,
        subsurface_offset.first, subsurface_offset.second,
        surface_size.first, surface_size.second));
    wl_subsurface_set_desync(*subsurface);
    struct wl_surface* const sub_wl_surface = *subsurface;
    client.roundtrip();

    wl_surface_attach(parent_wl_surface, nullptr, 0, 0);
    wl_surface_commit(parent_wl_surface);
    client.roundtrip();

    parent->attach_visible_buffer(surface_size.first, surface_size.second);
    //the_server().move_surface_to(*parent, top_left.first, top_left.second);

    auto const device = input->create_device(the_server());
    device->move_to({top_left.first + input_offset.first, top_left.second + input_offset.second});
    client.roundtrip();

    EXPECT_THAT(input->current_surface(client), Ne(parent_wl_surface))
        << input << " seen by " << builder << " when it should be seen by it's subsurface";

    EXPECT_THAT(input->current_surface(client), Eq(sub_wl_surface))
        << input << " not seen by subsurface after parent was unmapped and remapped";

    EXPECT_THAT(input->position_on_surface(client), Eq(
        std::make_pair(input_offset.first - subsurface_offset.first, input_offset.second - subsurface_offset.second)))
        << input << " seen in the wrong place";
}

TEST_P(SurfaceInputCombinations, input_seen_after_dragged_off_surface)
{
    auto const top_left = std::make_pair(200, 49);
    auto const input_offset = std::make_pair(-5, 5);
    wlcs::Client client{the_server()};
    std::shared_ptr<wlcs::SurfaceBuilder> builder;
    std::shared_ptr<wlcs::InputMethod> input;
    std::tie(builder, input) = GetParam();

    auto other = client.create_visible_surface(100, 100);
    the_server().move_surface_to(other, top_left.first - 102, top_left.second);
    struct wl_surface* const other_wl_surface = other;

    auto main = builder->build(
        the_server(),
        client,
        top_left,
        surface_size);
    struct wl_surface* const main_wl_surface = *main;
    client.roundtrip();

    auto const device = input->create_device(the_server());
    device->move_to({top_left.first + 5, top_left.second + 5});
    client.roundtrip();
    device->drag_to({top_left.first + input_offset.first, top_left.second + input_offset.second});
    client.roundtrip();

    EXPECT_THAT(input->current_surface(client), Ne(other_wl_surface))
        << input << " seen by second surface event though it was dragged from first";

    EXPECT_THAT(input->current_surface(client), Eq(main_wl_surface))
        << input << " not seen by " << builder << " after being dragged away";

    EXPECT_THAT(input->position_on_surface(client), Eq(input_offset))
        << input << " not seen by " << builder << " after being dragged away";
}

TEST_P(SurfaceInputCombinations, input_seen_by_second_surface_after_drag_off_first_and_up)
{
    auto const top_left = std::make_pair(200, 49);
    wlcs::Client client{the_server()};
    std::shared_ptr<wlcs::SurfaceBuilder> builder;
    std::shared_ptr<wlcs::InputMethod> input;
    std::tie(builder, input) = GetParam();

    auto other = builder->build(
        the_server(),
        client,
        std::make_pair(top_left.first - 102, top_left.second),
        std::make_pair(100, 100));
    struct wl_surface* const other_wl_surface = *other;

    auto main = builder->build(
        the_server(),
        client,
        top_left,
        surface_size);
    struct wl_surface* const main_wl_surface = *main;
    client.roundtrip();

    auto const device = input->create_device(the_server());
    device->move_to({top_left.first + 5, top_left.second + 5});
    client.roundtrip();
    device->drag_to({top_left.first - 80, top_left.second + 5});
    client.roundtrip();
    device->move_to({top_left.first - 80, top_left.second + 5});
    client.roundtrip();

    EXPECT_THAT(input->current_surface(client), Ne(main_wl_surface))
        << input << " seen by first " << builder << " after being up";

    EXPECT_THAT(input->current_surface(client), Eq(other_wl_surface))
        << input << " not seen by second " << builder << " after being up";
}

// Will only be instantiated with toplevel surfaces
class ToplevelInputCombinations :
    public wlcs::InProcessServer,
    public testing::WithParamInterface<std::tuple<
        std::shared_ptr<wlcs::SurfaceBuilder>,
        std::shared_ptr<wlcs::InputMethod>>>
{
};

TEST_P(ToplevelInputCombinations, input_falls_through_surface_without_region_after_null_buffer_committed)
{
    auto const top_left = std::make_pair(64, 49);
    wlcs::Client client{the_server()};
    std::shared_ptr<wlcs::SurfaceBuilder> builder;
    std::shared_ptr<wlcs::InputMethod> input;
    std::tie(builder, input) = GetParam();

    auto lower = client.create_visible_surface(surface_size.first, surface_size.second);
    the_server().move_surface_to(lower, top_left.first, top_left.second);
    struct wl_surface* const lower_wl_surface = lower;

    auto upper = builder->build(
        the_server(),
        client,
        top_left,
        surface_size);
    struct wl_surface* const upper_wl_surface = *upper;
    wl_surface_attach(upper_wl_surface, nullptr, 0, 0);
    wl_surface_commit(upper_wl_surface);
    client.roundtrip();

    auto const device = input->create_device(the_server());
    device->move_to(top_left);
    client.roundtrip();

    EXPECT_THAT(input->current_surface(client), Ne(upper_wl_surface))
        << input << " seen by " << builder << " after null buffer committed";

    EXPECT_THAT(input->current_surface(client), Eq(lower_wl_surface))
        << input << " not seen by lower toplevel after null buffer committed to " << builder;
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
    Combine(full_surface_edges, xdg_stable_surface_type, all_input_types));

INSTANTIATE_TEST_SUITE_P(
    SmallerRegion,
    RegionSurfaceInputCombinations,
    Combine(smaller_region_edges, xdg_stable_surface_type, all_input_types));

INSTANTIATE_TEST_SUITE_P(
    ClippedLargerRegion,
    RegionSurfaceInputCombinations,
    Combine(larger_region_edges, xdg_stable_surface_type, all_input_types));

INSTANTIATE_TEST_SUITE_P(
    MultiRectCorners,
    RegionSurfaceInputCombinations,
    Combine(multi_rect_corners, xdg_stable_surface_type, all_input_types));

INSTANTIATE_TEST_SUITE_P(
    SurfaceInputRegions,
    SurfaceInputCombinations,
    Combine(all_surface_types, all_input_types));

INSTANTIATE_TEST_SUITE_P(
    ToplevelInputRegions,
    ToplevelInputCombinations,
    Combine(toplevel_surface_types, all_input_types));
