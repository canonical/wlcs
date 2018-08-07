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
#include "xdg_shell_v6.h"

#include <gmock/gmock.h>

#include <experimental/optional>

using namespace testing;

int const window_width = 200, window_height = 300;
int const popup_width = window_width - 100, popup_height = window_height - 120;

class XdgPopupV6TestBase : public wlcs::StartedInProcessServer
{
public:
    static int const window_x = 500, window_y = 500;

    XdgPopupV6TestBase()
        : client{the_server()},
          surface{client},
          xdg_surface{client, surface},
          toplevel{xdg_surface},
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
        popup.emplace(popup_xdg_surface.value(), xdg_surface, positioner);

        popup_xdg_surface.value().add_configure_notification([&](uint32_t serial)
            {
                zxdg_surface_v6_ack_configure(popup_xdg_surface.value(), serial);
                popup_surface_configure_count++;
            });

        popup.value().add_configure_notification([this](int32_t x, int32_t y, int32_t width, int32_t height)
            {
                state = wlcs::XdgPopupV6::State{x, y, width, height};
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

    std::experimental::optional<std::pair<int, int>> get_popup_position()
    {
        if (state)
            return std::make_pair(state.value().x, state.value().y);
        else
            return std::experimental::nullopt;
    }

    wlcs::Client client;
    wlcs::Surface surface;
    wlcs::XdgSurfaceV6 xdg_surface;
    wlcs::XdgToplevelV6 toplevel;

    wlcs::XdgPositionerV6 positioner;
    std::experimental::optional<wlcs::Surface> popup_surface;
    std::experimental::optional<wlcs::XdgSurfaceV6> popup_xdg_surface;
    std::experimental::optional<wlcs::XdgPopupV6> popup;

    int popup_surface_configure_count{0};
    std::experimental::optional<wlcs::XdgPopupV6::State> state;
};

struct PopupTestParams
{
    PopupTestParams(std::string name, int expected_x, int expected_y)
        : name{name},
          expected_positon{expected_x, expected_y}
    {
    }

    PopupTestParams& with_size(int x, int y) { popup_size = {x, y}; return *this; }
    PopupTestParams& with_anchor_rect(int x, int y, int w, int h) { anchor_rect = {{x, y}, {w, h}}; return *this; }
    PopupTestParams& with_anchor(int value) { anchor = static_cast<zxdg_positioner_v6_anchor>(value); return *this; }
    PopupTestParams& with_gravity(int value) { gravity = static_cast<zxdg_positioner_v6_gravity>(value); return *this; }
    PopupTestParams& with_constraint_adjustment(int value)
    {
        constraint_adjustment = static_cast<zxdg_positioner_v6_constraint_adjustment>(value);
        return *this;
    }
    PopupTestParams& with_offset(int x, int y) { offset = {x, y}; return *this; }

    std::string name;
    std::pair<int, int> expected_positon;
    std::experimental::optional<std::pair<int, int>> popup_size; // will default to XdgPopupV6TestBase::popup_(width|height) if nullopt
    std::experimental::optional<std::pair<std::pair<int, int>, std::pair<int, int>>> anchor_rect; // will default to the full window rect
    std::experimental::optional<zxdg_positioner_v6_anchor> anchor;
    std::experimental::optional<zxdg_positioner_v6_gravity> gravity;
    std::experimental::optional<zxdg_positioner_v6_constraint_adjustment> constraint_adjustment;
    std::experimental::optional<std::pair<int, int>> offset;
};

std::ostream& operator<<(std::ostream& out, PopupTestParams const& param)
{
    return out << param.name;
}

class XdgPopupV6Test:
    public XdgPopupV6TestBase,
    public testing::WithParamInterface<PopupTestParams>
{
};

TEST_P(XdgPopupV6Test, positioner_places_popup_correctly)
{
    auto const& param = GetParam();

    // size must always be set
    if (param.popup_size)
        zxdg_positioner_v6_set_size(positioner, param.popup_size.value().first, param.popup_size.value().second);
    else
        zxdg_positioner_v6_set_size(positioner, popup_width, popup_height);

    // anchor rect must always be set
    if (param.anchor_rect)
        zxdg_positioner_v6_set_anchor_rect(positioner,
                                           param.anchor_rect.value().first.first,
                                           param.anchor_rect.value().first.second,
                                           param.anchor_rect.value().second.first,
                                           param.anchor_rect.value().second.second);
    else
        zxdg_positioner_v6_set_anchor_rect(positioner, 0, 0, window_width, window_height);

    if (param.anchor)
        zxdg_positioner_v6_set_anchor(positioner,  param.anchor.value());

    if (param.gravity)
        zxdg_positioner_v6_set_gravity(positioner, param.gravity.value());

    if (param.constraint_adjustment)
        zxdg_positioner_v6_set_constraint_adjustment(positioner, param.constraint_adjustment.value());

    if (param.offset)
        zxdg_positioner_v6_set_offset(positioner, param.offset.value().first, param.offset.value().second);

    map_popup();

    auto const pos = get_popup_position();
    ASSERT_TRUE(pos) << "popup not found";
    ASSERT_THAT(pos.value(), Eq(param.expected_positon)) << "popup placed in incorrect position";
}

INSTANTIATE_TEST_CASE_P(
    Default,
    XdgPopupV6Test,
    testing::Values(
        PopupTestParams{"default values",
                        (window_width - popup_width) / 2,
                        (window_height - popup_height) / 2}
    ));

INSTANTIATE_TEST_CASE_P(
    Anchor,
    XdgPopupV6Test,
    testing::Values(
        PopupTestParams{"anchor left",
                        - popup_width / 2,
                        (window_height - popup_height) / 2}
            .with_anchor(ZXDG_POSITIONER_V6_ANCHOR_LEFT),
        PopupTestParams{"anchor right",
                        window_width - popup_width / 2,
                        (window_height - popup_height) / 2}
            .with_anchor(ZXDG_POSITIONER_V6_ANCHOR_RIGHT),
        PopupTestParams{"anchor top",
                        (window_width - popup_width) / 2,
                         - popup_height / 2}
            .with_anchor(ZXDG_POSITIONER_V6_ANCHOR_TOP),
        PopupTestParams{"anchor bottom",
                        (window_width - popup_width) / 2,
                        window_height - popup_height / 2}
            .with_anchor(ZXDG_POSITIONER_V6_ANCHOR_BOTTOM),
        PopupTestParams{"anchor top left",
                         - popup_width / 2,
                        - popup_height / 2}
            .with_anchor(ZXDG_POSITIONER_V6_ANCHOR_TOP | ZXDG_POSITIONER_V6_ANCHOR_LEFT),
        PopupTestParams{"anchor top right",
                        window_width - popup_width / 2,
                        - popup_height / 2}
            .with_anchor(ZXDG_POSITIONER_V6_ANCHOR_TOP | ZXDG_POSITIONER_V6_ANCHOR_RIGHT),
        PopupTestParams{"anchor bottom left",
                         - popup_width / 2,
                        window_height - popup_height / 2}
            .with_anchor(ZXDG_POSITIONER_V6_ANCHOR_BOTTOM | ZXDG_POSITIONER_V6_ANCHOR_LEFT),
        PopupTestParams{"anchor bottom right",
                        window_width - popup_width / 2,
                        window_height - popup_height / 2}
            .with_anchor(ZXDG_POSITIONER_V6_ANCHOR_BOTTOM | ZXDG_POSITIONER_V6_ANCHOR_RIGHT)
    ));

// TODO: test that positioner is always overlapping or adjacent to parent
// TODO: test that positioner is copied immediately after use
// TODO: test that error is raised when incomplete positioner is used (positioner without size and anchor rect set)
// TODO: test set_size
// TODO: test set_anchor_rect
// TODO: test that set_window_geometry affects anchor rect
// TODO: test set_gravity
// TODO: test set_constraint_adjustment
// TODO: test set_offset
