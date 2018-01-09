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

#include <gmock/gmock.h>

using ClientSurfaceEventsTest = wlcs::InProcessServer;

//
//static void
//check_pointer(struct client *client, int x, int y)
//{
//	int sx, sy;
//
//	/* check that the client got the global pointer update */
//	assert(client->test->pointer_x == x);
//	assert(client->test->pointer_y == y);
//
//	/* Does global pointer map onto the surface? */
//	if (surface_contains(client->surface, x, y)) {
//		/* check that the surface has the pointer focus */
//		assert(client->input->pointer->focus == client->surface);
//
//		/*
//		 * check that the local surface pointer maps
//		 * to the global pointer.
//		 */
//		sx = client->input->pointer->x + client->surface->x;
//		sy = client->input->pointer->y + client->surface->y;
//		assert(sx == x);
//		assert(sy == y);
//	} else {
//		/*
//		 * The global pointer does not map onto surface.  So
//		 * check that it doesn't have the pointer focus.
//		 */
//		assert(client->input->pointer->focus == NULL);
//	}
//}
//
//static void
//check_pointer_move(struct client *client, int x, int y)
//{
//	weston_test_move_pointer(client->test->weston_test, x, y);
//	client_roundtrip(client);
//	check_pointer(client, x, y);
//}
//
//TEST(test_pointer_surface_move)
//{
//	struct client *client;
//
//	client = create_client_and_test_surface(100, 100, 100, 100);
//	assert(client);
//
//	/* move pointer outside of client */
//	assert(!surface_contains(client->surface, 50, 50));
//	check_pointer_move(client, 50, 50);
//
//	/* move client center to pointer */
//	move_client(client, 0, 0);
//	assert(surface_contains(client->surface, 50, 50));
//	check_pointer(client, 50, 50);
//}
//
//static int
//output_contains_client(struct client *client)
//{
//	struct output *output = client->output;
//	struct surface *surface = client->surface;
//
//	return !(output->x >= surface->x + surface->width
//		|| output->x + output->width <= surface->x
//		|| output->y >= surface->y + surface->height
//		|| output->y + output->height <= surface->y);
//}
//
//static void
//check_client_move(struct client *client, int x, int y)
//{
//	move_client(client, x, y);
//
//	if (output_contains_client(client)) {
//		assert(client->surface->output == client->output);
//	} else {
//		assert(client->surface->output == NULL);
//	}
//}
//
//TEST(test_surface_output)
//{
//	struct client *client;
//	int x, y;
//
//	client = create_client_and_test_surface(100, 100, 100, 100);
//	assert(client);
//
//	assert(output_contains_client(client));
//
//	/* not visible */
//	x = 0;
//	y = -client->surface->height;
//	check_client_move(client, x, y);
//
//	/* visible */
//	check_client_move(client, x, ++y);
//
//	/* not visible */
//	x = -client->surface->width;
//	y = 0;
//	check_client_move(client, x, y);
//
//	/* visible */
//	check_client_move(client, ++x, y);
//
//	/* not visible */
//	x = client->output->width;
//	y = 0;
//	check_client_move(client, x, y);
//
//	/* visible */
//	check_client_move(client, --x, y);
//	assert(output_contains_client(client));
//
//	/* not visible */
//	x = 0;
//	y = client->output->height;
//	check_client_move(client, x, y);
//	assert(!output_contains_client(client));
//
//	/* visible */
//	check_client_move(client, x, --y);
//	assert(output_contains_client(client));
//}

struct PointerMotion
{
    static int constexpr window_width = 231;
    static int constexpr window_height = 220;
    std::string name;
    int initial_x, initial_y; // Relative to surface top-left
    int dx, dy;
};

std::ostream& operator<<(std::ostream& out, PointerMotion const& motion)
{
    return out << motion.name;
}

class SurfacePointerMotionTest :
    public wlcs::InProcessServer,
    public testing::WithParamInterface<PointerMotion>
{
};

TEST_P(SurfacePointerMotionTest, pointer_movement)
{
    using namespace testing;

    auto pointer = the_server().create_pointer();

    wlcs::Client client{the_server()};

    auto const params = GetParam();

    auto surface = client.create_visible_surface(
        params.window_width,
        params.window_height);

    int const top_left_x = 23, top_left_y = 231;
    the_server().move_surface_to(surface, top_left_x, top_left_y);

    auto const wl_surface = static_cast<struct wl_surface*>(surface);

    pointer.move_to(top_left_x + params.initial_x, top_left_y + params.initial_y);

    client.roundtrip();

    EXPECT_THAT(client.focused_window(), Ne(wl_surface));

    /* move pointer; it should now be inside the surface */
    pointer.move_by(params.dx, params.dy);

    client.roundtrip();

    EXPECT_THAT(client.focused_window(), Eq(wl_surface));
    EXPECT_THAT(client.pointer_position(),
                Eq(std::make_pair(
                    wl_fixed_from_int(params.initial_x + params.dx),
                    wl_fixed_from_int(params.initial_y + params.dy))));

    /* move pointer back; it should now be outside the surface */
    pointer.move_by(-params.dx, -params.dy);

    client.roundtrip();
    EXPECT_THAT(client.focused_window(), Ne(wl_surface));
}

INSTANTIATE_TEST_CASE_P(
    PointerCrossingSurfaceCorner,
    SurfacePointerMotionTest,
    testing::Values(
        PointerMotion{"Top-left", -1, -1, 1, 1},
        PointerMotion{"Bottom-left", -1, PointerMotion::window_height, 1, -1},
        PointerMotion{"Bottom-right", PointerMotion::window_width, PointerMotion::window_height, -1, -1},
        PointerMotion{"Top-right", PointerMotion::window_width, -1, -1, 1}
    ));

INSTANTIATE_TEST_CASE_P(
    PointerCrossingSurfaceEdge,
    SurfacePointerMotionTest,
    testing::Values(
        PointerMotion{
            "Centre-left",
            -1, PointerMotion::window_height / 2,
            1, 0},
        PointerMotion{
            "Bottom-centre",
            PointerMotion::window_width / 2, PointerMotion::window_height,
            0, -1},
        PointerMotion{
            "Centre-right",
            PointerMotion::window_width, PointerMotion::window_height / 2,
            -1, 0},
        PointerMotion{
            "Top-centre",
            PointerMotion::window_width / 2, -1,
            0, 1}
    ));

TEST_F(ClientSurfaceEventsTest, buffer_release)
{
    wlcs::Client client{the_server()};

    auto surface = client.create_visible_surface(100, 100);

    std::array<wlcs::ShmBuffer, 3> buffers = {{
        wlcs::ShmBuffer{client, 100, 100},
        wlcs::ShmBuffer{client, 100, 100},
        wlcs::ShmBuffer{client, 100, 100}
    }};
    std::array<bool, 3> buffer_released = {{
        false,
        false,
        false
    }};

    for (auto i = 0u; i < buffers.size(); ++i)
    {
        buffers[i].add_release_listener(
            [released = &buffer_released[i]]()
            {
                *released = true;
                return false;
            });
    }

    /*
     * The first buffer must never be released, since it is replaced before
     * it is committed, therefore it never becomes busy.
     */
    wl_surface_attach(surface, buffers[0], 0, 0);
    wl_surface_attach(surface, buffers[1], 0, 0);

    bool frame_consumed{false};
    surface.add_frame_callback(
        [&frame_consumed](auto)
        {
            frame_consumed = true;
        });
    wl_surface_commit(surface);

    client.dispatch_until(
        [&frame_consumed]()
        {
            return frame_consumed;
        });

    EXPECT_FALSE(buffer_released[0]);
    // buffers[1] may or may not be released
    EXPECT_FALSE(buffer_released[2]);

    wl_surface_attach(surface, buffers[2], 0, 0);

    frame_consumed = false;
    surface.add_frame_callback(
        [&frame_consumed](auto)
        {
            frame_consumed = true;
        });
    wl_surface_commit(surface);

    client.dispatch_until(
        [&frame_consumed]()
        {
            return frame_consumed;
        });
    EXPECT_FALSE(buffer_released[0]);
    EXPECT_TRUE(buffer_released[1]);
    // buffer[2] may or may not be released

    wlcs::ShmBuffer final_buffer(client, 100, 100);
    wl_surface_attach(surface, final_buffer, 0, 0);

    frame_consumed = false;
    surface.add_frame_callback(
        [&frame_consumed](auto)
        {
            frame_consumed = true;
        });
    wl_surface_commit(surface);

    client.dispatch_until(
        [&frame_consumed]()
        {
            return frame_consumed;
        });

    EXPECT_FALSE(buffer_released[0]);
    EXPECT_TRUE(buffer_released[1]);
    EXPECT_TRUE(buffer_released[2]);
}
