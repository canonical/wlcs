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
#include "generated/xdg-shell-unstable-v6-client.h"

#include <gmock/gmock.h>

#include <memory>

using XdgShellV6Test = wlcs::InProcessServer;

TEST_F(XdgShellV6Test, supports_xdg_shell_v6_protocol)
{
    using namespace testing;

    wlcs::Client client{the_server()};

    wlcs::Surface surface{client};

    zxdg_surface_v6* xdg_shell_surface = zxdg_shell_v6_get_xdg_surface(client.xdg_shell_v6(), surface);
    zxdg_toplevel_v6* toplevel = zxdg_surface_v6_get_toplevel(xdg_shell_surface);
    wl_surface_commit(surface);

    auto buffer = std::make_shared<wlcs::ShmBuffer>(client, 600, 400);
    wl_surface_attach(surface, *buffer, 0, 0);
    wl_surface_commit(surface);

    client.roundtrip();

    zxdg_toplevel_v6_destroy(toplevel);
    zxdg_surface_v6_destroy(xdg_shell_surface);

    client.roundtrip();
}
