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

class XdgPopupV6Test : wlcs::StartedInProcessServer
{
public:
    int const window_width = 400, window_height = 320;

    XdgPopupV6Test()
        : client{the_server()},
          surface{client},
          xdg_surface{client, surface}
    {
        surface.attach_buffer(window_width, window_height);
        wl_surface_commit(surface);
    }

    wlcs::Client client;
    wlcs::Surface surface;
    wlcs::XdgSurfaceV6 xdg_surface;
};

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
