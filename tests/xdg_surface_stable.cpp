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

#include "in_process_server.h"
#include "xdg_shell_stable.h"
#include "wl_handle.h"
#include "version_specifier.h"
#include "expect_protocol_error.h"

#include "expect_protocol_error.h"

#include <gmock/gmock.h>

#include <vector>

using namespace testing;
using XdgSurfaceStableTest = wlcs::InProcessServer;
using wlcs::AnyVersion;

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

    bool configure_received{false};
    EXPECT_CALL(xdg_surface, configure)
        .WillOnce([&](auto serial)
        {
            xdg_surface_ack_configure(xdg_surface, serial);
            configure_received = true;
        });

    wlcs::XdgToplevelStable toplevel{xdg_surface};
    // The first commit triggers an initial xdg_surface.configure event
    wl_surface_commit(surface);

    client.dispatch_until([&configure_received]() { return configure_received;} );
}

TEST_F(XdgSurfaceStableTest, gets_multiple_configure_events)
{
    wlcs::Client client{the_server()};
    wlcs::Surface surface{client};
    wlcs::XdgSurfaceStable xdg_surface{client, surface};

    std::vector<uint32_t> configure_serials;
    ON_CALL(xdg_surface, configure)
        .WillByDefault([&](auto serial)
        {
            xdg_surface_ack_configure(xdg_surface, serial);
            configure_serials.push_back(serial);
        });

    wlcs::XdgToplevelStable toplevel{xdg_surface};
    // The first commit triggers an initial xdg_surface.configure event
    wl_surface_commit(surface);

    auto const wait_for_another_configure = [&]()
    {
        auto const previous_count = configure_serials.size();
        client.dispatch_until([&]() { return configure_serials.size() > previous_count; });
    };

    // Wait for the initial configure, ack it, and map the surface with a buffer.
    wait_for_another_configure();
    surface.attach_buffer(200, 320);
    wl_surface_commit(surface);

    // Requesting a state change causes the compositor to send a further configure.
    xdg_toplevel_set_maximized(toplevel);
    wait_for_another_configure();

    // The two waits above guarantee at least two configure events; each must
    // carry a distinct serial.
    EXPECT_THAT(configure_serials.back(), Ne(configure_serials.front()));
}

TEST_F(XdgSurfaceStableTest, only_the_last_of_multiple_configures_needs_acking)
{
    wlcs::Client client{the_server()};
    wlcs::Surface surface{client};
    wlcs::XdgSurfaceStable xdg_surface{client, surface};

    std::vector<uint32_t> configure_serials;
    bool auto_ack{true};
    ON_CALL(xdg_surface, configure)
        .WillByDefault([&](auto serial)
        {
            configure_serials.push_back(serial);
            if (auto_ack)
                xdg_surface_ack_configure(xdg_surface, serial);
        });

    wlcs::XdgToplevelStable toplevel{xdg_surface};
    // The first commit triggers an initial xdg_surface.configure event
    wl_surface_commit(surface);

    auto const wait_for_another_configure = [&]()
    {
        auto const previous_count = configure_serials.size();
        client.dispatch_until([&]() { return configure_serials.size() > previous_count; });
    };

    // Map the surface, acking the initial configure as required.
    wait_for_another_configure();
    surface.attach_buffer(200, 320);
    wl_surface_commit(surface);

    // Stop acking so we accumulate several un-acked configure events.
    auto_ack = false;

    // Each state change prompts the compositor to send a further configure. The
    // two waits guarantee multiple un-acked configure events are accumulated.
    xdg_toplevel_set_maximized(toplevel);
    wait_for_another_configure();
    xdg_toplevel_unset_maximized(toplevel);
    wait_for_another_configure();

    // Flush any further in-flight configure events so we know the latest serial.
    client.roundtrip();

    // Acknowledging only the most recent configure must be accepted: the earlier
    // configure events do not need to be acknowledged individually.
    xdg_surface_ack_configure(xdg_surface, configure_serials.back());
    wl_surface_commit(surface);
    client.roundtrip();
}

TEST_F(XdgSurfaceStableTest, ack_configure_for_an_earlier_configure_after_a_later_one_is_an_error)
{
    wlcs::Client client{the_server()};
    wlcs::Surface surface{client};
    wlcs::XdgSurfaceStable xdg_surface{client, surface};

    std::vector<uint32_t> configure_serials;
    bool auto_ack{true};
    ON_CALL(xdg_surface, configure)
        .WillByDefault([&](auto serial)
        {
            configure_serials.push_back(serial);
            if (auto_ack)
                xdg_surface_ack_configure(xdg_surface, serial);
        });

    wlcs::XdgToplevelStable toplevel{xdg_surface};
    // The first commit triggers an initial xdg_surface.configure event
    wl_surface_commit(surface);

    auto const wait_for_another_configure = [&]()
    {
        auto const previous_count = configure_serials.size();
        client.dispatch_until([&]() { return configure_serials.size() > previous_count; });
    };

    // Map the surface, acking the initial configure as required.
    wait_for_another_configure();
    surface.attach_buffer(200, 320);
    wl_surface_commit(surface);

    // Stop acking so we can ack the following configure events out of order.
    auto_ack = false;

    // Each state change prompts the compositor to send a further configure.
    xdg_toplevel_set_maximized(toplevel);
    wait_for_another_configure();
    auto const earlier_serial = configure_serials.back();

    xdg_toplevel_unset_maximized(toplevel);
    wait_for_another_configure();
    auto const later_serial = configure_serials.back();

    ASSERT_THAT(later_serial, Ne(earlier_serial));

    // Acking the most recent configure consumes all the earlier ones.
    xdg_surface_ack_configure(xdg_surface, later_serial);
    client.roundtrip();

    // Acking a configure event issued before the last acked one must error.
    EXPECT_PROTOCOL_ERROR({
        xdg_surface_ack_configure(xdg_surface, earlier_serial);
        client.roundtrip();
    }, &xdg_surface_interface, XDG_SURFACE_ERROR_INVALID_SERIAL);
}

TEST_F(XdgSurfaceStableTest, ack_configure_with_invalid_serial_is_an_error)
{
    wlcs::Client client{the_server()};
    wlcs::Surface surface{client};
    wlcs::XdgSurfaceStable xdg_surface{client, surface};

    uint32_t configure_serial{0};
    bool configure_received{false};
    EXPECT_CALL(xdg_surface, configure)
        .WillOnce([&](auto serial)
        {
            configure_serial = serial;
            configure_received = true;
        });

    wlcs::XdgToplevelStable toplevel{xdg_surface};
    // The first commit triggers an initial xdg_surface.configure event
    wl_surface_commit(surface);

    client.dispatch_until([&configure_received]() { return configure_received; });

    // Acking a configure event that was never sent must raise an error.
    EXPECT_PROTOCOL_ERROR({
        xdg_surface_ack_configure(xdg_surface, configure_serial + 1);
        client.roundtrip();
    }, &xdg_surface_interface, XDG_SURFACE_ERROR_INVALID_SERIAL);
}

TEST_F(XdgSurfaceStableTest, ack_configure_twice_for_same_event_is_an_error)
{
    wlcs::Client client{the_server()};
    wlcs::Surface surface{client};
    wlcs::XdgSurfaceStable xdg_surface{client, surface};

    uint32_t configure_serial{0};
    bool configure_received{false};
    EXPECT_CALL(xdg_surface, configure)
        .WillOnce([&](auto serial)
        {
            configure_serial = serial;
            configure_received = true;
        });

    wlcs::XdgToplevelStable toplevel{xdg_surface};
    // The first commit triggers an initial xdg_surface.configure event
    wl_surface_commit(surface);

    client.dispatch_until([&configure_received]() { return configure_received; });

    // The first ack of the configure serial is valid.
    xdg_surface_ack_configure(xdg_surface, configure_serial);
    client.roundtrip();

    // Acking the same configure event a second time must raise an error.
    EXPECT_PROTOCOL_ERROR({
        xdg_surface_ack_configure(xdg_surface, configure_serial);
        client.roundtrip();
    }, &xdg_surface_interface, XDG_SURFACE_ERROR_INVALID_SERIAL);
}

TEST_F(XdgSurfaceStableTest, ack_configure_without_a_configure_event_is_an_error)
{
    wlcs::Client client{the_server()};
    wlcs::Surface surface{client};
    wlcs::XdgSurfaceStable xdg_surface{client, surface};

    // No surface commit is made, so the compositor never sends a configure
    // event. Acking any serial must therefore raise an error.
    EXPECT_PROTOCOL_ERROR({
        xdg_surface_ack_configure(xdg_surface, 1);
        client.roundtrip();
    }, &xdg_surface_interface, XDG_SURFACE_ERROR_INVALID_SERIAL);
}

TEST_F(XdgSurfaceStableTest, creating_xdg_surface_from_wl_surface_with_existing_role_is_an_error)
{
    wlcs::Client client{the_server()};

    auto const xdg_wm_base = client.bind_if_supported<struct xdg_wm_base>(AnyVersion);

    // We need a parent for the subsurface
    auto const parent = client.create_visible_surface(300, 300);

    auto const surface = wlcs::wrap_wl_object(wl_compositor_create_surface(client.compositor()));

    // We need some way of assigning a role to a wl_surface. wl_subcompositor is as good a way as any.
    auto const subsurface =
        wlcs::wrap_wl_object(wl_subcompositor_get_subsurface(client.subcompositor(), surface, parent));

    client.roundtrip();

    EXPECT_PROTOCOL_ERROR({
        xdg_wm_base_get_xdg_surface(xdg_wm_base, surface);
        client.roundtrip();
    }, &xdg_wm_base_interface, XDG_WM_BASE_ERROR_ROLE);
}


TEST_F(XdgSurfaceStableTest, creating_xdg_surface_from_wl_surface_with_attached_buffer_is_an_error)
{
    wlcs::Client client{the_server()};

    auto const xdg_wm_base = client.bind_if_supported<struct xdg_wm_base>(AnyVersion);

    auto const surface = wlcs::wrap_wl_object(wl_compositor_create_surface(client.compositor()));
    wlcs::ShmBuffer buffer{client, 300, 300};
    wl_surface_attach(surface, buffer, 0, 0);
    client.roundtrip();

    EXPECT_PROTOCOL_ERROR({
        xdg_wm_base_get_xdg_surface(xdg_wm_base, surface);
        client.roundtrip();
    }, &xdg_wm_base_interface, XDG_WM_BASE_ERROR_INVALID_SURFACE_STATE);
}

TEST_F(XdgSurfaceStableTest, creating_xdg_surface_from_wl_surface_with_committed_buffer_is_an_error)
{
    wlcs::Client client{the_server()};

    auto const xdg_wm_base = client.bind_if_supported<struct xdg_wm_base>(AnyVersion);

    auto const surface = wlcs::wrap_wl_object(wl_compositor_create_surface(client.compositor()));
    wlcs::ShmBuffer buffer{client, 300, 300};
    wl_surface_attach(surface, buffer, 0, 0);
    wl_surface_commit(surface);
    client.roundtrip();

    EXPECT_PROTOCOL_ERROR({
        xdg_wm_base_get_xdg_surface(xdg_wm_base, surface);
        client.roundtrip();
    }, &xdg_wm_base_interface, XDG_WM_BASE_ERROR_INVALID_SURFACE_STATE);
}

TEST_F(XdgSurfaceStableTest, attaching_buffer_to_unconfigured_xdg_surface_is_an_error)
{
    wlcs::Client client{the_server()};

    auto const xdg_wm_base = client.bind_if_supported<struct xdg_wm_base>(AnyVersion);

    auto const surface = wlcs::wrap_wl_object(wl_compositor_create_surface(client.compositor()));
    wlcs::ShmBuffer buffer{client, 300, 300};
    client.roundtrip();

    EXPECT_PROTOCOL_ERROR({
        xdg_wm_base_get_xdg_surface(xdg_wm_base, surface);
        wl_surface_attach(surface, buffer, 0, 0);
        client.roundtrip();
    }, &xdg_surface_interface, XDG_SURFACE_ERROR_UNCONFIGURED_BUFFER);
}
