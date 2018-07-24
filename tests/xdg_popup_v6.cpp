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

class XdgPopupV6Test : public wlcs::StartedInProcessServer
{
public:
    int const window_width = 400, window_height = 320;
    int const popup_width = 400, popup_height = 320;

    XdgPopupV6Test()
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

        zxdg_positioner_v6_set_size(positioner, popup_width, popup_height);
        zxdg_positioner_v6_set_anchor_rect(positioner, 0, 0, window_width, window_height);
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
    wlcs::XdgSurfaceV6 xdg_surface;
    wlcs::XdgToplevelV6 toplevel;

    wlcs::XdgPositionerV6 positioner;
    std::experimental::optional<wlcs::Surface> popup_surface;
    std::experimental::optional<wlcs::XdgSurfaceV6> popup_xdg_surface;
    std::experimental::optional<wlcs::XdgPopupV6> popup;

    int popup_surface_configure_count{0};
    wlcs::XdgPopupV6::State state{0, 0, 0, 0};
};


TEST_F(XdgPopupV6Test, popup_is_created_at_default_location)
{
    int const pointer_x = 20, pointer_y = 100;

    the_server().move_surface_to(surface, 0, 0);

    map_popup();

    auto pointer = the_server().create_pointer();
    pointer.move_to(pointer_x, pointer_y);
    client.roundtrip();

    ASSERT_THAT(client.focused_window(), Eq((wl_surface*)popup_surface.value()));
    ASSERT_THAT(client.pointer_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(pointer_x),
                    wl_fixed_from_int(pointer_y))));

    //dispatch_until_popup_configure();
    //EXPECT_THAT(state.x, Eq(0));
    //EXPECT_THAT(state.y, Eq(0));
    //EXPECT_THAT(state.width, Eq(popup_width));
    //EXPECT_THAT(state.height, Eq(popup_height));
}

// TODO: test that positioner is always overlapping or adjacent to parent
// TODO: test that positioner is copied immediately after use
// TODO: test that error is raised when incomplete positioner is used (positioner without size and anchor rect set)
// TODO: test set_size
// TODO: test set_anchor_rect
// TODO: test that set_window_geometry affects anchor rect
// TODO: test set_anchor
// TODO: test set_gravity
// TODO: test set_constraint_adjustment
// TODO: test set_offset
