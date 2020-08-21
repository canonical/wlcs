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

#include <experimental/optional>

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
        xdg_shell_surface.add_configure_notification([&](uint32_t serial)
            {
                xdg_surface_ack_configure(xdg_shell_surface, serial);
                surface_configure_count++;
            });

        toplevel.add_configure_notification([this](int32_t width, int32_t height, struct wl_array *states)
            {
                state = wlcs::XdgToplevelStable::State{width, height, states};
            });

        wl_surface_commit(surface);
        client.roundtrip();
        surface.attach_buffer(window_width, window_height);
        wl_surface_commit(surface);
        dispatch_until_configure();
    }

    ~ConfigurationWindow()
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
