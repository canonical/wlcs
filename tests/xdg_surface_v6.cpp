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

using XdgSurfaceV6Test = wlcs::InProcessServer;

TEST_F(XdgSurfaceV6Test, supports_xdg_shell_v6_protocol)
{
    using namespace testing;

    wlcs::Client client{the_server()};
    ASSERT_THAT(client.xdg_shell_v6(), NotNull());
    wlcs::Surface surface{client};
    wlcs::XdgSurfaceV6 xdg_surface{client, surface};
}

TEST_F(XdgSurfaceV6Test, gets_configure_event)
{
    using namespace testing;

    wlcs::Client client{the_server()};
    wlcs::Surface surface{client};
    wlcs::XdgSurfaceV6 xdg_surface{client, surface};

    int surface_configure_count{0};
    xdg_surface.add_configure_notification([&](uint32_t serial)
        {
            zxdg_surface_v6_ack_configure(xdg_surface, serial);
            surface_configure_count++;
        });

    wlcs::XdgToplevelV6 toplevel{xdg_surface};
    surface.attach_buffer(600, 400);

    client.roundtrip();

    EXPECT_THAT(surface_configure_count, Eq(0));

    wl_surface_commit(surface);

    client.roundtrip();

    EXPECT_THAT(surface_configure_count, Eq(1));
}
