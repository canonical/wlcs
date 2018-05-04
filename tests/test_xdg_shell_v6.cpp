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

class XdgSurfaceV6Test: public wlcs::InProcessServer
{
public:

    void SetUp() override
    {
        wlcs::InProcessServer::SetUp();
        client = std::make_unique<wlcs::Client>(the_server());
        surface = std::make_unique<wlcs::Surface>(*client);
        shell_surface = zxdg_shell_v6_get_xdg_surface(client->xdg_shell_v6(), *surface);
        toplevel = zxdg_surface_v6_get_toplevel(shell_surface);
        wl_surface_commit(*surface);
    }

    void TearDown() override
    {
        client->roundtrip();
        buffers.clear();
        zxdg_toplevel_v6_destroy(toplevel);
        zxdg_surface_v6_destroy(shell_surface);
        surface.reset();
        client.reset();
        wlcs::InProcessServer::TearDown();
    }

    void attach_buffer(int width, int height)
    {
        buffers.push_back(wlcs::ShmBuffer{*client, width, height});
        wl_surface_attach(*surface, buffers.back(), 0, 0);
        wl_surface_commit(*surface);
    }

    std::unique_ptr<wlcs::Client> client;
    std::unique_ptr<wlcs::Surface> surface;
    std::vector<wlcs::ShmBuffer> buffers;
    zxdg_surface_v6* shell_surface;
    zxdg_toplevel_v6* toplevel;
};

TEST_F(XdgSurfaceV6Test, supports_xdg_shell_v6_protocol)
{
    using namespace testing;

    attach_buffer(600, 400);
    wl_surface_commit(*surface);

    client->roundtrip();
}
