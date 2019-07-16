/*
 * Copyright © 2019 Canonical Ltd.
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
#include "layer_shell_v1.h"
#include "xdg_shell_stable.h"

#include <gmock/gmock.h>

using namespace testing;

using LayerShellTest = wlcs::InProcessServer;
using Vec2 = std::pair<int, int>;
using Rect = std::pair<Vec2, Vec2>;

TEST_F(LayerShellTest, supports_layer_shell_protocol)
{
    wlcs::Client client{the_server()};
    ASSERT_THAT(client.layer_shell_v1(), NotNull());
    wlcs::Surface surface{client};
    wlcs::LayerSurfaceV1 layer_surface{client, surface};
}

namespace
{

class LayerSurfaceTest: public wlcs::StartedInProcessServer
{
public:
    LayerSurfaceTest()
        : client{the_server()},
          surface{client},
          layer_surface{client, surface}
    {
    }

    void commit_and_wait_for_configure()
    {
        wl_surface_commit(surface);
        layer_surface.dispatch_until_configure();
    }

    void attach_visible_buffer(Vec2 size)
    {
        surface.attach_buffer(size.first, size.second);
        bool surface_rendered{false};
        surface.add_frame_callback([&surface_rendered](auto) { surface_rendered = true; });
        wl_surface_commit(surface);
        client.dispatch_until([&surface_rendered]() { return surface_rendered; });
    }

    void expect_surface_is_at_position(std::pair<int, int> pos)
    {
        auto pointer = the_server().create_pointer();
        pointer.move_to(pos.first + 2, pos.second + 3);
        client.roundtrip();

        ASSERT_THAT(client.focused_window(), Eq((wl_surface*)surface));
        ASSERT_THAT(wl_fixed_to_int(client.pointer_position().first), Eq(2));
        ASSERT_THAT(wl_fixed_to_int(client.pointer_position().second), Eq(3));
    }

    auto output_rect() const -> Rect
    {
        EXPECT_THAT(client.output_count(), Ge(1u)) << "There are no outputs to get a size from";
        EXPECT_THAT(client.output_count(), Eq(1u)) << "Unclear which output the layer shell surface will be placed on";
        auto output_state = client.output_state(0);
        EXPECT_THAT(output_state.mode_size, IsTrue()) << "Output has no size";
        EXPECT_THAT(output_state.geometry_position, IsTrue()) << "Output has no position";
        auto size = output_state.mode_size.value();
        if (output_state.scale)
        {
            size.first /= output_state.scale.value();
            size.second /= output_state.scale.value();
        }
        return std::make_pair(output_state.geometry_position.value(), size);
    }

    auto configured_size() const -> Vec2
    {
        return std::make_pair(layer_surface.last_width(), layer_surface.last_height());
    }

    wlcs::Client client;
    wlcs::Surface surface;
    wlcs::LayerSurfaceV1 layer_surface;

    int static const default_width = 40;
    int static const default_height = 50;
};

struct LayerAnchor
{
    static auto get_all() -> std::vector<LayerAnchor>
    {
        bool const range[]{false, true};
        std::vector<LayerAnchor> result;
        for (auto left: range)
            for (auto right: range)
                for (auto top: range)
                    for (auto bottom: range)
                        result.emplace_back(left, right, top, bottom);
        return result;
    }

    LayerAnchor(bool left, bool right, bool top, bool bottom)
        : left{left},
          right{right},
          top{top},
          bottom{bottom}
    {
    }

    LayerAnchor(uint32_t const anchor)
        : left  {static_cast<bool>(anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT)},
          right {static_cast<bool>(anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT)},
          top   {static_cast<bool>(anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP)},
          bottom{static_cast<bool>(anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM)}
    {
    }

    operator uint32_t() const
    {
        return
            (left   ? ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT   : 0) |
            (right  ? ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT  : 0) |
            (top    ? ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP    : 0) |
            (bottom ? ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM : 0);
    }

    auto h_expand() const -> bool { return left && right; }
    auto v_expand() const -> bool { return top && bottom; }

    auto placement_rect(Rect const& output) const -> Rect
    {
        int const output_width = output.second.first;
        int const output_height = output.second.second;
        int const output_x = output.first.first;
        int const output_y = output.first.second;
        int const width = h_expand() ? output_width : LayerSurfaceTest::default_width;
        int const height = v_expand() ? output_height : LayerSurfaceTest::default_height;
        int const x =
            (left ?
                output_x :
                (right ?
                    (output_x + output_width - width) :
                    (output_x + (output_width - width) / 2)
                )
            );
        int const y =
            (top ?
                output_y :
                (bottom ?
                    (output_y + output_height - height) :
                    (output_y + (output_height - height) / 2)
                )
            );
        return std::make_pair(std::make_pair(x, y), std::make_pair(width, height));;
    }

    auto configure_size(Rect const& output) const -> Vec2
    {
        int const configure_width = h_expand() ? output.second.first : 0;
        int const configure_height = v_expand() ? output.second.second : 0;
        return std::make_pair(configure_width, configure_height);
    }

    // Will always either return 0, or a single enum value
    auto attached_edge() const -> zwlr_layer_surface_v1_anchor
    {
        if (top == bottom)
        {
            if (left && !right)
                return ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;
            else if (right && !left)
                return ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
        }
        else if (left == right)
        {
            if (top && !bottom)
                return ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP;
            else if (bottom && !top)
                return ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
        }
        return (zwlr_layer_surface_v1_anchor)0;
    }

    bool const left, right, top, bottom;
};

std::ostream& operator<<(std::ostream& os, const LayerAnchor& anchor)
{
    std::vector<std::string> strs;
    if (anchor.left)
        strs.emplace_back("left");
    if (anchor.right)
        strs.emplace_back("right");
    if (anchor.top)
        strs.emplace_back("top");
    if (anchor.bottom)
        strs.emplace_back("bottom");
    if (strs.empty())
        strs.emplace_back("none");
    os << "Anchor{";
    for (auto i = 0u; i < strs.size(); i++)
    {
        if (i > 0)
            os << " | ";
        os << strs[i];
    }
    os << "}";
    return os;
}

class LayerSurfaceAnchorTest:
    public LayerSurfaceTest,
    public testing::WithParamInterface<LayerAnchor>
{
};

}

TEST_F(LayerSurfaceTest, can_open_layer_surface)
{
    commit_and_wait_for_configure();
}

TEST_F(LayerSurfaceTest, by_default_gets_configured_without_size)
{
    commit_and_wait_for_configure();
    ASSERT_THAT(configured_size(), Eq(std::make_pair(0, 0)));
}

TEST_F(LayerSurfaceTest, when_anchored_to_all_edges_gets_configured_with_output_size)
{
    zwlr_layer_surface_v1_set_anchor(layer_surface, LayerAnchor(true, true, true, true));
    commit_and_wait_for_configure();
    auto const size = output_rect().second;
    ASSERT_THAT(configured_size(), Eq(size));
}

TEST_F(LayerSurfaceTest, gets_configured_after_anchor_change)
{
    commit_and_wait_for_configure();
    zwlr_layer_surface_v1_set_anchor(layer_surface, LayerAnchor(true, true, true, true));
    commit_and_wait_for_configure();
    ASSERT_THAT(configured_size().first, Gt(0));
    ASSERT_THAT(configured_size().second, Gt(0));
}

TEST_P(LayerSurfaceAnchorTest, is_initially_positioned_correctly_for_anchor)
{
    auto const anchor = GetParam();
    zwlr_layer_surface_v1_set_anchor(layer_surface, anchor);
    commit_and_wait_for_configure();
    auto const output = output_rect();

    auto configure_size = anchor.configure_size(output);
    if (configure_size.first)
    {
        EXPECT_THAT(configured_size().first, Eq(configure_size.first));
    }
    if (configure_size.second)
    {
        EXPECT_THAT(configured_size().second, Eq(configure_size.second));
    }

    auto rect = anchor.placement_rect(output);
    attach_visible_buffer(rect.second);
    expect_surface_is_at_position(rect.first);
}

TEST_P(LayerSurfaceAnchorTest, is_positioned_correctly_when_anchor_changed)
{
    commit_and_wait_for_configure();
    auto const output = output_rect();
    auto initial_rect = LayerAnchor(false, false, false, false).placement_rect(output);
    attach_visible_buffer(initial_rect.second);

    auto const anchor = GetParam();
    zwlr_layer_surface_v1_set_anchor(layer_surface, anchor);
    wl_surface_commit(surface);
    client.roundtrip(); // Sometimes we get a configure, sometimes we don't

    auto configure_size = anchor.configure_size(output);
    if (configure_size.first)
    {
        EXPECT_THAT(configured_size().first, Eq(configure_size.first));
    }
    if (configure_size.second)
    {
        EXPECT_THAT(configured_size().second, Eq(configure_size.second));
    }

    auto new_rect = anchor.placement_rect(output);
    attach_visible_buffer(new_rect.second);
    expect_surface_is_at_position(new_rect.first);
}

TEST_P(LayerSurfaceAnchorTest, maximized_xdg_toplevel_is_shrunk_for_exclusive_zone)
{
    int const exclusive_zone = 25;
    int width = 0, height = 0;
    wlcs::Surface other_surface{client};
    wlcs::XdgSurfaceStable xdg_surface{client, other_surface};
    wlcs::XdgToplevelStable toplevel{xdg_surface};
    toplevel.add_configure_notification([&](int32_t w, int32_t h, wl_array* /*states*/)
        {
            if (w != width || h != height)
            {
                width = w;
                height = h;
                if (w == 0)
                    w = 100;
                if (h == 0)
                    h = 150;
                other_surface.attach_buffer(w, h);
                xdg_surface_set_window_geometry(xdg_surface, 0, 0, w, h);
            }
        });
    xdg_surface.add_configure_notification([&](uint32_t serial)
        {
            xdg_surface_ack_configure(xdg_surface, serial);
            wl_surface_commit(other_surface);
        });
    xdg_toplevel_set_maximized(toplevel);
    wl_surface_commit(other_surface);
    client.dispatch_until([&](){ return width > 0; });

    int const initial_width = width;
    int const initial_height = height;

    ASSERT_THAT(initial_width, Gt(0)) << "Can't test as shell did not configure XDG surface with a size";
    ASSERT_THAT(initial_height, Gt(0)) << "Can't test as shell did not configure XDG surface with a size";

    auto const anchor = GetParam();
    zwlr_layer_surface_v1_set_anchor(layer_surface, anchor);
    zwlr_layer_surface_v1_set_exclusive_zone(layer_surface, exclusive_zone);
    commit_and_wait_for_configure();
    auto layer_size = configured_size();
    if (layer_size.first == 0)
        layer_size.first = default_width;
    if (layer_size.second == 0)
        layer_size.second = default_height;
    attach_visible_buffer(layer_size);

    int const new_width = width;
    int const new_height = height;

    int expected_width = initial_width;
    int expected_height = initial_height;

    switch (anchor.attached_edge())
    {
    case ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT:
        expected_width -= exclusive_zone;
        break;

    case ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT:
        expected_width -= exclusive_zone;
        break;

    case ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP:
        expected_height -= exclusive_zone;
        break;

    case ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM:
        expected_height -= exclusive_zone;
        break;

    default: ;
    }

    EXPECT_THAT(new_width, Eq(expected_width));
    EXPECT_THAT(new_height, Eq(expected_height));
}

INSTANTIATE_TEST_CASE_P(
    Anchor,
    LayerSurfaceAnchorTest,
    testing::ValuesIn(LayerAnchor::get_all()));
