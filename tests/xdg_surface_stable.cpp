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

using namespace testing;
using XdgSurfaceStableTest = wlcs::InProcessServer;

TEST_F(XdgSurfaceStableTest, supports_xdg_shell_stable_protocol)
{
    wlcs::Client client{the_server()};
    ASSERT_THAT(client.xdg_shell_stable(), NotNull());
    wlcs::Surface surface{client};
    wlcs::XdgSurfaceStable xdg_surface{client, surface};
}

TEST_F(XdgSurfaceStableTest, gets_configure_event)
{
    wlcs::Client client{the_server()};
    wlcs::Surface surface{client};
    wlcs::XdgSurfaceStable xdg_surface{client, surface};

    int surface_configure_count{0};
    xdg_surface.add_configure_notification([&](uint32_t serial)
        {
            xdg_surface_ack_configure(xdg_surface, serial);
            surface_configure_count++;
        });

    wlcs::XdgToplevelStable toplevel{xdg_surface};
    surface.attach_buffer(600, 400);

    client.roundtrip();

    EXPECT_THAT(surface_configure_count, Eq(1));
}

TEST_F(XdgSurfaceStableTest, creating_xdg_surface_from_wl_surface_with_existing_role_is_an_error)
{
    wlcs::Client client{the_server()};

    auto const xdg_wm_base = client.bind_if_supported<struct xdg_wm_base>(xdg_wm_base_interface, xdg_wm_base_destroy);

    // We need some way of assigning a role to a wl_surface. wl_subcompositor is as good a way as any.
    auto const subcompositor = client.bind_if_supported<struct wl_subcompositor>(wl_subcompositor_interface, wl_subcompositor_destroy);

    // We need a parent for the subsurface
    auto const parent = client.create_visible_surface(300, 300);

    auto const surface = wlcs::wrap_proxy(wl_compositor_create_surface(client.compositor()), wl_surface_destroy);
    // It doesn't matter that we leak the wl_subsurface; that'll be cleaned up in client destruction.
    wl_subcompositor_get_subsurface(subcompositor, surface, parent);
    client.roundtrip();

    try
    {
        xdg_wm_base_get_xdg_surface(xdg_wm_base, surface);
        client.roundtrip();
    }
    catch(wlcs::ProtocolError const& error)
    {
        EXPECT_THAT(error.interface(), Eq(&xdg_wm_base_interface));
        EXPECT_THAT(error.error_code(), Eq(XDG_WM_BASE_ERROR_ROLE));
        return;
    }

    FAIL() << "Expected protocol error not received";
}


TEST_F(XdgSurfaceStableTest, creating_xdg_surface_from_wl_surface_with_attached_buffer_is_an_error)
{
    wlcs::Client client{the_server()};

    auto const xdg_wm_base = client.bind_if_supported<struct xdg_wm_base>(xdg_wm_base_interface, xdg_wm_base_destroy);

    auto const surface = wlcs::wrap_proxy(wl_compositor_create_surface(client.compositor()), wl_surface_destroy);
    wlcs::ShmBuffer buffer{client, 300, 300};
    wl_surface_attach(surface, buffer, 0, 0);
    client.roundtrip();

    try
    {
        xdg_wm_base_get_xdg_surface(xdg_wm_base, surface);
        client.roundtrip();
    }
    catch(wlcs::ProtocolError const& error)
    {
        EXPECT_THAT(error.interface(), Eq(&xdg_wm_base_interface));
        EXPECT_THAT(error.error_code(), Eq(XDG_WM_BASE_ERROR_INVALID_SURFACE_STATE));
        return;
    }

    FAIL() << "Expected protocol error not received";
}

TEST_F(XdgSurfaceStableTest, creating_xdg_surface_from_wl_surface_with_committed_buffer_is_an_error)
{
    wlcs::Client client{the_server()};

    auto const xdg_wm_base = client.bind_if_supported<struct xdg_wm_base>(xdg_wm_base_interface, xdg_wm_base_destroy);

    auto const surface = wlcs::wrap_proxy(wl_compositor_create_surface(client.compositor()), wl_surface_destroy);
    wlcs::ShmBuffer buffer{client, 300, 300};
    wl_surface_attach(surface, buffer, 0, 0);
    wl_surface_commit(surface);
    client.roundtrip();

    try
    {
        xdg_wm_base_get_xdg_surface(xdg_wm_base, surface);
        client.roundtrip();
    }
    catch(wlcs::ProtocolError const& error)
    {
        EXPECT_THAT(error.interface(), Eq(&xdg_wm_base_interface));
        EXPECT_THAT(error.error_code(), Eq(XDG_WM_BASE_ERROR_INVALID_SURFACE_STATE));
        return;
    }

    FAIL() << "Expected protocol error not received";
}

TEST_F(XdgSurfaceStableTest, attaching_buffer_to_unconfigured_xdg_surface_is_an_error)
{
    wlcs::Client client{the_server()};

    auto const xdg_wm_base = client.bind_if_supported<struct xdg_wm_base>(xdg_wm_base_interface, xdg_wm_base_destroy);

    auto const surface = wlcs::wrap_proxy(wl_compositor_create_surface(client.compositor()), wl_surface_destroy);
    wlcs::ShmBuffer buffer{client, 300, 300};
    client.roundtrip();

    try
    {
        xdg_wm_base_get_xdg_surface(xdg_wm_base, surface);
        wl_surface_attach(surface, buffer, 0, 0);
        client.roundtrip();
    }
    catch(wlcs::ProtocolError const& error)
    {
        EXPECT_THAT(error.interface(), Eq(&xdg_surface_interface));
        EXPECT_THAT(error.error_code(), Eq(XDG_SURFACE_ERROR_UNCONFIGURED_BUFFER));
        return;
    }

    FAIL() << "Expected protocol error not received";
}
