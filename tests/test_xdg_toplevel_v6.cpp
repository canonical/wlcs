/*
 * Copyright © 2012 Intel Corporation
 * Copyright © 2013 Collabora, Ltd.
 * Copyright © 2017 Canonical Ltd.
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

using XdgToplevelV6Test = wlcs::InProcessServer;

class XdgToplevelWindow
{
public:
    XdgToplevelWindow(wlcs::Server& server)
        : client{server},
          surface{client},
          xdg_surface{client, surface},
          toplevel{xdg_surface}
    {}

    ~XdgToplevelWindow()
    {
        client.roundtrip();
        buffers.clear();
    }

    void attach_buffer(int width, int height)
    {
        buffers.push_back(wlcs::ShmBuffer{client, width, height});
        wl_surface_attach(surface, buffers.back(), 0, 0);
    }

    void dispatch_until_configure()
    {
        client.dispatch_until(
            [prev_count = xdg_surface.configure_events_count, &current_count = xdg_surface.configure_events_count]()
            {
                return current_count > prev_count;
            });
    }

    wlcs::Client client;
    wlcs::Surface surface;
    wlcs::XdgSurfaceV6 xdg_surface;
    wlcs::XdgToplevelV6 toplevel;
    std::vector<wlcs::ShmBuffer> buffers;
};

TEST_F(XdgToplevelV6Test, default_configuration)
{
    using namespace testing;

    XdgToplevelWindow window{the_server()};
    wl_surface_commit(window.surface);
    window.client.roundtrip();
    window.attach_buffer(600, 400);
    wl_surface_commit(window.surface);

    window.dispatch_until_configure();

    // default values
    EXPECT_EQ(window.toplevel.window_width, 0);
    EXPECT_EQ(window.toplevel.window_height, 0);
    EXPECT_FALSE(window.toplevel.window_maximized);
    EXPECT_FALSE(window.toplevel.window_fullscreen);
    EXPECT_FALSE(window.toplevel.window_resizing);
    EXPECT_TRUE(window.toplevel.window_activated);
}

TEST_F(XdgToplevelV6Test, correct_configuration_when_maximized)
{
    using namespace testing;

    XdgToplevelWindow window{the_server()};
    wl_surface_commit(window.surface);
    window.client.roundtrip();
    window.attach_buffer(200, 200);
    wl_surface_commit(window.surface);

    window.dispatch_until_configure();

    zxdg_toplevel_v6_set_maximized(window.toplevel);
    wl_surface_commit(window.surface);

    window.dispatch_until_configure();

    EXPECT_GT(window.toplevel.window_width, 0);
    EXPECT_GT(window.toplevel.window_height, 0);
    EXPECT_TRUE(window.toplevel.window_maximized);
    EXPECT_FALSE(window.toplevel.window_fullscreen);
    EXPECT_FALSE(window.toplevel.window_resizing);
    EXPECT_TRUE(window.toplevel.window_activated);
}

TEST_F(XdgToplevelV6Test, correct_configuration_when_maximized_and_unmaximized)
{
    using namespace testing;

    XdgToplevelWindow window{the_server()};
    wl_surface_commit(window.surface);
    window.client.roundtrip();
    window.attach_buffer(200, 200);
    wl_surface_commit(window.surface);

    window.dispatch_until_configure();

    zxdg_toplevel_v6_set_maximized(window.toplevel);
    wl_surface_commit(window.surface);

    window.dispatch_until_configure();

    zxdg_toplevel_v6_unset_maximized(window.toplevel);
    wl_surface_commit(window.surface);

    window.dispatch_until_configure();

    EXPECT_FALSE(window.toplevel.window_maximized);
    EXPECT_FALSE(window.toplevel.window_fullscreen);
    EXPECT_FALSE(window.toplevel.window_resizing);
    EXPECT_TRUE(window.toplevel.window_activated);
}
