/*
 * Copyright © 2012 Intel Corporation
 * Copyright © 2013 Collabora, Ltd.
 * Copyright © 2018 Canonical Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "helpers.h"
#include "in_process_server.h"
#include "xdg_shell_stable.h"
#include "xdg_shell_v6.h"
#include "layer_shell_v1.h"
#include "version_specifier.h"

#include <gmock/gmock.h>

#include <optional>

using namespace testing;

int const window_width = 400, window_height = 500;
int const popup_width = 60, popup_height = 40;

namespace
{
uint32_t anchor_stable_to_v6(xdg_positioner_anchor anchor)
{
    switch (anchor)
    {
        case XDG_POSITIONER_ANCHOR_NONE:
            return ZXDG_POSITIONER_V6_ANCHOR_NONE;
        case XDG_POSITIONER_ANCHOR_TOP:
            return ZXDG_POSITIONER_V6_ANCHOR_TOP;
        case XDG_POSITIONER_ANCHOR_BOTTOM:
            return ZXDG_POSITIONER_V6_ANCHOR_BOTTOM;
        case XDG_POSITIONER_ANCHOR_LEFT:
            return ZXDG_POSITIONER_V6_ANCHOR_LEFT;
        case XDG_POSITIONER_ANCHOR_RIGHT:
            return ZXDG_POSITIONER_V6_ANCHOR_RIGHT;
        case XDG_POSITIONER_ANCHOR_TOP_LEFT:
            return ZXDG_POSITIONER_V6_ANCHOR_TOP | ZXDG_POSITIONER_V6_ANCHOR_LEFT;
        case XDG_POSITIONER_ANCHOR_BOTTOM_LEFT:
            return ZXDG_POSITIONER_V6_ANCHOR_BOTTOM | ZXDG_POSITIONER_V6_ANCHOR_LEFT;
        case XDG_POSITIONER_ANCHOR_TOP_RIGHT:
            return ZXDG_POSITIONER_V6_ANCHOR_TOP | ZXDG_POSITIONER_V6_ANCHOR_RIGHT;
        case XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT:
            return ZXDG_POSITIONER_V6_ANCHOR_BOTTOM | ZXDG_POSITIONER_V6_ANCHOR_RIGHT;
        default:
            return ZXDG_POSITIONER_V6_ANCHOR_NONE;
    }
}

uint32_t gravity_stable_to_v6(xdg_positioner_gravity gravity)
{
    switch (gravity)
    {
        case XDG_POSITIONER_GRAVITY_NONE:
            return ZXDG_POSITIONER_V6_GRAVITY_NONE;
        case XDG_POSITIONER_GRAVITY_TOP:
            return ZXDG_POSITIONER_V6_GRAVITY_TOP;
        case XDG_POSITIONER_GRAVITY_BOTTOM:
            return ZXDG_POSITIONER_V6_GRAVITY_BOTTOM;
        case XDG_POSITIONER_GRAVITY_LEFT:
            return ZXDG_POSITIONER_V6_GRAVITY_LEFT;
        case XDG_POSITIONER_GRAVITY_RIGHT:
            return ZXDG_POSITIONER_V6_GRAVITY_RIGHT;
        case XDG_POSITIONER_GRAVITY_TOP_LEFT:
            return ZXDG_POSITIONER_V6_GRAVITY_TOP | ZXDG_POSITIONER_V6_GRAVITY_LEFT;
        case XDG_POSITIONER_GRAVITY_BOTTOM_LEFT:
            return ZXDG_POSITIONER_V6_GRAVITY_BOTTOM | ZXDG_POSITIONER_V6_GRAVITY_LEFT;
        case XDG_POSITIONER_GRAVITY_TOP_RIGHT:
            return ZXDG_POSITIONER_V6_ANCHOR_TOP | ZXDG_POSITIONER_V6_ANCHOR_RIGHT;
        case XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT:
            return ZXDG_POSITIONER_V6_GRAVITY_BOTTOM | ZXDG_POSITIONER_V6_GRAVITY_RIGHT;
        default:
            return ZXDG_POSITIONER_V6_GRAVITY_NONE;
    }
}

uint32_t constraint_adjustment_stable_to_v6(xdg_positioner_constraint_adjustment ca)
{
    return ca; // the two enums have the same values
}

struct PositionerParams
{
    PositionerParams()
        : popup_size{popup_width, popup_height},
          anchor_rect{{0, 0}, {window_width, window_height}}
    {
    }

    auto with_size(int x, int y) -> PositionerParams& { popup_size = {x, y}; return *this; }
    auto with_anchor_rect(int x, int y, int w, int h) -> PositionerParams& { anchor_rect = {{x, y}, {w, h}}; return *this; }
    auto with_anchor(xdg_positioner_anchor value) -> PositionerParams& { anchor_stable = {value}; return *this; }
    auto with_gravity(xdg_positioner_gravity value) -> PositionerParams& { gravity_stable = {value}; return *this; }
    auto with_constraint_adjustment(uint32_t value) -> PositionerParams& { constraint_adjustment_stable = static_cast<xdg_positioner_constraint_adjustment>(value); return *this; }
    auto with_offset(int x, int y) -> PositionerParams& { offset = {{x, y}}; return *this; }
    auto with_grab(bool enable = true) -> PositionerParams& { grab = enable; return *this; }

    std::pair<int, int> popup_size; // will default to XdgPopupStableTestBase::popup_(width|height) if nullopt
    std::pair<std::pair<int, int>, std::pair<int, int>> anchor_rect; // will default to the full window rect
    std::optional<xdg_positioner_anchor> anchor_stable;
    std::optional<xdg_positioner_gravity> gravity_stable;
    std::optional<xdg_positioner_constraint_adjustment> constraint_adjustment_stable;
    std::optional<std::pair<int, int>> offset;
    bool grab{false};
};

struct PositionerTestParams
{
    PositionerTestParams(std::string name, int expected_x, int expected_y, PositionerParams const& positioner)
        : name{name},
          expected_positon{expected_x, expected_y},
          expected_size{popup_width, popup_height},
          positioner{positioner},
          parent_position_func{std::nullopt}
    {
    }

    PositionerTestParams(
        std::string name,
        int expected_x, int expected_y,
        int expected_width, int expected_height,
        PositionerParams const& positioner,
        std::function<std::pair<int, int>(int, int)> parent_position_func)
        : name{name},
          expected_positon{expected_x, expected_y},
          expected_size{expected_width, expected_height},
          positioner{positioner},
          parent_position_func{std::move(parent_position_func)}
    {
    }

    std::string name;
    std::pair<int, int> expected_positon;
    std::pair<int, int> expected_size;
    PositionerParams positioner;
    /// parent_position_func is called with the size of the output
    std::optional<std::function<std::pair<int, int>(int output_width, int output_height)>> parent_position_func;
};

std::ostream& operator<<(std::ostream& out, PositionerTestParams const& param)
{
    return out << param.name;
}

class XdgPopupManagerBase
{
public:
    struct State
    {
        int x;
        int y;
        int width;
        int height;
    };

    static int const window_x = 500, window_y = 500;

    XdgPopupManagerBase(wlcs::InProcessServer* const in_process_server)
        : the_server{in_process_server->the_server()},
          client{the_server},
          surface{client}
    {
        surface.add_frame_callback([this](auto) { surface_rendered = true; });
    }

    virtual ~XdgPopupManagerBase() = default;

    void wait_for_frame_to_render()
    {
        surface.attach_buffer(window_width, window_height);
        surface_rendered = false;
        wl_surface_commit(surface);
        client.dispatch_until([this]() { return surface_rendered; });
        the_server.move_surface_to(surface, window_x, window_y);
    }

    void map_popup(PositionerParams const& params)
    {
        popup_surface.emplace(client);
        setup_popup(params);
        wl_surface_commit(popup_surface.value());
        dispatch_until_popup_configure();
        popup_surface.value().attach_buffer(params.popup_size.first, params.popup_size.second);
        bool surface_rendered{false};
        popup_surface.value().add_frame_callback([&surface_rendered](auto) { surface_rendered = true; });
        wl_surface_commit(popup_surface.value());
        client.dispatch_until([&surface_rendered]() { return surface_rendered; });
    }

    void set_parent_position(
        std::function<std::pair<int, int>(int output_width, int output_height)> const& parent_position_func)
    {
        auto const output_size = client.output_state(0).mode_size.value();
        auto const parent_position = parent_position_func(output_size.first, output_size.second);
        the_server.move_surface_to(surface, parent_position.first, parent_position.second);
        client.roundtrip();
    }

    void unmap_popup()
    {
        clear_popup();
        popup_surface = std::nullopt;
    }

    virtual void dispatch_until_popup_configure() = 0;

    MOCK_METHOD0(popup_done, void());

    wlcs::Server& the_server;

    wlcs::Client client;
    wlcs::Surface surface;
    std::optional<wlcs::Surface> popup_surface;

    std::optional<State> state;

protected:
    virtual void setup_popup(PositionerParams const& params) = 0;
    virtual void clear_popup() = 0;

    bool surface_rendered{true};
};

class XdgPopupStableManager : public XdgPopupManagerBase
{
public:
    XdgPopupStableManager(wlcs::InProcessServer* const in_process_server)
        : XdgPopupManagerBase{in_process_server},
          xdg_shell_surface{client, surface},
          toplevel{xdg_shell_surface}
    {
        wait_for_frame_to_render();
    }

    void dispatch_until_popup_configure() override
    {
        client.dispatch_until(
            [prev_count = popup_surface_configure_count, &current_count = popup_surface_configure_count]()
            {
                return current_count > prev_count;
            });
    }

    void setup_popup(PositionerParams const& param) override
    {
        wlcs::XdgPositionerStable positioner{client};

        // size must always be set
        xdg_positioner_set_size(positioner, param.popup_size.first, param.popup_size.second);

        // anchor rect must always be set
        xdg_positioner_set_anchor_rect(
            positioner,
            param.anchor_rect.first.first,
            param.anchor_rect.first.second,
            param.anchor_rect.second.first,
            param.anchor_rect.second.second);

        if (param.anchor_stable)
            xdg_positioner_set_anchor(positioner,  param.anchor_stable.value());

        if (param.gravity_stable)
            xdg_positioner_set_gravity(positioner, param.gravity_stable.value());

        if (param.constraint_adjustment_stable)
            xdg_positioner_set_constraint_adjustment(positioner, param.constraint_adjustment_stable.value());

        if (param.offset)
            xdg_positioner_set_offset(positioner, param.offset.value().first, param.offset.value().second);

        popup_xdg_surface.emplace(client, popup_surface.value());

        popup.emplace(popup_xdg_surface.value(), &xdg_shell_surface, positioner);
        if (param.grab)
        {
            if (!client.latest_serial())
            {
                BOOST_THROW_EXCEPTION(std::runtime_error("client does not have a serial"));
            }
            xdg_popup_grab(popup.value().popup, client.seat(), client.latest_serial().value());
        }

        ON_CALL(popup_xdg_surface.value(), configure).WillByDefault([&](auto serial)
            {
                xdg_surface_ack_configure(popup_xdg_surface.value(), serial);
                popup_surface_configure_count++;
            });
        ON_CALL(popup.value(), configure).WillByDefault([&](auto... args)
            {
                state = State{args...};
            });
        ON_CALL(popup.value(), done()).WillByDefault([this](){ popup_done(); });
    }

    void clear_popup() override
    {
        popup = std::nullopt;
        popup_xdg_surface = std::nullopt;
    }

    wlcs::XdgSurfaceStable xdg_shell_surface;
    wlcs::XdgToplevelStable toplevel;

    std::optional<wlcs::XdgSurfaceStable> popup_xdg_surface;
    std::optional<wlcs::XdgPopupStable> popup;

    int popup_surface_configure_count{0};
};

class XdgPopupV6Manager : public XdgPopupManagerBase
{
public:
    XdgPopupV6Manager(wlcs::InProcessServer* const in_process_server)
        : XdgPopupManagerBase{in_process_server},
          xdg_shell_surface{client, surface},
          toplevel{xdg_shell_surface}
    {
        wait_for_frame_to_render();
    }

    void dispatch_until_popup_configure() override
    {
        client.dispatch_until(
            [prev_count = popup_surface_configure_count, &current_count = popup_surface_configure_count]()
            {
                return current_count > prev_count;
            });
    }

    void setup_popup(PositionerParams const& param) override
    {
        wlcs::XdgPositionerV6 positioner{client};

        // size must always be set
        zxdg_positioner_v6_set_size(positioner, param.popup_size.first, param.popup_size.second);

        // anchor rect must always be set
        zxdg_positioner_v6_set_anchor_rect(
            positioner,
            param.anchor_rect.first.first,
            param.anchor_rect.first.second,
            param.anchor_rect.second.first,
            param.anchor_rect.second.second);

        if (param.anchor_stable)
        {
            uint32_t v6_anchor = anchor_stable_to_v6(param.anchor_stable.value());
            zxdg_positioner_v6_set_anchor(positioner, v6_anchor);
        }

        if (param.gravity_stable)
        {
            uint32_t v6_gravity = gravity_stable_to_v6(param.gravity_stable.value());
            zxdg_positioner_v6_set_gravity(positioner, v6_gravity);
        }

        if (param.constraint_adjustment_stable)
        {
            uint32_t v6_constraint_adjustment =
                constraint_adjustment_stable_to_v6(param.constraint_adjustment_stable.value());
            zxdg_positioner_v6_set_constraint_adjustment(positioner, v6_constraint_adjustment);
        }

        if (param.offset)
        {
            zxdg_positioner_v6_set_offset(positioner, param.offset.value().first, param.offset.value().second);
        }


        popup_xdg_surface.emplace(client, popup_surface.value());
        popup.emplace(popup_xdg_surface.value(), xdg_shell_surface, positioner);
        if (param.grab)
        {
            if (!client.latest_serial())
            {
                BOOST_THROW_EXCEPTION(std::runtime_error("client does not have a serial"));
            }
            zxdg_popup_v6_grab(popup.value().popup, client.seat(), client.latest_serial().value());
        }

        ON_CALL(popup.value(), done).WillByDefault([this](){ popup_done(); });
        ON_CALL(popup_xdg_surface.value(), configure).WillByDefault([&](auto serial)
            {
                zxdg_surface_v6_ack_configure(popup_xdg_surface.value(), serial);
                popup_surface_configure_count++;
            });
        ON_CALL(popup.value(), configure).WillByDefault([this](auto... args)
            {
                state = State{args...};
            });
    }

    void clear_popup() override
    {
        popup = std::nullopt;
        popup_xdg_surface = std::nullopt;
    }

    wlcs::XdgSurfaceV6 xdg_shell_surface;
    wlcs::XdgToplevelV6 toplevel;

    std::optional<wlcs::XdgSurfaceV6> popup_xdg_surface;
    std::optional<wlcs::XdgPopupV6> popup;

    int popup_surface_configure_count{0};
};

class LayerV1PopupManager : public XdgPopupManagerBase
{
public:
    LayerV1PopupManager(wlcs::InProcessServer* const in_process_server)
        : XdgPopupManagerBase{in_process_server},
          layer_surface{client, surface}
    {
        {
            wlcs::Client client{in_process_server->the_server()};
            auto const layer_shell = client.bind_if_supported<zwlr_layer_shell_v1>(
                wlcs::AtLeastVersion{ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND_SINCE_VERSION});
            client.roundtrip();
        }
        zwlr_layer_surface_v1_set_size(layer_surface, window_width, window_height);
        zwlr_layer_surface_v1_set_keyboard_interactivity(
            layer_surface,
            ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND);
        wait_for_frame_to_render();
    }

    void dispatch_until_popup_configure() override
    {
        client.dispatch_until(
            [prev_count = popup_surface_configure_count, &current_count = popup_surface_configure_count]()
            {
                return current_count > prev_count;
            });
    }

    void setup_popup(PositionerParams const& param) override
    {
        wlcs::XdgPositionerStable positioner{client};

        // size must always be set
        xdg_positioner_set_size(positioner, param.popup_size.first, param.popup_size.second);

        // anchor rect must always be set
        xdg_positioner_set_anchor_rect(
            positioner,
            param.anchor_rect.first.first,
            param.anchor_rect.first.second,
            param.anchor_rect.second.first,
            param.anchor_rect.second.second);

        if (param.anchor_stable)
            xdg_positioner_set_anchor(positioner,  param.anchor_stable.value());

        if (param.gravity_stable)
            xdg_positioner_set_gravity(positioner, param.gravity_stable.value());

        if (param.constraint_adjustment_stable)
            xdg_positioner_set_constraint_adjustment(positioner, param.constraint_adjustment_stable.value());

        if (param.offset)
            xdg_positioner_set_offset(positioner, param.offset.value().first, param.offset.value().second);


        popup_xdg_surface.emplace(client, popup_surface.value());
        popup.emplace(popup_xdg_surface.value(), std::nullopt, positioner);
        zwlr_layer_surface_v1_get_popup(layer_surface, popup.value());
        if (param.grab)
        {
            if (!client.latest_serial())
            {
                BOOST_THROW_EXCEPTION(std::runtime_error("client does not have a serial"));
            }
            xdg_popup_grab(popup.value().popup, client.seat(), client.latest_serial().value());
        }

        ON_CALL(popup_xdg_surface.value(), configure).WillByDefault([&](uint32_t serial)
            {
                xdg_surface_ack_configure(popup_xdg_surface.value(), serial);
                popup_surface_configure_count++;
            });
        ON_CALL(popup.value(), configure).WillByDefault([this](auto... args)
            {
                state = State{args...};
            });
        ON_CALL(popup.value(), done()).WillByDefault([this](){ popup_done(); });
    }

    void clear_popup() override
    {
        popup = std::nullopt;
        popup_xdg_surface = std::nullopt;
    }

    wlcs::LayerSurfaceV1 layer_surface;

    std::optional<wlcs::XdgSurfaceStable> popup_xdg_surface;
    std::optional<wlcs::XdgPopupStable> popup;

    int popup_surface_configure_count{0};
};

}

class XdgPopupPositionerTest:
    public wlcs::StartedInProcessServer,
    public testing::WithParamInterface<PositionerTestParams>
{
};

TEST_P(XdgPopupPositionerTest, xdg_shell_stable_popup_placed_correctly)
{
    auto manager = std::make_unique<XdgPopupStableManager>(this);
    auto const& param = GetParam();

    if (param.parent_position_func)
    {
        manager->set_parent_position(param.parent_position_func.value());
    }
    manager->map_popup(param.positioner);

    ASSERT_THAT(
        manager->state,
        Ne(std::nullopt)) << "popup configure event not sent";

    EXPECT_THAT(
        std::make_pair(manager->state.value().x, manager->state.value().y),
        Eq(param.expected_positon)) << "popup placed in incorrect position";

    EXPECT_THAT(
        std::make_pair(manager->state.value().width, manager->state.value().height),
        Eq(param.expected_size)) << "popup has incorrect size";
}

TEST_P(XdgPopupPositionerTest, xdg_shell_unstable_v6_popup_placed_correctly)
{
    auto manager = std::make_unique<XdgPopupV6Manager>(this);
    auto const& param = GetParam();

    if (param.parent_position_func)
    {
        manager->set_parent_position(param.parent_position_func.value());
    }
    manager->map_popup(param.positioner);

    ASSERT_THAT(
        manager->state,
        Ne(std::nullopt)) << "popup configure event not sent";

    EXPECT_THAT(
        std::make_pair(manager->state.value().x, manager->state.value().y),
        Eq(param.expected_positon)) << "popup placed in incorrect position";

    EXPECT_THAT(
        std::make_pair(manager->state.value().width, manager->state.value().height),
        Eq(param.expected_size)) << "popup has incorrect size";
}

TEST_P(XdgPopupPositionerTest, layer_shell_popup_placed_correctly)
{
    auto manager = std::make_unique<LayerV1PopupManager>(this);
    auto const& param = GetParam();

    if (param.parent_position_func)
    {
        manager->set_parent_position(param.parent_position_func.value());
    }
    manager->map_popup(param.positioner);

    ASSERT_THAT(
        manager->state,
        Ne(std::nullopt)) << "popup configure event not sent";

    EXPECT_THAT(
        std::make_pair(manager->state.value().x, manager->state.value().y),
        Eq(param.expected_positon)) << "popup placed in incorrect position";

    EXPECT_THAT(
        std::make_pair(manager->state.value().width, manager->state.value().height),
        Eq(param.expected_size)) << "popup has incorrect size";
}

INSTANTIATE_TEST_SUITE_P(
    Default,
    XdgPopupPositionerTest,
    testing::Values(
        PositionerTestParams{"default values", (window_width - popup_width) / 2, (window_height - popup_height) / 2, PositionerParams()}
    ));

INSTANTIATE_TEST_SUITE_P(
    Anchor,
    XdgPopupPositionerTest,
    testing::Values(
        PositionerTestParams{"anchor left", -popup_width / 2, (window_height - popup_height) / 2,
            PositionerParams().with_anchor(XDG_POSITIONER_ANCHOR_LEFT)},

        PositionerTestParams{"anchor right", window_width - popup_width / 2, (window_height - popup_height) / 2,
            PositionerParams().with_anchor(XDG_POSITIONER_ANCHOR_RIGHT)},

        PositionerTestParams{"anchor top", (window_width - popup_width) / 2, -popup_height / 2,
            PositionerParams().with_anchor(XDG_POSITIONER_ANCHOR_TOP)},

        PositionerTestParams{"anchor bottom", (window_width - popup_width) / 2, window_height - popup_height / 2,
            PositionerParams().with_anchor(XDG_POSITIONER_ANCHOR_BOTTOM)},

        PositionerTestParams{"anchor top left", -popup_width / 2, -popup_height / 2,
            PositionerParams().with_anchor(XDG_POSITIONER_ANCHOR_TOP_LEFT)},

        PositionerTestParams{"anchor top right", window_width - popup_width / 2, -popup_height / 2,
            PositionerParams().with_anchor(XDG_POSITIONER_ANCHOR_TOP_RIGHT)},

        PositionerTestParams{"anchor bottom left", -popup_width / 2, window_height - popup_height / 2,
            PositionerParams().with_anchor(XDG_POSITIONER_ANCHOR_BOTTOM_LEFT)},

        PositionerTestParams{"anchor bottom right", window_width - popup_width / 2, window_height - popup_height / 2,
            PositionerParams().with_anchor(XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT)}
    ));

INSTANTIATE_TEST_SUITE_P(
    Gravity,
    XdgPopupPositionerTest,
    testing::Values(
        PositionerTestParams{"gravity none", (window_width - popup_width) / 2, (window_height - popup_height) / 2,
            PositionerParams().with_gravity(XDG_POSITIONER_GRAVITY_NONE)},

        PositionerTestParams{"gravity left", window_width / 2 - popup_width, (window_height - popup_height) / 2,
            PositionerParams().with_gravity(XDG_POSITIONER_GRAVITY_LEFT)},

        PositionerTestParams{"gravity right", window_width / 2, (window_height - popup_height) / 2,
            PositionerParams().with_gravity(XDG_POSITIONER_GRAVITY_RIGHT)},

        PositionerTestParams{"gravity top", (window_width - popup_width) / 2, window_height / 2 - popup_height,
            PositionerParams().with_gravity(XDG_POSITIONER_GRAVITY_TOP)},

        PositionerTestParams{"gravity bottom", (window_width - popup_width) / 2, window_height / 2,
            PositionerParams().with_gravity(XDG_POSITIONER_GRAVITY_BOTTOM)},

        PositionerTestParams{"gravity top left", window_width / 2 - popup_width, window_height / 2 - popup_height,
            PositionerParams().with_gravity(XDG_POSITIONER_GRAVITY_TOP_LEFT)},

        PositionerTestParams{"gravity top right", window_width / 2, window_height / 2 - popup_height,
            PositionerParams().with_gravity(XDG_POSITIONER_GRAVITY_TOP_RIGHT)},

        PositionerTestParams{"gravity bottom left", window_width / 2 - popup_width, window_height / 2,
            PositionerParams().with_gravity(XDG_POSITIONER_GRAVITY_BOTTOM_LEFT)},

        PositionerTestParams{"gravity bottom right", window_width / 2, window_height / 2,
            PositionerParams().with_gravity(XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT)}
    ));

INSTANTIATE_TEST_SUITE_P(
    AnchorRect,
    XdgPopupPositionerTest,
    testing::Values(
        PositionerTestParams{"explicit defaultPositionerParams anchor rect", (window_width - popup_width) / 2, (window_height - popup_height) / 2,
            PositionerParams().with_anchor_rect(0, 0, window_width, window_height)},

        PositionerTestParams{"upper left anchor rect", (window_width - 40 - popup_width) / 2, (window_height - 30 - popup_height) / 2,
            PositionerParams().with_anchor_rect(0, 0, window_width - 40, window_height - 30)},

        PositionerTestParams{"upper right anchor rect", (window_width + 40 - popup_width) / 2, (window_height - 30 - popup_height) / 2,
            PositionerParams().with_anchor_rect(40, 0, window_width - 40, window_height - 30)},

        PositionerTestParams{"lower left anchor rect", (window_width - 40 - popup_width) / 2, (window_height + 30 - popup_height) / 2,
            PositionerParams().with_anchor_rect(0, 30, window_width - 40, window_height - 30)},

        PositionerTestParams{"lower right anchor rect", (window_width + 40 - popup_width) / 2, (window_height + 30 - popup_height) / 2,
            PositionerParams().with_anchor_rect(40, 30, window_width - 40, window_height - 30)},

        PositionerTestParams{"offset anchor rect", (window_width - 40 - popup_width) / 2, (window_height - 80 - popup_height) / 2,
            PositionerParams().with_anchor_rect(20, 20, window_width - 80, window_height - 120)}
    ));

INSTANTIATE_TEST_SUITE_P(
    ConstraintAdjustmentNone,
    XdgPopupPositionerTest,
    testing::Values(
        PositionerTestParams{"middle of screen",
            -popup_width, -popup_height,
            popup_width, popup_height,
            PositionerParams()
                .with_anchor(XDG_POSITIONER_ANCHOR_TOP_LEFT)
                .with_gravity(XDG_POSITIONER_GRAVITY_TOP_LEFT)
                .with_constraint_adjustment(XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_NONE),
            [](int width, int height){ return std::make_pair((width - window_width) / 2, (height - window_height) / 2); }},
        PositionerTestParams{"off top left edge",
            -popup_width, -popup_height,
            popup_width, popup_height,
            PositionerParams()
                .with_anchor(XDG_POSITIONER_ANCHOR_TOP_LEFT)
                .with_gravity(XDG_POSITIONER_GRAVITY_TOP_LEFT)
                .with_constraint_adjustment(XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_NONE),
            [](int /*width*/, int /*height*/){ return std::make_pair(5, 5); }},
        PositionerTestParams{"off top right edge",
            window_width, -popup_height,
            popup_width, popup_height,
            PositionerParams()
                .with_anchor(XDG_POSITIONER_ANCHOR_TOP_RIGHT)
                .with_gravity(XDG_POSITIONER_GRAVITY_TOP_RIGHT)
                .with_constraint_adjustment(XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_NONE),
            [](int width, int /*height*/){ return std::make_pair(width - window_width - 5, 5); }},
        PositionerTestParams{"off bottom left edge",
            -popup_width, window_height,
            popup_width, popup_height,
            PositionerParams()
                .with_anchor(XDG_POSITIONER_ANCHOR_BOTTOM_LEFT)
                .with_gravity(XDG_POSITIONER_GRAVITY_BOTTOM_LEFT)
                .with_constraint_adjustment(XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_NONE),
            [](int /*width*/, int height){ return std::make_pair(5, height - window_height - 5); }},
        PositionerTestParams{"off bottom right edge",
            window_width, window_height,
            popup_width, popup_height,
            PositionerParams()
                .with_anchor(XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT)
                .with_gravity(XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT)
                .with_constraint_adjustment(XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_NONE),
            [](int width, int height){ return std::make_pair(width - window_width - 5, height - window_height - 5); }}
    ));

INSTANTIATE_TEST_SUITE_P(
    ConstraintAdjustmentSlide,
    XdgPopupPositionerTest,
    testing::Values(
        PositionerTestParams{"middle of screen",
            -popup_width, -popup_height,
            popup_width, popup_height,
            PositionerParams()
                .with_anchor(XDG_POSITIONER_ANCHOR_TOP_LEFT)
                .with_gravity(XDG_POSITIONER_GRAVITY_TOP_LEFT)
                .with_constraint_adjustment(
                    XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X | XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y),
            [](int width, int height){ return std::make_pair((width - window_width) / 2, (height - window_height) / 2); }},
        PositionerTestParams{"off top left edge",
            -5, -5,
            popup_width, popup_height,
            PositionerParams()
                .with_anchor(XDG_POSITIONER_ANCHOR_TOP_LEFT)
                .with_gravity(XDG_POSITIONER_GRAVITY_TOP_LEFT)
                .with_constraint_adjustment(
                    XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X | XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y),
            [](int /*width*/, int /*height*/){ return std::make_pair(5, 5); }},
        PositionerTestParams{"off top right edge",
            window_width - popup_width + 5, -5,
            popup_width, popup_height,
            PositionerParams()
                .with_anchor(XDG_POSITIONER_ANCHOR_TOP_RIGHT)
                .with_gravity(XDG_POSITIONER_GRAVITY_TOP_RIGHT)
                .with_constraint_adjustment(
                    XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X | XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y),
            [](int width, int /*height*/){ return std::make_pair(width - window_width - 5, 5); }},
        PositionerTestParams{"off bottom left edge", -5,
            window_height - popup_height + 5,
            popup_width, popup_height,
            PositionerParams()
                .with_anchor(XDG_POSITIONER_ANCHOR_BOTTOM_LEFT)
                .with_gravity(XDG_POSITIONER_GRAVITY_BOTTOM_LEFT)
                .with_constraint_adjustment(
                    XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X | XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y),
            [](int /*width*/, int height){ return std::make_pair(5, height - window_height - 5); }},
        PositionerTestParams{"off bottom right edge",
            window_width - popup_width + 5, window_height - popup_height + 5,
            popup_width, popup_height,
            PositionerParams()
                .with_anchor(XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT)
                .with_gravity(XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT)
                .with_constraint_adjustment(
                    XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X | XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y),
            [](int width, int height){ return std::make_pair(width - window_width - 5, height - window_height - 5); }}
    ));

INSTANTIATE_TEST_SUITE_P(
    ConstraintAdjustmentFlip,
    XdgPopupPositionerTest,
    testing::Values(
        PositionerTestParams{"middle of screen",
            -popup_width, -popup_height,
            popup_width, popup_height,
            PositionerParams()
                .with_anchor(XDG_POSITIONER_ANCHOR_TOP_LEFT)
                .with_gravity(XDG_POSITIONER_GRAVITY_TOP_LEFT)
                .with_constraint_adjustment(
                    XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_X | XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y),
            [](int width, int height){ return std::make_pair((width - window_width) / 2, (height - window_height) / 2); }},
        PositionerTestParams{"off top left edge",
            window_width, window_height,
            popup_width, popup_height,
            PositionerParams()
                .with_anchor(XDG_POSITIONER_ANCHOR_TOP_LEFT)
                .with_gravity(XDG_POSITIONER_GRAVITY_TOP_LEFT)
                .with_constraint_adjustment(
                    XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_X | XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y),
            [](int /*width*/, int /*height*/){ return std::make_pair(5, 5); }},
        PositionerTestParams{"off top right edge",
            -popup_width, window_height,
            popup_width, popup_height,
            PositionerParams()
                .with_anchor(XDG_POSITIONER_ANCHOR_TOP_RIGHT)
                .with_gravity(XDG_POSITIONER_GRAVITY_TOP_RIGHT)
                .with_constraint_adjustment(
                    XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_X | XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y),
            [](int width, int /*height*/){ return std::make_pair(width - window_width - 5, 5); }},
        PositionerTestParams{"off bottom left edge",
            window_width, -popup_height,
            popup_width, popup_height,
            PositionerParams()
                .with_anchor(XDG_POSITIONER_ANCHOR_BOTTOM_LEFT)
                .with_gravity(XDG_POSITIONER_GRAVITY_BOTTOM_LEFT)
                .with_constraint_adjustment(
                    XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_X | XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y),
            [](int /*width*/, int height){ return std::make_pair(5, height - window_height - 5); }},
        PositionerTestParams{"off bottom right edge",
            -popup_width, -popup_height,
            popup_width, popup_height,
            PositionerParams()
                .with_anchor(XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT)
                .with_gravity(XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT)
                .with_constraint_adjustment(
                    XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_X | XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y),
            [](int width, int height){ return std::make_pair(width - window_width - 5, height - window_height - 5); }}
    ));

INSTANTIATE_TEST_SUITE_P(
    ConstraintAdjustmentResize,
    XdgPopupPositionerTest,
    testing::Values(
        PositionerTestParams{"middle of screen",
            -popup_width, -popup_height,
            5, 5,
            PositionerParams()
                .with_anchor(XDG_POSITIONER_ANCHOR_TOP_LEFT)
                .with_gravity(XDG_POSITIONER_GRAVITY_TOP_LEFT)
                .with_constraint_adjustment(
                    XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_X | XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_Y),
            [](int width, int height){ return std::make_pair((width - window_width) / 2, (height - window_height) / 2); }},
        PositionerTestParams{"off top left edge",
            -popup_width, -popup_height,
            5, 5,
            PositionerParams()
                .with_anchor(XDG_POSITIONER_ANCHOR_TOP_LEFT)
                .with_gravity(XDG_POSITIONER_GRAVITY_TOP_LEFT)
                .with_constraint_adjustment(
                    XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_X | XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_Y),
            [](int /*width*/, int /*height*/){ return std::make_pair(5, 5); }},
        PositionerTestParams{"off top right edge",
            window_width, -popup_height,
            5, 5,
            PositionerParams()
                .with_anchor(XDG_POSITIONER_ANCHOR_TOP_RIGHT)
                .with_gravity(XDG_POSITIONER_GRAVITY_TOP_RIGHT)
                .with_constraint_adjustment(
                    XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_X | XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_Y),
            [](int width, int /*height*/){ return std::make_pair(width - window_width - 5, 5); }},
        PositionerTestParams{"off bottom left edge",
            -popup_width, window_height,
            5, 5,
            PositionerParams()
                .with_anchor(XDG_POSITIONER_ANCHOR_BOTTOM_LEFT)
                .with_gravity(XDG_POSITIONER_GRAVITY_BOTTOM_LEFT)
                .with_constraint_adjustment(
                    XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_X | XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_Y),
            [](int /*width*/, int height){ return std::make_pair(5, height - window_height - 5); }},
        PositionerTestParams{"off bottom right edge",
            window_width, window_height,
            5, 5,
            PositionerParams()
                .with_anchor(XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT)
                .with_gravity(XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT)
                .with_constraint_adjustment(
                    XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_X | XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_Y),
            [](int width, int height){ return std::make_pair(width - window_width - 5, height - window_height - 5); }}
    ));

struct XdgPopupTestParam
{
    std::function<std::unique_ptr<XdgPopupManagerBase>(wlcs::InProcessServer* const)> build;
};

std::ostream& operator<<(std::ostream& out, XdgPopupTestParam const&) { return out; }

class XdgPopupTest:
    public wlcs::StartedInProcessServer,
    public testing::WithParamInterface<XdgPopupTestParam>
{
};

TEST_P(XdgPopupTest, pointer_focus_goes_to_popup)
{
    auto const& param = GetParam();
    auto manager = param.build(this);
    auto pointer = the_server().create_pointer();
    pointer.move_to(manager->window_x + 1, manager->window_y + 1);
    manager->client.roundtrip();

    EXPECT_THAT(manager->client.window_under_cursor(), Eq((wl_surface*)manager->surface));

    auto positioner = PositionerParams{}
        .with_size(30, 30)
        .with_anchor(XDG_POSITIONER_ANCHOR_TOP_LEFT)
        .with_gravity(XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT);
    manager->map_popup(positioner);
    manager->client.roundtrip();

    pointer.move_to(manager->window_x + 2, manager->window_y + 1);
    manager->client.roundtrip();

    EXPECT_THAT(manager->client.window_under_cursor(), Eq((wl_surface*)manager->popup_surface.value()));
}

TEST_P(XdgPopupTest, popup_gives_up_pointer_focus_when_gone)
{
    auto const& param = GetParam();
    auto manager = param.build(this);
    auto pointer = the_server().create_pointer();

    auto positioner = PositionerParams{}
        .with_size(30, 30)
        .with_anchor(XDG_POSITIONER_ANCHOR_TOP_LEFT)
        .with_gravity(XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT);
    manager->map_popup(positioner);
    manager->client.roundtrip();

    pointer.move_to(manager->window_x + 2, manager->window_y + 1);
    manager->client.roundtrip();

    EXPECT_THAT(manager->client.window_under_cursor(), Eq((wl_surface*)manager->popup_surface.value()));

    manager->unmap_popup();
    manager->client.roundtrip();
    pointer.move_to(manager->window_x + 3, manager->window_y + 1);
    manager->client.roundtrip();

    EXPECT_THAT(manager->client.window_under_cursor(), Eq((wl_surface*)manager->surface));
}

TEST_P(XdgPopupTest, grabbed_popup_gets_done_event_when_new_toplevel_created)
{
    auto const& param = GetParam();
    auto manager = param.build(this);
    auto pointer = the_server().create_pointer();

    // This is needed to get a serial, which will be used later on
    pointer.move_to(manager->window_x + 2, manager->window_y + 2);
    pointer.left_click();
    manager->client.roundtrip();

    auto positioner = PositionerParams{}
        .with_size(30, 30)
        .with_anchor(XDG_POSITIONER_ANCHOR_TOP_LEFT)
        .with_gravity(XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT)
        .with_grab();
    manager->map_popup(positioner);
    manager->client.roundtrip();

    EXPECT_CALL(*manager, popup_done());

    manager->client.create_visible_surface(window_width, window_height);
}

TEST_P(XdgPopupTest, grabbed_popup_gets_keyboard_focus)
{
    auto const& param = GetParam();
    auto manager = param.build(this);
    auto pointer = the_server().create_pointer();

    // This is needed to get a serial, which will be used later on
    pointer.move_to(manager->window_x + 2, manager->window_y + 2);
    pointer.left_click();
    manager->client.roundtrip();

    auto positioner = PositionerParams{}
        .with_size(30, 30)
        .with_anchor(XDG_POSITIONER_ANCHOR_TOP_LEFT)
        .with_gravity(XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT)
        .with_grab();
    manager->map_popup(positioner);
    manager->client.roundtrip();

    EXPECT_THAT(
        manager->client.keyboard_focused_window(),
        Eq(static_cast<wl_surface*>(manager->popup_surface.value())))
        << "grabbed popup not given keyboard focus";
}

TEST_P(XdgPopupTest, non_grabbed_popup_does_not_get_keyboard_focus)
{
    auto const& param = GetParam();
    auto manager = param.build(this);

    auto positioner = PositionerParams{}
        .with_size(30, 30)
        .with_anchor(XDG_POSITIONER_ANCHOR_TOP_LEFT)
        .with_gravity(XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT);
    manager->map_popup(positioner);
    manager->client.roundtrip();

    EXPECT_THAT(
        manager->client.keyboard_focused_window(),
        Ne(static_cast<wl_surface*>(manager->popup_surface.value())))
        << "popup given keyboard focus";
    EXPECT_THAT(
        manager->client.keyboard_focused_window(),
        Eq(static_cast<wl_surface*>(manager->surface)));
}

TEST_P(XdgPopupTest, does_not_get_popup_done_event_before_button_press)
{
    auto const& param = GetParam();
    auto manager = param.build(this);
    auto pointer = the_server().create_pointer();

    // This is needed to get a serial, which will be used later on
    pointer.move_to(manager->window_x + 2, manager->window_y + 2);
    pointer.left_click();
    manager->client.roundtrip();

    auto positioner = PositionerParams{}
        .with_size(30, 30)
        .with_anchor(XDG_POSITIONER_ANCHOR_TOP_LEFT)
        .with_gravity(XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT)
        .with_grab();
    manager->map_popup(positioner);
    manager->client.roundtrip();

    // This may or may not be sent, but a button press should not come in after it if it is sent
    bool got_popup_done = false;
    EXPECT_CALL(*manager, popup_done()).Times(AnyNumber()).WillRepeatedly([&]()
        {
            got_popup_done = true;
        });

    manager->client.add_pointer_button_notification([&](auto, auto, auto)
        {
            EXPECT_THAT(got_popup_done, IsFalse()) << "pointer button sent after popup done";
            return true;
        });

    pointer.move_to(manager->window_x + 32, manager->window_y + 32);
    pointer.left_click();
    manager->client.roundtrip();
}

TEST_F(XdgPopupTest, zero_size_anchor_rect_stable)
{
    auto manager = std::make_unique<XdgPopupStableManager>(this);

    auto positioner = PositionerParams{}
        .with_anchor_rect(window_width / 2, window_height / 2, 0, 0);

    manager->map_popup(positioner);
    manager->client.roundtrip();

    ASSERT_THAT(
        std::make_pair(manager->state.value().x, manager->state.value().y),
        Eq(std::make_pair(
            (window_width - popup_width) / 2,
            (window_height - popup_height) / 2))) << "popup placed in incorrect position";
}

// regression test for https://github.com/MirServer/mir/issues/836
TEST_P(XdgPopupTest, popup_configure_is_valid)
{
    auto const& param = GetParam();
    auto manager = param.build(this);

    auto positioner = PositionerParams{}
        .with_size(30, 30)
        .with_anchor(XDG_POSITIONER_ANCHOR_TOP_LEFT)
        .with_gravity(XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT);
    manager->map_popup(positioner);
    manager->client.roundtrip();

    ASSERT_THAT(manager->state, Ne(std::nullopt));
    EXPECT_THAT(manager->state.value().width, Gt(0));
    EXPECT_THAT(manager->state.value().height, Gt(0));
}

INSTANTIATE_TEST_SUITE_P(
    XdgPopupStable,
    XdgPopupTest,
    testing::Values(XdgPopupTestParam{
        [](wlcs::InProcessServer* const server)
        {
            return std::make_unique<XdgPopupStableManager>(server);
        }}));

INSTANTIATE_TEST_SUITE_P(
    XdgPopupUnstableV6,
    XdgPopupTest,
    testing::Values(XdgPopupTestParam{
        [](wlcs::InProcessServer* const server)
        {
            return std::make_unique<XdgPopupV6Manager>(server);
        }}));

INSTANTIATE_TEST_SUITE_P(
    LayerShellPopup,
    XdgPopupTest,
    testing::Values(XdgPopupTestParam{
        [](wlcs::InProcessServer* const server)
        {
            return std::make_unique<LayerV1PopupManager>(server);
        }}));

// TODO: test that positioner is always overlapping or adjacent to parent
// TODO: test that positioner is copied immediately after use
// TODO: test that error is raised when incomplete positioner is used (positioner without size and anchor rect set)
// TODO: test set_size
// TODO: test that set_window_geometry affects anchor rect
// TODO: test set_offset
// TODO: test that a zero size anchor rect fails on v6
