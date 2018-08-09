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

#include <gmock/gmock.h>

#include <experimental/optional>

using namespace testing;

int const window_width = 400, window_height = 500;
int const popup_width = 60, popup_height = 40;

class XdgPopupStableTestBase : public wlcs::StartedInProcessServer
{
public:
    static int const window_x = 500, window_y = 500;

    XdgPopupStableTestBase()
        : client{the_server()},
          surface{client},
          xdg_shell_surface{client, surface},
          toplevel{xdg_shell_surface},
          positioner{client}
    {
        surface.attach_buffer(window_width, window_height);
        bool surface_rendered{false};
        surface.add_frame_callback([&surface_rendered](auto) { surface_rendered = true; });
        wl_surface_commit(surface);
        client.dispatch_until([&surface_rendered]() { return surface_rendered; });
        the_server().move_surface_to(surface, window_x, window_y);
    }

    void map_popup()
    {
        popup_surface.emplace(client);
        popup_xdg_surface.emplace(client, popup_surface.value());
        popup.emplace(popup_xdg_surface.value(), xdg_shell_surface, positioner);

        popup_xdg_surface.value().add_configure_notification([&](uint32_t serial)
            {
                xdg_surface_ack_configure(popup_xdg_surface.value(), serial);
                popup_surface_configure_count++;
            });

        popup.value().add_configure_notification([this](int32_t x, int32_t y, int32_t width, int32_t height)
            {
                state = wlcs::XdgPopupStable::State{x, y, width, height};
            });

        wl_surface_commit(popup_surface.value());
        dispatch_until_popup_configure();
        popup_surface.value().attach_buffer(popup_width, popup_height);
        bool surface_rendered{false};
        popup_surface.value().add_frame_callback([&surface_rendered](auto) { surface_rendered = true; });
        wl_surface_commit(popup_surface.value());
        client.dispatch_until([&surface_rendered]() { return surface_rendered; });
    }

    void dispatch_until_popup_configure()
    {
        client.dispatch_until(
            [prev_count = popup_surface_configure_count, &current_count = popup_surface_configure_count]()
            {
                return current_count > prev_count;
            });
    }

    wlcs::Client client;
    wlcs::Surface surface;
    wlcs::XdgSurfaceStable xdg_shell_surface;
    wlcs::XdgToplevelStable toplevel;

    wlcs::XdgPositionerStable positioner;
    std::experimental::optional<wlcs::Surface> popup_surface;
    std::experimental::optional<wlcs::XdgSurfaceStable> popup_xdg_surface;
    std::experimental::optional<wlcs::XdgPopupStable> popup;

    int popup_surface_configure_count{0};
    std::experimental::optional<wlcs::XdgPopupStable::State> state;
};

struct PopupStableTestParams
{
    PopupStableTestParams(std::string name, int expected_x, int expected_y)
        : name{name},
          expected_positon{expected_x, expected_y},
          popup_size{popup_width, popup_height},
          anchor_rect{{0, 0}, {window_width, window_height}}
    {
    }

    PopupStableTestParams& with_size(int x, int y) { popup_size = {x, y}; return *this; }
    PopupStableTestParams& with_anchor_rect(int x, int y, int w, int h) { anchor_rect = {{x, y}, {w, h}}; return *this; }
    PopupStableTestParams& with_anchor(int value) { anchor = {static_cast<xdg_positioner_anchor>(value)}; return *this; }
    PopupStableTestParams& with_gravity(int value) { gravity = {static_cast<xdg_positioner_gravity>(value)}; return *this; }
    PopupStableTestParams& with_constraint_adjustment(int value)
    {
        constraint_adjustment = {static_cast<xdg_positioner_constraint_adjustment>(value)};
        return *this;
    }
    PopupStableTestParams& with_offset(int x, int y) { offset = {{x, y}}; return *this; }

    std::string name;
    std::pair<int, int> expected_positon;
    std::pair<int, int> popup_size; // will default to XdgPopupStableTestBase::popup_(width|height) if nullopt
    std::pair<std::pair<int, int>, std::pair<int, int>> anchor_rect; // will default to the full window rect
    std::experimental::optional<xdg_positioner_anchor> anchor;
    std::experimental::optional<xdg_positioner_gravity> gravity;
    std::experimental::optional<xdg_positioner_constraint_adjustment> constraint_adjustment;
    std::experimental::optional<std::pair<int, int>> offset;
};

std::ostream& operator<<(std::ostream& out, PopupStableTestParams const& param)
{
    return out << param.name;
}

class XdgPopupStableTest:
    public XdgPopupStableTestBase,
    public testing::WithParamInterface<PopupStableTestParams>
{
};

TEST_P(XdgPopupStableTest, positioner_places_popup_correctly)
{
    auto const& param = GetParam();

    // size must always be set
    xdg_positioner_set_size(positioner, param.popup_size.first, param.popup_size.second);

    // anchor rect must always be set
    xdg_positioner_set_anchor_rect(positioner,
                                   param.anchor_rect.first.first,
                                   param.anchor_rect.first.second,
                                   param.anchor_rect.second.first,
                                   param.anchor_rect.second.second);

    if (param.anchor)
        xdg_positioner_set_anchor(positioner,  param.anchor.value());

    if (param.gravity)
        xdg_positioner_set_gravity(positioner, param.gravity.value());

    if (param.constraint_adjustment)
        xdg_positioner_set_constraint_adjustment(positioner, param.constraint_adjustment.value());

    if (param.offset)
        xdg_positioner_set_offset(positioner, param.offset.value().first, param.offset.value().second);

    map_popup();

    ASSERT_THAT(state, Ne(std::experimental::nullopt)) << "popup configure event not sent";
    ASSERT_THAT(std::make_pair(state.value().x, state.value().y), Eq(param.expected_positon)) << "popup placed in incorrect position";
}

INSTANTIATE_TEST_CASE_P(
    Default,
    XdgPopupStableTest,
    testing::Values(
        PopupStableTestParams{"default values", (window_width - popup_width) / 2, (window_height - popup_height) / 2}
    ));

INSTANTIATE_TEST_CASE_P(
    DISABLED_Anchor,
    XdgPopupStableTest,
    testing::Values(
        PopupStableTestParams{"anchor left", -popup_width / 2, (window_height - popup_height) / 2}
            .with_anchor(XDG_POSITIONER_ANCHOR_LEFT),

        PopupStableTestParams{"anchor right", window_width - popup_width / 2, (window_height - popup_height) / 2}
            .with_anchor(XDG_POSITIONER_ANCHOR_RIGHT),

        PopupStableTestParams{"anchor top", (window_width - popup_width) / 2, -popup_height / 2}
            .with_anchor(XDG_POSITIONER_ANCHOR_TOP),

        PopupStableTestParams{"anchor bottom", (window_width - popup_width) / 2, window_height - popup_height / 2}
            .with_anchor(XDG_POSITIONER_ANCHOR_BOTTOM),

        PopupStableTestParams{"anchor top left", -popup_width / 2, -popup_height / 2}
            .with_anchor(XDG_POSITIONER_ANCHOR_TOP | XDG_POSITIONER_ANCHOR_LEFT),

        PopupStableTestParams{"anchor top right", window_width - popup_width / 2, -popup_height / 2}
            .with_anchor(XDG_POSITIONER_ANCHOR_TOP | XDG_POSITIONER_ANCHOR_RIGHT),

        PopupStableTestParams{"anchor bottom left", -popup_width / 2, window_height - popup_height / 2}
            .with_anchor(XDG_POSITIONER_ANCHOR_BOTTOM | XDG_POSITIONER_ANCHOR_LEFT),

        PopupStableTestParams{"anchor bottom right", window_width - popup_width / 2, window_height - popup_height / 2}
            .with_anchor(XDG_POSITIONER_ANCHOR_BOTTOM | XDG_POSITIONER_ANCHOR_RIGHT)
    ));

INSTANTIATE_TEST_CASE_P(
    DISABLED_Gravity,
    XdgPopupStableTest,
    testing::Values(
        PopupStableTestParams{"gravity none", (window_width - popup_width) / 2, (window_height - popup_height) / 2}
            .with_gravity(XDG_POSITIONER_GRAVITY_NONE),

        PopupStableTestParams{"gravity left", window_width / 2 - popup_width, (window_height - popup_height) / 2}
            .with_gravity(XDG_POSITIONER_GRAVITY_LEFT),

        PopupStableTestParams{"gravity right", window_width / 2, (window_height - popup_height) / 2}
            .with_gravity(XDG_POSITIONER_GRAVITY_RIGHT),

        PopupStableTestParams{"gravity top", (window_width - popup_width) / 2, window_height / 2 - popup_height}
            .with_gravity(XDG_POSITIONER_GRAVITY_TOP),

        PopupStableTestParams{"gravity bottom", (window_width - popup_width) / 2, window_height / 2}
            .with_gravity(XDG_POSITIONER_GRAVITY_BOTTOM),

        PopupStableTestParams{"gravity top left", window_width / 2 - popup_width, window_height / 2 - popup_height}
            .with_gravity(XDG_POSITIONER_GRAVITY_TOP | XDG_POSITIONER_GRAVITY_LEFT),

        PopupStableTestParams{"gravity top right", window_width / 2, window_height / 2 - popup_height}
            .with_gravity(XDG_POSITIONER_GRAVITY_TOP | XDG_POSITIONER_GRAVITY_RIGHT),

        PopupStableTestParams{"gravity bottom left", window_width / 2 - popup_width, window_height / 2}
            .with_gravity(XDG_POSITIONER_GRAVITY_BOTTOM | XDG_POSITIONER_GRAVITY_LEFT),

        PopupStableTestParams{"gravity bottom right", window_width / 2, window_height / 2}
            .with_gravity(XDG_POSITIONER_GRAVITY_BOTTOM | XDG_POSITIONER_GRAVITY_RIGHT)
    ));

INSTANTIATE_TEST_CASE_P(
    DISABLED_AnchorRect,
    XdgPopupStableTest,
    testing::Values(
        PopupStableTestParams{"explicit default anchor rect", (window_width - popup_width) / 2, (window_height - popup_height) / 2}
            .with_anchor_rect(0, 0, window_width, window_height),

        PopupStableTestParams{"upper left anchor rect", (window_width - 40 - popup_width) / 2, (window_height - 30 - popup_height) / 2}
            .with_anchor_rect(0, 0, window_width - 40, window_height - 30),

        PopupStableTestParams{"upper right anchor rect", (window_width + 40 - popup_width) / 2, (window_height - 30 - popup_height) / 2}
            .with_anchor_rect(40, 0, window_width - 40, window_height - 30),

        PopupStableTestParams{"lower left anchor rect", (window_width - 40 - popup_width) / 2, (window_height + 30 - popup_height) / 2}
            .with_anchor_rect(0, 30, window_width - 40, window_height - 30),

        PopupStableTestParams{"lower right anchor rect", (window_width + 40 - popup_width) / 2, (window_height + 30 - popup_height) / 2}
            .with_anchor_rect(40, 30, window_width - 40, window_height - 30),

        PopupStableTestParams{"offset anchor rect", (window_width - 40 - popup_width) / 2, (window_height - 80 - popup_height) / 2}
            .with_anchor_rect(20, 20, window_width - 80, window_height - 120)
    ));

INSTANTIATE_TEST_CASE_P(
    ZeroSizeAnchorRect, // only allowed in XDG shell stable, not unstable v6
    XdgPopupStableTest,
    testing::Values(
        PopupStableTestParams{"centered zero size anchor rect", (window_width - popup_width) / 2, (window_height - popup_height) / 2}
            .with_anchor_rect(window_width / 2, window_height / 2, 0, 0)
    ));

// TODO: test that positioner is always overlapping or adjacent to parent
// TODO: test that positioner is copied immediately after use
// TODO: test that error is raised when incomplete positioner is used (positioner without size and anchor rect set)
// TODO: test set_size
// TODO: test that set_window_geometry affects anchor rect
// TODO: test set_constraint_adjustment
// TODO: test set_offset
