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
#include "version_specifier.h"

#include <gmock/gmock.h>

using namespace testing;

using Vec2 = std::pair<int, int>;
using Rect = std::pair<Vec2, Vec2>;

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

    ~LayerSurfaceTest()
    {
        client.roundtrip();
    }

    void commit_and_wait_for_configure()
    {
        wl_surface_commit(surface);
        layer_surface.dispatch_until_configure();
    }

    void expect_surface_is_at_position(std::pair<int, int> pos, wlcs::Surface const& expected)
    {
        auto pointer = the_server().create_pointer();
        pointer.move_to(pos.first + 10, pos.second + 10);
        client.roundtrip();

        EXPECT_THAT(client.window_under_cursor(), Eq((wl_surface*)expected));
        if (client.window_under_cursor() == expected)
        {
            EXPECT_THAT(wl_fixed_to_int(client.pointer_position().first), Eq(10));
            EXPECT_THAT(wl_fixed_to_int(client.pointer_position().second), Eq(10));
        }
    }

    void expect_surface_is_at_position(std::pair<int, int> pos)
    {
        expect_surface_is_at_position(pos, surface);
    }

    auto output_rect() const -> Rect
    {
        EXPECT_THAT(client.output_count(), Ge(1u)) << "There are no outputs to get a size from";
        EXPECT_THAT(client.output_count(), Eq(1u)) << "Unclear which output the layer shell surface will be placed on";
        auto output_state = client.output_state(0);
        EXPECT_THAT((bool)output_state.mode_size, Eq(true)) << "Output has no size";
        EXPECT_THAT((bool)output_state.geometry_position, Eq(true)) << "Output has no position";
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

    int static const default_width = 200;
    int static const default_height = 300;
};

struct LayerSurfaceLayout
{
    template<typename T>
    struct Sides
    {
        T left, right, top, bottom;
    };

    static auto get_all() -> std::vector<LayerSurfaceLayout>
    {
        bool const range[]{false, true};
        std::vector<LayerSurfaceLayout> result;
        for (auto left: range)
        {
            for (auto right: range)
            {
                for (auto top: range)
                {
                    for (auto bottom: range)
                    {
                        result.emplace_back(Sides<bool>{left, right, top, bottom});
                        result.emplace_back(Sides<bool>{left, right, top, bottom}, Sides<int>{6, 9, 12, 15});
                    }
                }
            }
        }
        return result;
    }

    LayerSurfaceLayout(Sides<bool> anchor)
        : anchor{anchor},
          margin{0, 0, 0, 0}
    {
    }

    LayerSurfaceLayout(Sides<bool> anchor, Sides<int> margin)
        : anchor{anchor},
          margin{margin}
    {
    }

    operator uint32_t() const
    {
        return
            (anchor.left   ? ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT   : 0) |
            (anchor.right  ? ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT  : 0) |
            (anchor.top    ? ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP    : 0) |
            (anchor.bottom ? ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM : 0);
    }

    auto h_expand() const -> bool { return anchor.left && anchor.right; }
    auto v_expand() const -> bool { return anchor.top && anchor.bottom; }

    auto placement_rect(Rect const& output) const -> Rect
    {
        int const output_width = output.second.first;
        int const output_height = output.second.second;
        int const output_x = output.first.first;
        int const output_y = output.first.second;
        auto const config_size = configure_size(output);
        int const width = config_size.first ? config_size.first : LayerSurfaceTest::default_width;
        int const height = config_size.second ? config_size.second : LayerSurfaceTest::default_height;
        int const x =
            (anchor.left ?
                output_x + margin.left :
                (anchor.right ?
                    (output_x + output_width - width - margin.right) :
                    (output_x + (output_width - width) / 2)
                )
            );
        int const y =
            (anchor.top ?
                output_y + margin.top :
                (anchor.bottom ?
                    (output_y + output_height - height - margin.bottom) :
                    (output_y + (output_height - height) / 2)
                )
            );
        return std::make_pair(std::make_pair(x, y), std::make_pair(width, height));;
    }

    auto request_size() const -> Vec2
    {
        return std::make_pair(
            h_expand() ? 0 : LayerSurfaceTest::default_width,
            v_expand() ? 0 : LayerSurfaceTest::default_height);
    }

    auto configure_size(Rect const& output) const -> Vec2
    {
        int const configure_width = h_expand() ? output.second.first - margin.left - margin.right : 0;
        int const configure_height = v_expand() ? output.second.second - margin.top - margin.bottom : 0;
        return std::make_pair(configure_width, configure_height);
    }

    // Will always either return 0, or a single enum value
    auto attached_edge() const -> zwlr_layer_surface_v1_anchor
    {
        if (anchor.top == anchor.bottom)
        {
            if (anchor.left && !anchor.right)
                return ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;
            else if (anchor.right && !anchor.left)
                return ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
        }
        else if (anchor.left == anchor.right)
        {
            if (anchor.top && !anchor.bottom)
                return ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP;
            else if (anchor.bottom && !anchor.top)
                return ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
        }
        return (zwlr_layer_surface_v1_anchor)0;
    }

    Sides<bool> const anchor;
    Sides<int> const margin;
};

static void invoke_zwlr_layer_surface_v1_set_margin(
    zwlr_layer_surface_v1* layer_surface,
    LayerSurfaceLayout::Sides<int> const& margin)
{
    zwlr_layer_surface_v1_set_margin(layer_surface, margin.top, margin.right, margin.bottom, margin.left);
}

std::ostream& operator<<(std::ostream& os, const LayerSurfaceLayout& layout)
{
    std::vector<std::string> strs;
    if (layout.anchor.left)
        strs.emplace_back("left");
    if (layout.anchor.right)
        strs.emplace_back("right");
    if (layout.anchor.top)
        strs.emplace_back("top");
    if (layout.anchor.bottom)
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
    os << "}, Margin{";
    if (layout.margin.left || layout.margin.right || layout.margin.top || layout.margin.bottom)
    {
        os << "l: " << layout.margin.left << ", ";
        os << "r: " << layout.margin.right << ", ";
        os << "t: " << layout.margin.top << ", ";
        os << "b: " << layout.margin.bottom;
    }
    os << "}";
    return os;
}

class LayerSurfaceLayoutTest:
    public LayerSurfaceTest,
    public testing::WithParamInterface<LayerSurfaceLayout>
{
};

struct LayerLayerParams
{
    std::experimental::optional<zwlr_layer_shell_v1_layer> below;
    std::experimental::optional<zwlr_layer_shell_v1_layer> above;
};

std::ostream& operator<<(std::ostream& os, const LayerLayerParams& layer)
{
    os << "layers:";
    for (auto const& i : {layer.below, layer.above})
    {
        os << " ";
        os << "Layer{";
        if (i)
        {
            switch (auto const value = i.value())
            {
            case ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND:
                os << "background";
                break;
            case ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM:
                os << "bottom";
                break;
            case ZWLR_LAYER_SHELL_V1_LAYER_TOP:
                os << "top";
                break;
            case ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY:
                os << "overlay";
                break;
            default:
                os << "INVALID(" << value << ")";
            }
        }
        else
        {
            os << "none";
        }
        os << "}";
    }
    return os;
}

class LayerSurfaceLayerTest:
    public wlcs::StartedInProcessServer,
    public testing::WithParamInterface<LayerLayerParams>
{
public:
    struct SurfaceOnLayer
    {
        SurfaceOnLayer(
            wlcs::Client& client,
            std::experimental::optional<zwlr_layer_shell_v1_layer> layer)
            : surface{layer ? wlcs::Surface{client} : client.create_visible_surface(width, height)}
        {
            if (layer)
            {
                layer_surface.emplace(client, surface, layer.value());
                zwlr_layer_surface_v1_set_size(layer_surface.value(), width, height);
                surface.attach_visible_buffer(width, height);
            }
        }

        wlcs::Surface surface;
        std::experimental::optional<wlcs::LayerSurfaceV1> layer_surface;
    };

    LayerSurfaceLayerTest()
        : client(the_server())
    {
    }

    static const int width{200}, height{100};
    wlcs::Client client;
};

struct SizeAndAnchors
{
    uint32_t width, height;
    LayerSurfaceLayout anchors;
};

class LayerSurfaceErrorsTest:
    public LayerSurfaceTest,
    public testing::WithParamInterface<SizeAndAnchors>
{
};

}

TEST_F(LayerSurfaceTest, specifying_no_size_without_anchors_is_an_error)
{
    try
    {
        // Protocol specifies that a size of (0,0) is the default
        commit_and_wait_for_configure();
    }
    catch (wlcs::ProtocolError const& err)
    {
        EXPECT_THAT(err.interface(), Eq(&zwlr_layer_surface_v1_interface));
        // The protocol does not explicitly state what error to send here; INVALID_SIZE seems most appropriate
        EXPECT_THAT(err.error_code(), Eq(ZWLR_LAYER_SURFACE_V1_ERROR_INVALID_SIZE));
        return;
    }
    FAIL() << "Expected protocol error not raised";
}

TEST_F(LayerSurfaceTest, specifying_zero_size_without_anchors_is_an_error)
{
    try
    {
        zwlr_layer_surface_v1_set_size(layer_surface, 0, 0);
        commit_and_wait_for_configure();
    }
    catch (wlcs::ProtocolError const& err)
    {
        EXPECT_THAT(err.interface(), Eq(&zwlr_layer_surface_v1_interface));
        // The protocol does not explicitly state what error to send here; INVALID_SIZE seems most appropriate
        EXPECT_THAT(err.error_code(), Eq(ZWLR_LAYER_SURFACE_V1_ERROR_INVALID_SIZE));
        return;
    }
    FAIL() << "Expected protocol error not raised";
}

TEST_P(LayerSurfaceErrorsTest, specifying_zero_size_without_corresponding_anchors_is_an_error)
{
    try
    {
        zwlr_layer_surface_v1_set_size(layer_surface, GetParam().width, GetParam().height);
        zwlr_layer_surface_v1_set_anchor(layer_surface, GetParam().anchors);
        commit_and_wait_for_configure();
    }
    catch (wlcs::ProtocolError const& err)
    {
        EXPECT_THAT(err.interface(), Eq(&zwlr_layer_surface_v1_interface));
        // The protocol does not explicitly state what error to send here; INVALID_SIZE seems most appropriate
        EXPECT_THAT(err.error_code(), Eq(ZWLR_LAYER_SURFACE_V1_ERROR_INVALID_SIZE));
        return;
    }
    FAIL() << "Expected protocol error not raised";
}

INSTANTIATE_TEST_SUITE_P(
    Anchors,
    LayerSurfaceErrorsTest,
    testing::Values(
        SizeAndAnchors{0, 0, LayerSurfaceLayout{{false, false, false, false}}},
        SizeAndAnchors{0, 0, LayerSurfaceLayout{{true, false, false, false}}},
        SizeAndAnchors{0, 0, LayerSurfaceLayout{{false, true, false, false}}},
        SizeAndAnchors{0, 0, LayerSurfaceLayout{{true, true, false, false}}},
        SizeAndAnchors{0, 0, LayerSurfaceLayout{{false, false, true, false}}},
        SizeAndAnchors{0, 0, LayerSurfaceLayout{{true, false, true, false}}},
        SizeAndAnchors{0, 0, LayerSurfaceLayout{{false, true, true, false}}},
        SizeAndAnchors{0, 0, LayerSurfaceLayout{{true, true, true, false}}},
        SizeAndAnchors{0, 0, LayerSurfaceLayout{{false, false, false, true}}},
        SizeAndAnchors{0, 0, LayerSurfaceLayout{{true, false, false, true}}},
        SizeAndAnchors{0, 0, LayerSurfaceLayout{{false, true, false, true}}},
        SizeAndAnchors{0, 0, LayerSurfaceLayout{{true, true, false, true}}},
        SizeAndAnchors{0, 0, LayerSurfaceLayout{{false, true, true, true}}},
        SizeAndAnchors{200, 0, LayerSurfaceLayout{{false, false, false, true}}},
        SizeAndAnchors{200, 0, LayerSurfaceLayout{{false, false, true, false}}},
        SizeAndAnchors{0, 200, LayerSurfaceLayout{{true, false, false, true}}},
        SizeAndAnchors{0, 200, LayerSurfaceLayout{{false, true, true, false}}}
    ));

TEST_F(LayerSurfaceTest, can_open_layer_surface)
{
    zwlr_layer_surface_v1_set_size(layer_surface, default_width, default_height);
    commit_and_wait_for_configure();
}

TEST_F(LayerSurfaceTest, gets_configured_with_supplied_size_when_set)
{
    int width = 123, height = 546;
    zwlr_layer_surface_v1_set_size(layer_surface, width, height);
    commit_and_wait_for_configure();
    EXPECT_THAT(configured_size(), Eq(std::make_pair(width, height)));
}

TEST_F(LayerSurfaceTest, gets_configured_with_supplied_size_even_when_anchored_to_edges)
{
    int width = 321, height = 218;
    zwlr_layer_surface_v1_set_anchor(layer_surface, LayerSurfaceLayout({true, true, true, true}));
    zwlr_layer_surface_v1_set_size(layer_surface, width, height);
    commit_and_wait_for_configure();
    EXPECT_THAT(configured_size(), Eq(std::make_pair(width, height)));
}

TEST_F(LayerSurfaceTest, when_anchored_to_all_edges_gets_configured_with_output_size)
{
    zwlr_layer_surface_v1_set_anchor(layer_surface, LayerSurfaceLayout({true, true, true, true}));
    commit_and_wait_for_configure();
    auto const size = output_rect().second;
    ASSERT_THAT(configured_size(), Eq(size));
}

TEST_F(LayerSurfaceTest, gets_configured_after_anchor_change)
{
    zwlr_layer_surface_v1_set_size(layer_surface, default_width, default_height);
    commit_and_wait_for_configure();
    zwlr_layer_surface_v1_set_size(layer_surface, 0, 0);
    zwlr_layer_surface_v1_set_anchor(layer_surface, LayerSurfaceLayout({true, true, true, true}));
    commit_and_wait_for_configure();
    EXPECT_THAT(configured_size().first, Gt(0));
    EXPECT_THAT(configured_size().second, Gt(0));
}

TEST_F(LayerSurfaceTest, destroy_request_supported)
{
    wlcs::Client client{the_server()};

    {
        auto const layer_shell = client.bind_if_supported<zwlr_layer_shell_v1>(
            wlcs::AtLeastVersion{ZWLR_LAYER_SHELL_V1_DESTROY_SINCE_VERSION});
        client.roundtrip();
    }
    // layer_shell is now destroyed

    client.roundtrip();
}

TEST_F(LayerSurfaceTest, destroy_request_not_sent_when_not_supported)
{
    wlcs::Client client{the_server()};

    {
        auto const layer_shell = client.bind_if_supported<zwlr_layer_shell_v1>(
            wlcs::ExactlyVersion{ZWLR_LAYER_SHELL_V1_DESTROY_SINCE_VERSION - 1});
        client.roundtrip();
    }
    // layer_shell is now destroyed

    client.roundtrip();
}

TEST_F(LayerSurfaceTest, does_not_take_keyboard_focus_without_keyboard_interactivity)
{
    auto normal_surface = client.create_visible_surface(100, 100);
    the_server().move_surface_to(normal_surface, 0, default_height);
    auto pointer = the_server().create_pointer();
    pointer.move_to(5, default_height + 5);
    pointer.left_click();
    client.roundtrip();
    ASSERT_THAT(client.keyboard_focused_window(), Eq(static_cast<wl_surface*>(normal_surface)))
        << "Could not run test because normal surface was not given keyboeard focus";

    zwlr_layer_surface_v1_set_size(layer_surface, default_width, default_height);
    zwlr_layer_surface_v1_set_anchor(
        layer_surface,
        ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);
    commit_and_wait_for_configure();
    surface.attach_visible_buffer(default_width, default_height);

    EXPECT_THAT(client.keyboard_focused_window(), Eq(static_cast<wl_surface*>(normal_surface)))
        << "Normal surface lost keyboard focus when non-keyboard layer surface appeared";
}

TEST_F(LayerSurfaceTest, takes_keyboard_focus_with_exclusive_keyboard_interactivity)
{
    auto normal_surface = client.create_visible_surface(100, 100);
    the_server().move_surface_to(normal_surface, 0, default_height);
    auto pointer = the_server().create_pointer();
    pointer.move_to(5, default_height + 5);
    pointer.left_click();
    client.roundtrip();
    ASSERT_THAT(client.keyboard_focused_window(), Eq(static_cast<wl_surface*>(normal_surface)))
        << "Could not run test because normal surface was not given keyboeard focus";

    zwlr_layer_surface_v1_set_keyboard_interactivity(
        layer_surface,
        ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE);
    zwlr_layer_surface_v1_set_size(layer_surface, default_width, default_height);
    zwlr_layer_surface_v1_set_anchor(
        layer_surface,
        ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);
    commit_and_wait_for_configure();
    surface.attach_visible_buffer(default_width, default_height);

    EXPECT_THAT(client.keyboard_focused_window(), Eq(static_cast<wl_surface*>(surface)))
        << "Layer surface not given keyboard focus in exclusive mode";
}

TEST_F(LayerSurfaceTest, takes_keyboard_focus_after_click_with_on_demand_keyboard_interactivity)
{
    auto normal_surface = client.create_visible_surface(100, 100);
    the_server().move_surface_to(normal_surface, 0, default_height);
    auto pointer = the_server().create_pointer();
    pointer.move_to(5, default_height + 5);
    pointer.left_click();
    client.roundtrip();
    ASSERT_THAT(client.keyboard_focused_window(), Eq(static_cast<wl_surface*>(normal_surface)))
        << "Could not run test because normal surface was not given keyboeard focus";

    {
        wlcs::Client client{the_server()};
        auto const layer_shell = client.bind_if_supported<zwlr_layer_shell_v1>(
            wlcs::AtLeastVersion{ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND_SINCE_VERSION});
        client.roundtrip();
    }

    zwlr_layer_surface_v1_set_keyboard_interactivity(
        layer_surface,
        ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND);
    zwlr_layer_surface_v1_set_size(layer_surface, default_width, default_height);
    zwlr_layer_surface_v1_set_anchor(
        layer_surface,
        ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);
    commit_and_wait_for_configure();
    surface.attach_visible_buffer(default_width, default_height);
    client.roundtrip();

    pointer.move_to(5, 5);
    pointer.left_click();
    client.roundtrip();

    EXPECT_THAT(client.keyboard_focused_window(), Eq(static_cast<wl_surface*>(surface)))
        << "Layer surface not given keyboard focus in on-demand mode";
}

TEST_F(LayerSurfaceTest, does_not_lose_keyboard_focus_with_exclusive_keyboard_interactivity)
{
    zwlr_layer_surface_v1_set_keyboard_interactivity(
        layer_surface,
        ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE);
    zwlr_layer_surface_v1_set_size(layer_surface, default_width, default_height);
    zwlr_layer_surface_v1_set_anchor(
        layer_surface,
        ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);
    commit_and_wait_for_configure();
    surface.attach_visible_buffer(default_width, default_height);
    ASSERT_THAT(client.keyboard_focused_window(), Eq(static_cast<wl_surface*>(surface)))
        << "Layer surface not given keyboard focus in exclusive mode";

    auto normal_surface = client.create_visible_surface(100, 100);
    the_server().move_surface_to(normal_surface, 0, default_height);
    auto pointer = the_server().create_pointer();
    pointer.move_to(5, default_height + 5);
    pointer.left_click();
    client.roundtrip();
    ASSERT_THAT(client.keyboard_focused_window(), Eq(static_cast<wl_surface*>(surface)))
        << "Creating a normal surface caused the layer surface in exclusive mode to lose keyboard focus";
}

TEST_F(LayerSurfaceTest, can_lose_keyboard_focus_with_on_demand_keyboard_interactivity)
{
    {
        wlcs::Client client{the_server()};
        auto const layer_shell = client.bind_if_supported<zwlr_layer_shell_v1>(
            wlcs::AtLeastVersion{ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND_SINCE_VERSION});
        client.roundtrip();
    }

    zwlr_layer_surface_v1_set_keyboard_interactivity(
        layer_surface,
        ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND);
    zwlr_layer_surface_v1_set_size(layer_surface, default_width, default_height);
    zwlr_layer_surface_v1_set_anchor(
        layer_surface,
        ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);
    commit_and_wait_for_configure();
    surface.attach_visible_buffer(default_width, default_height);
    ASSERT_THAT(client.keyboard_focused_window(), Eq(static_cast<wl_surface*>(surface)))
        << "Layer surface not given keyboard focus in exclusive mode";

    auto normal_surface = client.create_visible_surface(100, 100);
    the_server().move_surface_to(normal_surface, 0, default_height);
    auto pointer = the_server().create_pointer();
    pointer.move_to(5, default_height + 5);
    pointer.left_click();
    client.roundtrip();
    ASSERT_THAT(client.keyboard_focused_window(), Ne(static_cast<wl_surface*>(surface)))
        << "Creating a normal surface did not cause the layer surface in on-demand mode to lose keyboard focus";
}

TEST_F(LayerSurfaceTest, takes_keyboard_focus_when_interactivity_changes_to_exclusive)
{
    auto normal_surface = client.create_visible_surface(100, 100);
    the_server().move_surface_to(normal_surface, 0, default_height);
    auto pointer = the_server().create_pointer();
    pointer.move_to(5, default_height + 5);
    pointer.left_click();
    client.roundtrip();
    ASSERT_THAT(client.keyboard_focused_window(), Eq(static_cast<wl_surface*>(normal_surface)))
        << "Could not run test because normal surface was not given keyboeard focus";

    zwlr_layer_surface_v1_set_keyboard_interactivity(
        layer_surface,
        ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE);
    zwlr_layer_surface_v1_set_size(layer_surface, default_width, default_height);
    zwlr_layer_surface_v1_set_anchor(
        layer_surface,
        ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);
    commit_and_wait_for_configure();
    surface.attach_visible_buffer(default_width, default_height);

    ASSERT_THAT(client.keyboard_focused_window(), Eq(static_cast<wl_surface*>(normal_surface)))
        << "Could not run test because normal surface was not given keyboeard focus";

    zwlr_layer_surface_v1_set_keyboard_interactivity(
        layer_surface,
        ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE);
    wl_surface_commit(surface);
    client.roundtrip();

    ASSERT_THAT(client.keyboard_focused_window(), Eq(static_cast<wl_surface*>(surface)))
        << "Layer surface did not take keyboard focus when mode changed to exclusive";

    pointer.left_click();
    client.roundtrip();

    EXPECT_THAT(client.keyboard_focused_window(), Eq(static_cast<wl_surface*>(surface)))
        << "Layer surface did not hold exclusive keyboard focus";
}

TEST_F(LayerSurfaceTest, loses_keybaord_focus_when_interactivity_changes_to_none)
{
    auto normal_surface = client.create_visible_surface(100, 100);
    ASSERT_THAT(client.keyboard_focused_window(), Eq(static_cast<wl_surface*>(normal_surface)))
        << "Could not run test because normal surface was not given keyboeard focus";

    zwlr_layer_surface_v1_set_keyboard_interactivity(
        layer_surface,
        ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE);
    zwlr_layer_surface_v1_set_size(layer_surface, default_width, default_height);
    zwlr_layer_surface_v1_set_anchor(
        layer_surface,
        ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);
    commit_and_wait_for_configure();
    surface.attach_visible_buffer(default_width, default_height);

    ASSERT_THAT(client.keyboard_focused_window(), Eq(static_cast<wl_surface*>(surface)))
        << "Could not run test because surface not given keyboard focus despite exclusive interactivity";

    zwlr_layer_surface_v1_set_keyboard_interactivity(
        layer_surface,
        ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE);
    wl_surface_commit(surface);
    client.roundtrip();

    ASSERT_THAT(client.keyboard_focused_window(), Ne(static_cast<wl_surface*>(surface)))
        << "Layer surface did not lose keyboard focus when interactivity changed to none";
}

TEST_P(LayerSurfaceLayoutTest, is_initially_positioned_correctly_for_anchor)
{
    auto const layout = GetParam();
    auto const output = output_rect();
    auto const rect = layout.placement_rect(output);
    auto const request_size = layout.request_size();

    zwlr_layer_surface_v1_set_anchor(layer_surface, layout);
    invoke_zwlr_layer_surface_v1_set_margin(layer_surface, layout.margin);
    zwlr_layer_surface_v1_set_size(layer_surface, request_size.first, request_size.second);
    commit_and_wait_for_configure();

    auto configure_size = layout.configure_size(output);
    if (configure_size.first)
    {
        EXPECT_THAT(configured_size().first, Eq(configure_size.first));
    }
    if (configure_size.second)
    {
        EXPECT_THAT(configured_size().second, Eq(configure_size.second));
    }

    surface.attach_visible_buffer(rect.second.first, rect.second.second);
    expect_surface_is_at_position(rect.first);
}

TEST_P(LayerSurfaceLayoutTest, is_positioned_correctly_when_explicit_size_does_not_match_buffer_size)
{
    auto const layout = GetParam();
    int const initial_width{52}, initial_height{74};
    auto const rect = layout.placement_rect(output_rect());
    auto const request_size = layout.request_size();

    zwlr_layer_surface_v1_set_anchor(layer_surface, layout);
    invoke_zwlr_layer_surface_v1_set_margin(layer_surface, layout.margin);
    zwlr_layer_surface_v1_set_size(layer_surface, request_size.first, request_size.second);
    commit_and_wait_for_configure();

    surface.attach_visible_buffer(initial_width, initial_height);

    expect_surface_is_at_position(rect.first);
}

TEST_P(LayerSurfaceLayoutTest, is_positioned_correctly_when_layout_changed)
{
    auto const layout = GetParam();
    auto const output = output_rect();
    auto const result_rect = layout.placement_rect(output);
    auto const request_size = layout.request_size();

    zwlr_layer_surface_v1_set_size(layer_surface, default_width - 5, default_height - 2);
    commit_and_wait_for_configure();

    zwlr_layer_surface_v1_set_anchor(layer_surface, layout);
    invoke_zwlr_layer_surface_v1_set_margin(layer_surface, layout.margin);
    zwlr_layer_surface_v1_set_size(layer_surface, request_size.first, request_size.second);
    surface.attach_visible_buffer(result_rect.second.first, result_rect.second.second);
    wl_surface_commit(surface);
    client.roundtrip(); // Sometimes we get a configure, sometimes we don't

    auto configure_size = layout.configure_size(output);
    if (configure_size.first)
    {
        EXPECT_THAT(configured_size().first, Eq(configure_size.first));
    }
    if (configure_size.second)
    {
        EXPECT_THAT(configured_size().second, Eq(configure_size.second));
    }
    expect_surface_is_at_position(result_rect.first);
}

TEST_P(LayerSurfaceLayoutTest, is_positioned_correctly_after_multiple_changes)
{
    auto const layout = GetParam();
    auto const output = output_rect();
    auto const result_rect = layout.placement_rect(output);
    auto const request_size = layout.request_size();

    zwlr_layer_surface_v1_set_size(layer_surface, default_width, default_height);
    commit_and_wait_for_configure();

    zwlr_layer_surface_v1_set_anchor(layer_surface, layout);
    invoke_zwlr_layer_surface_v1_set_margin(layer_surface, layout.margin);
    zwlr_layer_surface_v1_set_size(layer_surface, request_size.first, request_size.second);
    surface.attach_visible_buffer(result_rect.second.first, result_rect.second.second);
    wl_surface_commit(surface);
    client.roundtrip(); // Sometimes we get a configure, sometimes we don't

    zwlr_layer_surface_v1_set_anchor(layer_surface, 0);
    zwlr_layer_surface_v1_set_margin(layer_surface, 0, 0, 0, 0);
    zwlr_layer_surface_v1_set_size(layer_surface, default_width, default_height);
    surface.attach_visible_buffer(default_width, default_height);
    wl_surface_commit(surface);
    client.roundtrip(); // Sometimes we get a configure, sometimes we don't

    EXPECT_THAT(configured_size().first, Eq(default_width));
    EXPECT_THAT(configured_size().second, Eq(default_height));
    auto expected_top_left = output.first;
    expected_top_left.first += (output.second.first - default_width) / 2;
    expected_top_left.second += (output.second.second - default_height) / 2;
    expect_surface_is_at_position(expected_top_left);
}

TEST_P(LayerSurfaceLayoutTest, is_positioned_to_accommodate_other_surfaces_exclusive_zone)
{
    auto const layout = GetParam();
    auto const request_size = layout.request_size();
    auto const initial_rect = layout.placement_rect(output_rect());
    auto const exclusive = 12;

    zwlr_layer_surface_v1_set_anchor(layer_surface, layout);
    invoke_zwlr_layer_surface_v1_set_margin(layer_surface, layout.margin);
    zwlr_layer_surface_v1_set_size(layer_surface, request_size.first, request_size.second);
    commit_and_wait_for_configure();
    surface.attach_visible_buffer(initial_rect.second.first, initial_rect.second.second);

    // Create layer surfaces with exclusive zones on the top and left of the output to push our surface out of the way

    wlcs::Surface top_surface{client};
    wlcs::LayerSurfaceV1 top_layer_surface{client, top_surface};
    zwlr_layer_surface_v1_set_anchor(top_layer_surface, ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP);
    zwlr_layer_surface_v1_set_exclusive_zone(top_layer_surface, exclusive);
    zwlr_layer_surface_v1_set_size(top_layer_surface, exclusive, exclusive);
    wl_surface_commit(top_surface);
    top_layer_surface.dispatch_until_configure();
    top_surface.attach_visible_buffer(exclusive, exclusive);
    wl_surface_commit(top_surface);

    wlcs::Surface left_surface{client};
    wlcs::LayerSurfaceV1 left_layer_surface{client, left_surface};
    zwlr_layer_surface_v1_set_anchor(left_layer_surface, ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);
    zwlr_layer_surface_v1_set_exclusive_zone(left_layer_surface, exclusive);
    zwlr_layer_surface_v1_set_size(left_layer_surface, exclusive, exclusive);
    wl_surface_commit(left_surface);
    left_layer_surface.dispatch_until_configure();
    left_surface.attach_visible_buffer(exclusive, exclusive);
    wl_surface_commit(left_surface);

    client.roundtrip();

    Rect non_exlusive_zone = output_rect();
    non_exlusive_zone.first.first += exclusive; // left
    non_exlusive_zone.first.second += exclusive; // top
    non_exlusive_zone.second.first -= exclusive; // width
    non_exlusive_zone.second.second -= exclusive; // height

    auto expected_config_size = layout.configure_size(non_exlusive_zone);
    if (expected_config_size.first)
    {
        EXPECT_THAT(configured_size().first, Eq(expected_config_size.first));
    }
    if (expected_config_size.second)
    {
        EXPECT_THAT(configured_size().second, Eq(expected_config_size.second));
    }

    auto const expected_placement = layout.placement_rect(non_exlusive_zone);
    surface.attach_visible_buffer(expected_placement.second.first, expected_placement.second.second);
    expect_surface_is_at_position(expected_placement.first);
}

TEST_P(LayerSurfaceLayoutTest, maximized_xdg_toplevel_is_shrunk_for_exclusive_zone)
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

    auto const layout = GetParam();
    auto const request_size = layout.request_size();
    auto const rect = layout.placement_rect(output_rect());
    zwlr_layer_surface_v1_set_anchor(layer_surface, layout);
    invoke_zwlr_layer_surface_v1_set_margin(layer_surface, layout.margin);
    zwlr_layer_surface_v1_set_exclusive_zone(layer_surface, exclusive_zone);
    zwlr_layer_surface_v1_set_size(layer_surface, request_size.first, request_size.second);
    commit_and_wait_for_configure();
    surface.attach_visible_buffer(rect.second.first, rect.second.second);

    int const new_width = width;
    int const new_height = height;

    int expected_width = initial_width;
    int expected_height = initial_height;

    switch (layout.attached_edge())
    {
    case ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT:
        expected_width -= exclusive_zone + layout.margin.left;
        break;

    case ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT:
        expected_width -= exclusive_zone + layout.margin.right;
        break;

    case ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP:
        expected_height -= exclusive_zone + layout.margin.top;
        break;

    case ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM:
        expected_height -= exclusive_zone + layout.margin.bottom;
        break;

    default: ;
    }

    EXPECT_THAT(new_width, Eq(expected_width));
    EXPECT_THAT(new_height, Eq(expected_height));
}

TEST_P(LayerSurfaceLayoutTest, simple_popup_positioned_correctly)
{
    auto const layout = GetParam();
    auto const output = output_rect();
    auto const layer_surface_rect = layout.placement_rect(output);
    auto const layer_surface_request_size = layout.request_size();

    zwlr_layer_surface_v1_set_anchor(layer_surface, layout);
    invoke_zwlr_layer_surface_v1_set_margin(layer_surface, layout.margin);
    zwlr_layer_surface_v1_set_size(layer_surface, layer_surface_request_size.first, layer_surface_request_size.second);
    commit_and_wait_for_configure();
    surface.attach_visible_buffer(layer_surface_rect.second.first, layer_surface_rect.second.second);

    auto const popup_size = std::make_pair(30, 30);
    wlcs::XdgPositionerStable positioner{client};
    xdg_positioner_set_size(positioner, popup_size.first, popup_size.second);
    xdg_positioner_set_anchor_rect(
        positioner,
        5, 5,
        layer_surface_rect.second.first - 10, layer_surface_rect.second.second - 10);
    xdg_positioner_set_anchor(positioner,  0);
    xdg_positioner_set_gravity(positioner, XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT);

    wlcs::Surface popup_wl_surface{client};
    wlcs::XdgSurfaceStable popup_xdg_surface{client, popup_wl_surface};
    wlcs::XdgPopupStable popup_xdg_popup{popup_xdg_surface, std::experimental::nullopt, positioner};
    zwlr_layer_surface_v1_get_popup(layer_surface, popup_xdg_popup);

    int popup_surface_configure_count = 0;
    Vec2 popup_configured_position;
    popup_xdg_surface.add_configure_notification([&](uint32_t serial)
        {
            xdg_surface_ack_configure(popup_xdg_surface, serial);
            popup_surface_configure_count++;
        });
    popup_xdg_popup.add_configure_notification([&](int32_t x, int32_t y, int32_t, int32_t)
        {
            popup_configured_position.first = x;
            popup_configured_position.second = y;
        });

    popup_wl_surface.attach_visible_buffer(popup_size.first, popup_size.second);
    client.dispatch_until([&](){ return popup_surface_configure_count > 0; });

    EXPECT_THAT(
        popup_configured_position,
        Eq(Vec2{layer_surface_rect.second.first / 2, layer_surface_rect.second.second / 2}));

    expect_surface_is_at_position(
        Vec2{
            layer_surface_rect.first.first + layer_surface_rect.second.first / 2,
            layer_surface_rect.first.second + layer_surface_rect.second.second / 2},
        popup_wl_surface);
}

INSTANTIATE_TEST_SUITE_P(
    Anchor,
    LayerSurfaceLayoutTest,
    testing::ValuesIn(LayerSurfaceLayout::get_all()));

TEST_P(LayerSurfaceLayerTest, surface_on_lower_layer_is_initially_placed_below)
{
    auto const& param = GetParam();

    SurfaceOnLayer above{client, param.above};
    SurfaceOnLayer below{client, param.below};

    the_server().move_surface_to(above.surface, 100, 0);
    the_server().move_surface_to(below.surface, 0, 0);

    auto pointer = the_server().create_pointer();
    pointer.move_to(1, 1);
    client.roundtrip();

    ASSERT_THAT(client.window_under_cursor(), Eq((wl_surface*)below.surface))
        << "Test could not run because below surface was not detected when above surface was not in the way";

    the_server().move_surface_to(above.surface, 0, 0);
    the_server().move_surface_to(below.surface, 0, 0);

    pointer.move_to(2, 3);
    client.roundtrip();

    ASSERT_THAT(client.window_under_cursor(), Ne((wl_surface*)below.surface))
        << "Wrong wl_surface was on top";
    ASSERT_THAT(client.window_under_cursor(), Eq((wl_surface*)above.surface))
        << "Correct surface was not on top";
}

TEST_P(LayerSurfaceLayerTest, below_surface_can_not_be_raised_with_click)
{
    auto const& param = GetParam();

    SurfaceOnLayer above{client, param.above};
    SurfaceOnLayer below{client, param.below};

    the_server().move_surface_to(above.surface, width / 2, 0);
    the_server().move_surface_to(below.surface, 0, 0);

    auto pointer = the_server().create_pointer();
    pointer.move_to(1, 1);
    client.roundtrip();

    ASSERT_THAT(client.window_under_cursor(), Eq((wl_surface*)below.surface))
        << "Test could not run because below surface was not detected and clicked on";

    pointer.left_button_down();
    client.roundtrip();
    pointer.left_button_up();
    client.roundtrip();
    pointer.move_to(width / 2 + 2, 1);
    client.roundtrip();

    ASSERT_THAT(client.window_under_cursor(), Ne((wl_surface*)below.surface))
        << "Wrong wl_surface was on top";
    ASSERT_THAT(client.window_under_cursor(), Eq((wl_surface*)above.surface))
        << "Correct surface was not on top";
}

TEST_P(LayerSurfaceLayerTest, surface_can_be_moved_to_layer)
{
    client.bind_if_supported<zwlr_layer_shell_v1>(wlcs::AtLeastVersion{ZWLR_LAYER_SURFACE_V1_SET_LAYER_SINCE_VERSION});
    auto const& param = GetParam();

    auto initial_below{param.below}, initial_above{param.above};
    if (initial_below)
    {
        initial_below = ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY;
    }
    if (initial_above)
    {
        initial_above = ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND;
    }
    SurfaceOnLayer above{client, initial_above};
    SurfaceOnLayer below{client, initial_below};

    client.roundtrip();

    if (above.layer_surface)
    {
        zwlr_layer_surface_v1_set_layer(above.layer_surface.value(), param.above.value());
    }
    if (below.layer_surface)
    {
        zwlr_layer_surface_v1_set_layer(below.layer_surface.value(), param.below.value());
    }

    wl_surface_commit(above.surface);
    wl_surface_commit(below.surface);

    auto pointer = the_server().create_pointer();
    the_server().move_surface_to(above.surface, 0, 0);
    the_server().move_surface_to(below.surface, 0, 0);
    pointer.move_to(2, 3);

    client.roundtrip();

    ASSERT_THAT(client.window_under_cursor(), Ne((wl_surface*)below.surface))
        << "Wrong wl_surface was on top";
    ASSERT_THAT(client.window_under_cursor(), Eq((wl_surface*)above.surface))
        << "Correct surface was not on top";
}

INSTANTIATE_TEST_SUITE_P(
    Layer,
    LayerSurfaceLayerTest,
    testing::Values(
        LayerLayerParams{ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM},
        LayerLayerParams{ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM, ZWLR_LAYER_SHELL_V1_LAYER_TOP},
        LayerLayerParams{ZWLR_LAYER_SHELL_V1_LAYER_TOP, ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY},
        LayerLayerParams{ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY},
        LayerLayerParams{ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM, ZWLR_LAYER_SHELL_V1_LAYER_TOP},
        LayerLayerParams{ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, std::experimental::nullopt},
        LayerLayerParams{ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM, std::experimental::nullopt},
        LayerLayerParams{std::experimental::nullopt, ZWLR_LAYER_SHELL_V1_LAYER_TOP},
        LayerLayerParams{std::experimental::nullopt, ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY}
    ));

// TODO: test it gets put on a specified output
// TODO: test margin
// TODO: test keyboard interactivity
