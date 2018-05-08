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

#include <memory>

using XdgSurfaceV6Test = wlcs::XdgSurfaceV6;

TEST_F(XdgSurfaceV6Test, supports_xdg_shell_v6_protocol)
{
    using namespace testing;
}

TEST_F(XdgSurfaceV6Test, configure_event)
{
    using namespace testing;

    attach_buffer(200, 300);
    wl_surface_commit(*surface);

    dispatch_until_configure();
}

TEST_F(XdgSurfaceV6Test, maximize)
{
    using namespace testing;

    // default values
    EXPECT_EQ(window_width, 0);
    EXPECT_EQ(window_height, 0);
    EXPECT_FALSE(window_maximized);

    //zxdg_surface_v6_set_window_geometry(shell_surface, 0, 0, 200, 300);
    zxdg_toplevel_v6_set_maximized(toplevel);
    wl_surface_commit(*surface);

    dispatch_until_configure();

    EXPECT_TRUE(window_maximized);
    EXPECT_GT(window_width, 0);
    EXPECT_GT(window_height, 0);
}

TEST_F(XdgSurfaceV6Test, unmaximize)
{
    using namespace testing;

    zxdg_toplevel_v6_set_maximized(toplevel);
    wl_surface_commit(*surface);

    dispatch_until_configure();

    EXPECT_TRUE(window_maximized);
    EXPECT_GT(window_width, 0);
    EXPECT_GT(window_height, 0);

    zxdg_toplevel_v6_unset_maximized(toplevel);
    wl_surface_commit(*surface);

    dispatch_until_configure();

    EXPECT_FALSE(window_maximized);
}
