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

#include "generated/xdg-shell-client.h"
#include "in_process_server.h"
#include "version_specifier.h"
#include "xdg_shell_stable.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <cmath>
#include <gmock/gmock.h>

#include <numbers>
#include <unistd.h>
#include <wayland-client-protocol.h>

using namespace testing;

namespace
{
class ConfigurationWindow
{
public:
    int const window_width = 200, window_height = 320;

    ConfigurationWindow(wlcs::Client& client)
        : client{client},
          surface{client},
          xdg_shell_surface{client, surface},
          toplevel{xdg_shell_surface}
    {
        ON_CALL(xdg_shell_surface, configure).WillByDefault([&](auto serial)
            {
                xdg_surface_ack_configure(xdg_shell_surface, serial);
                surface_configure_count++;
            });

        ON_CALL(toplevel, configure).WillByDefault([&](auto... args)
            {
                state = wlcs::XdgToplevelStable::State{args...};
            });

        wl_surface_commit(surface);
        /* From the xdg_surface protocol:
         * ...
         * After creating a role-specific object and setting it up, the client must perform an initial commit
         * without any buffer attached. The compositor will reply with initial wl_surface state such as
         * wl_surface.preferred_buffer_scale followed by an xdg_surface.configure event. The client must acknowledge
         * it and is then allowed to attach a buffer to map the surface.
         * ...
         * We've created the role-specific XdgToplevel above; we should now wait for
         * a configure event, ack it, and *then* attach a buffer.
         */
        dispatch_until_configure();

        surface.attach_buffer(window_width, window_height);
        wl_surface_commit(surface);
        client.flush();

        /* Now that we've committed a buffer (and hence should be mapped) we expect
         * that our surface will be active. Mir (and GNOME) send a second configure
         * event after the initial buffer submitted, but this isn't mandated by
         * the protocol.
         *
         * Instead, wait until we're in the “activated” state, as WLCS makes the
         * assumption that newly-mapped windows are active.
         */
        client.dispatch_until([this]() { return state.activated; });
    }

    void dispatch_until_configure()
    {
        client.dispatch_until(
            [prev_count = surface_configure_count, &current_count = surface_configure_count]()
            {
                return current_count > prev_count;
            });
    }

    operator wlcs::Surface&() {return surface;}

    operator wl_surface*() const {return surface;}
    operator xdg_surface*() const {return xdg_shell_surface;}
    operator xdg_toplevel*() const {return toplevel;}

    wlcs::Client& client;
    wlcs::Surface surface;
    wlcs::XdgSurfaceStable xdg_shell_surface;
    wlcs::XdgToplevelStable toplevel;

    int surface_configure_count{0};
    wlcs::XdgToplevelStable::State state{0, 0, nullptr};
};
}

using XdgToplevelStableTest = wlcs::InProcessServer;

TEST_F(XdgToplevelStableTest, wm_capabilities_are_sent)
{
    wlcs::Client client{the_server()};
    client.bind_if_supported<xdg_wm_base>(wlcs::AtLeastVersion{XDG_TOPLEVEL_WM_CAPABILITIES_SINCE_VERSION});
    wlcs::Surface surface{client};
    wlcs::XdgSurfaceStable xdg_shell_surface{client, surface};
    wlcs::XdgToplevelStable toplevel{xdg_shell_surface};
    EXPECT_CALL(toplevel, wm_capabilities).Times(1);
    client.roundtrip();
}

// there *could* be a bug in these tests, but also the window manager may not be behaving properly
// lets take another look when we've updated the window manager
TEST_F(XdgToplevelStableTest, pointer_respects_window_geom_offset)
{
    const int offset_x = 35, offset_y = 12;
    const int window_pos_x = 200, window_pos_y = 280;
    const int pointer_x = window_pos_x + 20, pointer_y = window_pos_y + 30;

    wlcs::Client client{the_server()};
    ConfigurationWindow window{client};
    xdg_surface_set_window_geometry(window.xdg_shell_surface,
                                    offset_x,
                                    offset_y,
                                    window.window_width - offset_x,
                                    window.window_height - offset_y);
    wl_surface_commit(window.surface);
    the_server().move_surface_to(window.surface, window_pos_x, window_pos_y);

    auto pointer = the_server().create_pointer();
    pointer.move_to(pointer_x, pointer_y);
    client.roundtrip();

    ASSERT_THAT(client.window_under_cursor(), Eq((wl_surface*)window.surface));
    ASSERT_THAT(client.pointer_position(),
                Ne(std::make_pair(
                    wl_fixed_from_int(pointer_x - window_pos_x),
                    wl_fixed_from_int(pointer_y - window_pos_y)))) << "set_window_geometry offset was ignored";
    ASSERT_THAT(client.pointer_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(pointer_x - window_pos_x + offset_x),
                    wl_fixed_from_int(pointer_y - window_pos_y + offset_y))));
}

TEST_F(XdgToplevelStableTest, touch_respects_window_geom_offset)
{
    const int offset_x = 35, offset_y = 12;
    const int window_pos_x = 200, window_pos_y = 280;
    const int pointer_x = window_pos_x + 20, pointer_y = window_pos_y + 30;

    wlcs::Client client{the_server()};
    ConfigurationWindow window{client};
    xdg_surface_set_window_geometry(window.xdg_shell_surface,
                                        offset_x,
                                        offset_y,
                                        window.window_width - offset_x,
                                        window.window_height - offset_y);
    wl_surface_commit(window.surface);
    the_server().move_surface_to(window.surface, window_pos_x, window_pos_y);

    auto touch = the_server().create_touch();
    touch.down_at(pointer_x, pointer_y);
    client.roundtrip();

    ASSERT_THAT(client.touched_window(), Eq((wl_surface*)window.surface));
    ASSERT_THAT(client.touch_position(),
                Ne(std::make_pair(
                    wl_fixed_from_int(pointer_x - window_pos_x),
                    wl_fixed_from_int(pointer_y - window_pos_y)))) << "set_window_geometry offset was ignored";
    ASSERT_THAT(client.touch_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(pointer_x - window_pos_x + offset_x),
                    wl_fixed_from_int(pointer_y - window_pos_y + offset_y))));
}

// TODO: set_window_geometry window size (something will need to be added to wlcs)

TEST_F(XdgToplevelStableTest, surface_can_be_moved_interactively)
{
    int window_x = 100, window_y = 100;
    int window_width = 420, window_height = 390;
    int start_x = window_x + 5, start_y = window_y + 5;
    int dx = 60, dy = -40;
    int end_x = window_x + dx + 20, end_y = window_y + dy + 20;

    wlcs::Client client{the_server()};
    wlcs::Surface surface{client};
    wlcs::XdgSurfaceStable xdg_shell_surface{client, surface};
    wlcs::XdgToplevelStable toplevel{xdg_shell_surface};
    surface.attach_buffer(window_width, window_height);
    wl_surface_commit(surface);
    client.roundtrip();

    the_server().move_surface_to(surface, window_x, window_y);

    auto pointer = the_server().create_pointer();

    bool button_down{false};
    uint32_t last_serial{0};

    client.add_pointer_button_notification([&](uint32_t serial, uint32_t, bool is_down) -> bool {
            last_serial = serial;
            button_down = is_down;
            return true;
        });

    pointer.move_to(start_x, start_y);
    pointer.left_button_down();

    client.dispatch_until([&](){
            return button_down;
        });

    xdg_toplevel_move(toplevel, client.seat(), last_serial);
    client.roundtrip();
    pointer.move_to(start_x + dx, start_x + dy);
    pointer.left_button_up();
    client.roundtrip();

    pointer.move_to(end_x, end_y);
    client.roundtrip();

    EXPECT_THAT(client.window_under_cursor(), Eq(static_cast<struct wl_surface*>(surface)));
    EXPECT_THAT(client.pointer_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(end_x - window_x - dx),
                    wl_fixed_from_int(end_y - window_y - dy))));

    client.roundtrip();
}

// Tests https://github.com/MirServer/mir/issues/1792
TEST_F(XdgToplevelStableTest, touch_can_not_steal_pointer_based_move)
{
    int window_x = 100, window_y = 100;
    int window_width = 420, window_height = 390;
    int start_x = window_x + 5, start_y = window_y + 5;

    wlcs::Client client{the_server()};
    wlcs::Surface surface{client};
    wlcs::XdgSurfaceStable xdg_shell_surface{client, surface};
    wlcs::XdgToplevelStable toplevel{xdg_shell_surface};
    surface.attach_visible_buffer(window_width, window_height);
    the_server().move_surface_to(surface, window_x, window_y);

    auto pointer = the_server().create_pointer();
    auto touch = the_server().create_touch();

    bool button_down{false};
    uint32_t last_pointer_serial{0};

    client.add_pointer_button_notification([&](uint32_t serial, uint32_t, bool is_down) -> bool {
            last_pointer_serial = serial;
            button_down = is_down;
            return true;
        });

    pointer.move_to(start_x, start_y);
    pointer.left_button_down();
    touch.down_at(start_x, start_y);

    client.dispatch_until([&](){
            return button_down;
        });

    xdg_toplevel_move(toplevel, client.seat(), last_pointer_serial);
    client.roundtrip();
    pointer.left_button_up();
    touch.move_to(0, 0);
    client.roundtrip();

    // The move should have either been ignored entirly or been based on the pointer (which didn't move)
    // Either way, the window should be in the same place it started
    EXPECT_THAT(client.window_under_cursor(), Eq(static_cast<struct wl_surface*>(surface)));
    EXPECT_THAT(client.pointer_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(start_x - window_x),
                    wl_fixed_from_int(start_y - window_y))));
}

TEST_F(XdgToplevelStableTest, pointer_leaves_surface_during_interactive_move)
{
    int window_x = 100, window_y = 100;
    int window_width = 420, window_height = 390;
    int start_x = window_x + 5, start_y = window_y + 5;

    wlcs::Client client{the_server()};
    wlcs::Surface surface{client};
    wlcs::XdgSurfaceStable xdg_shell_surface{client, surface};
    wlcs::XdgToplevelStable toplevel{xdg_shell_surface};
    surface.attach_buffer(window_width, window_height);
    wl_surface_commit(surface);
    client.roundtrip();

    the_server().move_surface_to(surface, window_x, window_y);

    auto pointer = the_server().create_pointer();

    bool button_down{false};
    uint32_t last_serial{0};

    client.add_pointer_button_notification([&](uint32_t serial, uint32_t, bool is_down) -> bool {
            last_serial = serial;
            button_down = is_down;
            return true;
        });

    pointer.move_to(start_x, start_y);
    pointer.left_button_down();

    client.dispatch_until([&](){
            return button_down;
        });

    xdg_toplevel_move(toplevel, client.seat(), last_serial);
    client.dispatch_until([&](){
            return !client.window_under_cursor();
        });
}

TEST_F(XdgToplevelStableTest, surface_can_be_resized_interactively)
{
    int window_x = 100, window_y = 100;
    int window_width = 420, window_height = 390;
    int start_x = window_x + 5, start_y = window_y + 5;
    int dx = 60, dy = -40;
    int end_x = window_x + dx + 20, end_y = window_y + dy + 20;

    wlcs::Client client{the_server()};
    wlcs::Surface surface{client};
    wlcs::XdgSurfaceStable xdg_shell_surface{client, surface};
    wlcs::XdgToplevelStable toplevel{xdg_shell_surface};
    surface.attach_buffer(window_width, window_height);
    wl_surface_commit(surface);
    client.roundtrip();

    the_server().move_surface_to(surface, window_x, window_y);

    auto pointer = the_server().create_pointer();

    bool button_down{false};
    uint32_t last_serial{0};

    client.add_pointer_button_notification([&](uint32_t serial, uint32_t, bool is_down) -> bool {
            last_serial = serial;
            button_down = is_down;
            return true;
        });

    pointer.move_to(start_x, start_y);
    pointer.left_button_down();

    client.dispatch_until([&](){
            return button_down;
        });

    xdg_toplevel_resize(toplevel, client.seat(), last_serial, XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT);
    client.roundtrip();
    pointer.move_to(start_x + dx, start_y + dy);
    pointer.left_button_up();
    client.roundtrip();

    pointer.move_to(end_x, end_y);
    client.roundtrip();

    EXPECT_THAT(client.window_under_cursor(), Eq(static_cast<struct wl_surface*>(surface)));
    EXPECT_THAT(client.pointer_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(end_x - window_x - dx),
                    wl_fixed_from_int(end_y - window_y - dy))));

    client.roundtrip();
}

TEST_F(XdgToplevelStableTest, pointer_leaves_surface_during_interactive_resize)
{
    int window_x = 100, window_y = 100;
    int window_width = 420, window_height = 390;
    int start_x = window_x + 5, start_y = window_y + 5;

    wlcs::Client client{the_server()};
    wlcs::Surface surface{client};
    wlcs::XdgSurfaceStable xdg_shell_surface{client, surface};
    wlcs::XdgToplevelStable toplevel{xdg_shell_surface};
    surface.attach_buffer(window_width, window_height);
    wl_surface_commit(surface);
    client.roundtrip();

    the_server().move_surface_to(surface, window_x, window_y);

    auto pointer = the_server().create_pointer();

    bool button_down{false};
    uint32_t last_serial{0};

    client.add_pointer_button_notification([&](uint32_t serial, uint32_t, bool is_down) -> bool {
            last_serial = serial;
            button_down = is_down;
            return true;
        });

    pointer.move_to(start_x, start_y);
    pointer.left_button_down();

    client.dispatch_until([&](){
            return button_down;
        });

    xdg_toplevel_resize(toplevel, client.seat(), last_serial, XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT);
    client.dispatch_until([&](){
            return !client.window_under_cursor();
        });
}

TEST_F(XdgToplevelStableTest, parent_can_be_set)
{
    const int window_pos_x = 200, window_pos_y = 280;

    wlcs::Client client{the_server()};

    ConfigurationWindow parent{client};
    the_server().move_surface_to(parent.surface, window_pos_x, window_pos_y);

    ConfigurationWindow child{client};
    the_server().move_surface_to(child.surface, window_pos_x, window_pos_y);

    xdg_toplevel_set_parent(child, parent);
    wl_surface_commit(child.surface);
    client.roundtrip();
}

TEST_F(XdgToplevelStableTest, null_parent_can_be_set)
{
    const int window_pos_x = 200, window_pos_y = 280;

    wlcs::Client client{the_server()};
    ConfigurationWindow window{client};
    the_server().move_surface_to(window.surface, window_pos_x, window_pos_y);

    xdg_toplevel_set_parent(window, nullptr);
    wl_surface_commit(window.surface);
    client.roundtrip();
}

TEST_F(XdgToplevelStableTest, when_parent_is_set_to_self_error_is_raised)
{
    wlcs::Client client{the_server()};
    ConfigurationWindow window{client};
    xdg_toplevel_set_parent(window, window);
    wl_surface_commit(window);
    try
    {
        client.roundtrip();
    }
    catch (wlcs::ProtocolError const& err)
    {
        return;
    }
    FAIL() << "Protocol error not raised";
}

TEST_F(XdgToplevelStableTest, when_parent_is_set_to_child_descendant_error_is_raised)
{
    wlcs::Client client{the_server()};
    ConfigurationWindow parent{client};
    ConfigurationWindow child{client};
    xdg_toplevel_set_parent(child, parent);
    wl_surface_commit(child);
    client.roundtrip();

    ConfigurationWindow grandchild{client};
    xdg_toplevel_set_parent(grandchild, child);
    wl_surface_commit(grandchild);
    client.roundtrip();

    xdg_toplevel_set_parent(parent, grandchild);
    wl_surface_commit(parent);

    try
    {
        client.roundtrip();
    }
    catch (wlcs::ProtocolError const& err)
    {
        return;
    }
    FAIL() << "Protocol error not raised";
}

using XdgToplevelStableConfigurationTest = wlcs::InProcessServer;

TEST_F(XdgToplevelStableConfigurationTest, defaults)
{
    wlcs::Client client{the_server()};
    ConfigurationWindow window{client};
    auto& state = window.state;

    // default values
    EXPECT_THAT(state.width, Eq(0));
    EXPECT_THAT(state.height, Eq(0));
    EXPECT_THAT(state.maximized, Eq(false));
    EXPECT_THAT(state.fullscreen, Eq(false));
    EXPECT_THAT(state.resizing, Eq(false));
    EXPECT_THAT(state.activated, Eq(true));
}

TEST_F(XdgToplevelStableConfigurationTest, window_can_maximize_itself)
{
    wlcs::Client client{the_server()};
    ConfigurationWindow window{client};
    auto& state = window.state;

    ASSERT_THAT(state.maximized, Eq(false)) << "test could not run as precondition failed";

    xdg_toplevel_set_maximized(window);
    window.dispatch_until_configure();

    EXPECT_THAT(state.width, Gt(0));
    EXPECT_THAT(state.height, Gt(0));
    EXPECT_THAT(state.maximized, Eq(true));
    EXPECT_THAT(state.fullscreen, Eq(false));
    EXPECT_THAT(state.resizing, Eq(false));
    EXPECT_THAT(state.activated, Eq(true));
}

TEST_F(XdgToplevelStableConfigurationTest, window_can_unmaximize_itself)
{
    wlcs::Client client{the_server()};
    ConfigurationWindow window{client};
    auto& state = window.state;

    xdg_toplevel_set_maximized(window);
    window.dispatch_until_configure();

    ASSERT_THAT(state.maximized, Eq(true)) << "test could not run as precondition failed";

    xdg_toplevel_unset_maximized(window);
    window.dispatch_until_configure();

    EXPECT_THAT(state.maximized, Eq(false));
    EXPECT_THAT(state.fullscreen, Eq(false));
    EXPECT_THAT(state.resizing, Eq(false));
    EXPECT_THAT(state.activated, Eq(true));
}

TEST_F(XdgToplevelStableConfigurationTest, window_can_fullscreen_itself)
{
    wlcs::Client client{the_server()};
    ConfigurationWindow window{client};
    auto& state = window.state;

    xdg_toplevel_set_fullscreen(window, nullptr);
    window.dispatch_until_configure();

    EXPECT_THAT(state.width, Gt(0));
    EXPECT_THAT(state.height, Gt(0));
    EXPECT_THAT(state.maximized, Eq(false));
    EXPECT_THAT(state.fullscreen, Eq(true));
    EXPECT_THAT(state.resizing, Eq(false));
    EXPECT_THAT(state.activated, Eq(true));
}

TEST_F(XdgToplevelStableConfigurationTest, window_can_unfullscreen_itself)
{
    wlcs::Client client{the_server()};
    ConfigurationWindow window{client};
    auto& state = window.state;

    xdg_toplevel_set_fullscreen(window, nullptr);
    window.dispatch_until_configure();

    ASSERT_THAT(state.fullscreen, Eq(true)) << "test could not run as precondition failed";

    xdg_toplevel_unset_fullscreen(window);
    window.dispatch_until_configure();

    EXPECT_THAT(state.maximized, Eq(false));
    EXPECT_THAT(state.fullscreen, Eq(false));
    EXPECT_THAT(state.resizing, Eq(false));
    EXPECT_THAT(state.activated, Eq(true));
}

TEST_F(XdgToplevelStableConfigurationTest, DISABLED_window_stays_maximized_after_fullscreen)
{
    wlcs::Client client{the_server()};
    ConfigurationWindow window{client};
    auto& state = window.state;

    xdg_toplevel_set_maximized(window);
    window.dispatch_until_configure();

    ASSERT_THAT(state.maximized, Eq(true)) << "test could not run as precondition failed";

    xdg_toplevel_set_fullscreen(window, nullptr);
    window.dispatch_until_configure();

    ASSERT_THAT(state.fullscreen, Eq(true)) << "test could not run as precondition failed";

    xdg_toplevel_unset_fullscreen(window);
    window.dispatch_until_configure();

    EXPECT_THAT(state.width, Gt(0));
    EXPECT_THAT(state.height, Gt(0));
    EXPECT_THAT(state.maximized, Eq(true));
    EXPECT_THAT(state.fullscreen, Eq(false));
    EXPECT_THAT(state.resizing, Eq(false));
    EXPECT_THAT(state.activated, Eq(true));
}

TEST_F(XdgToplevelStableConfigurationTest, DISABLED_window_can_maximize_itself_while_fullscreen)
{
    wlcs::Client client{the_server()};
    ConfigurationWindow window{client};
    auto& state = window.state;

    ASSERT_THAT(state.maximized, Eq(false)) << "test could not run as precondition failed";

    xdg_toplevel_set_fullscreen(window, nullptr);
    window.dispatch_until_configure();

    ASSERT_THAT(state.fullscreen, Eq(true)) << "test could not run as precondition failed";

    xdg_toplevel_set_maximized(window);
    window.dispatch_until_configure();

    EXPECT_THAT(state.maximized, Eq(true));

    xdg_toplevel_unset_fullscreen(window);
    window.dispatch_until_configure();

    EXPECT_THAT(state.width, Gt(0));
    EXPECT_THAT(state.height, Gt(0));
    EXPECT_THAT(state.maximized, Eq(true));
    EXPECT_THAT(state.fullscreen, Eq(false));
    EXPECT_THAT(state.resizing, Eq(false));
    EXPECT_THAT(state.activated, Eq(true));
}

TEST_F(XdgToplevelStableConfigurationTest, activated_state_follows_pointer)
{
    wlcs::Client client{the_server()};

    ConfigurationWindow window_a{client};
    auto& state_a = window_a.state;
    int const a_x = 12, a_y = 15;
    the_server().move_surface_to(window_a, a_x, a_y);

    ConfigurationWindow window_b{client};
    auto& state_b = window_b.state;
    int const b_x = a_x + window_a.window_width + 27, b_y = 15;
    the_server().move_surface_to(window_b, b_x, b_y);

    auto pointer = the_server().create_pointer();

    pointer.move_to(a_x + 10, a_y + 10);
    pointer.left_click();
    client.roundtrip();

    ASSERT_THAT(state_a.activated, Eq(true));
    ASSERT_THAT(state_b.activated, Eq(false));

    pointer.move_to(b_x + 10, b_y + 10);
    pointer.left_click();
    client.roundtrip();

    EXPECT_THAT(state_a.activated, Eq(false));
    EXPECT_THAT(state_b.activated, Eq(true));
}

// How to repro (manually)
//
// 1. Make sure you're running Mir 2.17 or earlier (it's fixed on main by
// db0f621a0f79721f32222d7184aa7587e3bde8dc)
//
// 2. Open a QT application (bomber, qtcreator, etc..). Note that GNOME apps
// don't seem to suffer from this bug
//
// 3. Press the left mouse button on a resize edge and move the edge.
//
// You should see the application rapidly change between two sizes, the effect
// is more visible if you flick the mouse instead of moving it gently
//
//
//
// This test doesn't seem to properly reproduce this bug, this is most likely
// related to the code reacting to `xdg_toplevel.configure` not being 1:1 with
// QT
//
// A quick rundown of what it does:
//  1. Create window
//  2. Move cursor to left edge
//  3. Press left mouse button
//  4. Move the mouse on the X axis with different offsets
//  5. See how many times `toplevel.configure` was called
//
// If the bug occurs, `toplevel.configure` should be called A LOT, doesn't seem
// to happen as of right now.
//
// A snippet of `WAYLAND_DEBUG=client bomber` when the bug occurs:
// [3973075.596] xdg_toplevel@28.configure(701, 774, array[4])
// [3973075.602] xdg_surface@27.configure(1275)
// [3973075.613] wl_buffer@39.release()
// [3973075.790]  -> xdg_surface@27.set_window_geometry(0, 0, 701, 774)
// [3973075.800]  -> wl_compositor@4.create_region(new id wl_region@37)
// [3973075.805]  -> wl_region@37.add(3, 30, 695, 741)
// [3973075.809]  -> wl_surface@22.set_opaque_region(wl_region@37)
// [3973075.811]  -> wl_region@37.destroy()
// [3973075.815]  -> xdg_surface@27.ack_configure(1275)
// [3973076.629] wl_display@1.delete_id(41)
// [3973076.642] wl_display@1.delete_id(29)
// [3973076.648] wl_display@1.delete_id(35)
// [3973076.654] wl_callback@35.done(3818883)
// [3973077.260]  -> zwp_text_input_v2@12.update_state(49, 0)
// [3973077.341]  -> wl_shm_pool@40.destroy()
// [3973077.349]  -> wl_buffer@39.destroy()
// [3973077.395]  -> wl_shm@13.create_pool(new id wl_shm_pool@35, fd 83, 2170296)
// [3973077.401]  -> wl_shm_pool@35.create_buffer(new id wl_buffer@29, 0, 701, 774, 2804, 0)
// [3973079.006] wl_display@1.delete_id(37)
// [3973084.796]  -> wl_surface@22.damage_buffer(0, 0, 701, 30)
// [3973084.811]  -> wl_surface@22.damage_buffer(0, 30, 3, 741)
// [3973084.815]  -> wl_surface@22.damage_buffer(698, 30, 3, 741)
// [3973084.818]  -> wl_surface@22.damage_buffer(0, 771, 701, 3)
// [3973084.822]  -> wl_surface@22.damage_buffer(0, 774, 3, 30)
// [3973084.932]  -> wl_surface@22.frame(new id wl_callback@37)
// [3973084.945]  -> wl_surface@22.attach(wl_buffer@29, 0, 0)
// [3973084.952]  -> wl_surface@22.damage_buffer(3, 30, 695, 741)
// [3973084.955]  -> wl_surface@22.commit()
// [3973084.995] wl_pointer@16.leave(1276, wl_surface@22)
// [3973085.006] wl_pointer@16.frame()
// [3973085.011] xdg_toplevel@28.configure(327, 774, array[4])
// [3973085.018] xdg_surface@27.configure(1278)
// [3973085.026] wl_buffer@38.release()
TEST_F(XdgToplevelStableTest, no_ping_pong)
{
    int const window_x = 100, window_y = 100;
    int const initial_width = 420, initial_height = 390;
    int const start_x = window_x + 5, start_y = window_y + 5;

    wlcs::Client client{the_server()};
    wlcs::Surface surface{client};

    wlcs::XdgSurfaceStable xdg_shell_surface{client, surface};
    wlcs::XdgToplevelStable toplevel{xdg_shell_surface};

    surface.attach_visible_buffer(initial_width, initial_height);
    xdg_surface_set_window_geometry(xdg_shell_surface, 0, 0, initial_width, initial_height);
    wl_surface_commit(surface);
    client.roundtrip();

    the_server().move_surface_to(surface, window_x, window_y);

    auto pointer = the_server().create_pointer();

    bool button_down{false};
    uint32_t last_serial{0};

    client.add_pointer_button_notification([&](uint32_t serial, uint32_t, bool is_down) -> bool {
            last_serial = serial;
            button_down = is_down;
            return true;
        });

    pointer.move_to(start_x, start_y);
    pointer.left_button_down();

    client.dispatch_until([&](){
            return button_down;
        });

    wl_surface_commit(surface);
    client.roundtrip();

    auto const start = 2, end = 17;
    auto const count = end - start + 1;

    EXPECT_CALL(xdg_shell_surface, configure)
        .Times(Exactly(count)) // Should fail if the bug occurs
        .WillRepeatedly(
            [&](auto serial)
            {
                xdg_surface_ack_configure(xdg_shell_surface, serial);
            });

    auto width = initial_width, height = initial_height;

    EXPECT_CALL(toplevel, configure)
        .Times(Exactly(count)) // Should fail if the bug occurs
        .WillRepeatedly(
            [&](int32_t server_requested_width, int32_t server_requested_height, wl_array*)
            {
                if(width != server_requested_width || height != server_requested_height)
                {
                    width = server_requested_width;
                    height = server_requested_height;

                    // Playing around trying to get close to bomber's log
                    // Order doesn't matter, right???
                    xdg_surface_set_window_geometry(xdg_shell_surface, 0, 0, width, height);
                    wl_region* region = wl_compositor_create_region(client.compositor());
                    wl_region_add(region, 0, 0, width, height);
                    wl_surface_set_opaque_region(surface, region);
                    wl_region_destroy(region);

                    auto& buffer = client.create_buffer(width, height);
                    wl_surface_attach(surface, buffer, 0, 0);
                    wl_surface_damage_buffer(surface, 0, 0, width, height);

                    wl_surface_commit(surface);
                    client.roundtrip();
                }
            });


    auto sum = 0;
    for(int i = start; i <= end; i++)
    {
        xdg_toplevel_resize(toplevel, client.seat(), last_serial, XDG_TOPLEVEL_RESIZE_EDGE_LEFT);
        client.roundtrip();

        auto t = float(i) / 2.0;

        sum += t;
        assert(sum < 100.0 && "No more space to resize to the left!");

        pointer.move_by(-t, 0);
        client.roundtrip();
    }

    // Not strictly necessary, the bug occurs even if you don't let go of the
    // left mouse button
    pointer.left_button_up();
    client.roundtrip();
}
