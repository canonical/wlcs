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

using XdgToplevelV6Configuration = wlcs::InProcessServer;

class XdgToplevelWindow
{
public:
    int const window_width = 200, window_height = 320;

    XdgToplevelWindow(wlcs::Server& server)
        : client{server},
          surface{client},
          xdg_surface{client, surface},
          toplevel{xdg_surface}
    {
        xdg_surface.add_configure_notification([&](uint32_t serial)
            {
                zxdg_surface_v6_ack_configure(xdg_surface, serial);
                surface_configure_count++;
            });

        toplevel.add_configure_notification([this] (int32_t width, int32_t height, struct wl_array *states)
            {
                state = wlcs::XdgToplevelV6::State{width, height, states};
            });

        wl_surface_commit(surface);
        client.roundtrip();
        surface.attach_buffer(window_width, window_height);
        wl_surface_commit(surface);
        dispatch_until_configure();
    }

    ~XdgToplevelWindow()
    {
        client.roundtrip();
    }

    void dispatch_until_configure()
    {
        client.dispatch_until(
            [prev_count = surface_configure_count, &current_count = surface_configure_count]()
            {
                return current_count > prev_count;
            });
    }

    wlcs::Client client;
    wlcs::Surface surface;
    wlcs::XdgSurfaceV6 xdg_surface;
    wlcs::XdgToplevelV6 toplevel;

    int surface_configure_count{0};
    std::experimental::optional<wlcs::XdgToplevelV6::State> state;
};

TEST_F(XdgToplevelV6Configuration, default)
{
    using namespace testing;

    XdgToplevelWindow window{the_server()};

    // default values
    ASSERT_THAT(window.state, Ne(std::experimental::nullopt));
    auto state = window.state.value();
    EXPECT_THAT(state.width, Eq(0));
    EXPECT_THAT(state.height, Eq(0));
    EXPECT_THAT(state.maximized, Eq(false));
    EXPECT_THAT(state.fullscreen, Eq(false));
    EXPECT_THAT(state.resizing, Eq(false));
    EXPECT_THAT(state.activated, Eq(true));
}

TEST_F(XdgToplevelV6Configuration, maximized_and_unmaximized)
{
    using namespace testing;

    XdgToplevelWindow window{the_server()};

    zxdg_toplevel_v6_set_maximized(window.toplevel);
    wl_surface_commit(window.surface);
    window.dispatch_until_configure();

    ASSERT_THAT(window.state, Ne(std::experimental::nullopt));
    auto state = window.state.value();
    EXPECT_THAT(state.width, Gt(0));
    EXPECT_THAT(state.height, Gt(0));
    EXPECT_THAT(state.maximized, Eq(true));
    EXPECT_THAT(state.fullscreen, Eq(false));
    EXPECT_THAT(state.resizing, Eq(false));
    EXPECT_THAT(state.activated, Eq(true));

    zxdg_toplevel_v6_unset_maximized(window.toplevel);
    wl_surface_commit(window.surface);
    window.dispatch_until_configure();

    ASSERT_THAT(window.state, Ne(std::experimental::nullopt));
    state = window.state.value();
    EXPECT_THAT(state.maximized, Eq(false));
    EXPECT_THAT(state.fullscreen, Eq(false));
    EXPECT_THAT(state.resizing, Eq(false));
    EXPECT_THAT(state.activated, Eq(true));
}

TEST_F(XdgToplevelV6Configuration, fullscreened_and_restored)
{
    using namespace testing;

    XdgToplevelWindow window{the_server()};

    zxdg_toplevel_v6_set_fullscreen(window.toplevel, nullptr);
    wl_surface_commit(window.surface);
    window.dispatch_until_configure();

    ASSERT_THAT(window.state, Ne(std::experimental::nullopt));
    auto state = window.state.value();
    EXPECT_THAT(state.width, Gt(0));
    EXPECT_THAT(state.height, Gt(0));
    EXPECT_THAT(state.maximized, Eq(false)); // is this right? should it not be maximized, even when fullscreen?
    EXPECT_THAT(state.fullscreen, Eq(true));
    EXPECT_THAT(state.resizing, Eq(false));
    EXPECT_THAT(state.activated, Eq(true));

    zxdg_toplevel_v6_unset_fullscreen(window.toplevel);
    wl_surface_commit(window.surface);
    window.dispatch_until_configure();

    ASSERT_THAT(window.state, Ne(std::experimental::nullopt));
    state = window.state.value();
    EXPECT_THAT(state.maximized, Eq(false));
    EXPECT_THAT(state.fullscreen, Eq(false));
    EXPECT_THAT(state.resizing, Eq(false));
    EXPECT_THAT(state.activated, Eq(true));
}
