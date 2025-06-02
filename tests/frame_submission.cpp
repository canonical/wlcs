/*
 * Copyright © 2018 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#include "in_process_server.h"

#include <gmock/gmock.h>

using namespace testing;
using namespace wlcs;

namespace
{

struct FrameSubmission : StartedInProcessServer
{
    Client client{the_server()};
    Surface surface{client.create_visible_surface(200, 200)};

    void submit_frame(bool& frame_consumed);

    void wait_for_frame(bool const& frame_consumed);
};

void FrameSubmission::submit_frame(bool& consumed_flag)
{
    static wl_callback_listener const frame_listener
    {
        [](void *data, struct wl_callback* callback, uint32_t /*callback_data*/)
        {
            *static_cast<bool*>(data) = true;
            wl_callback_destroy(callback);
        }
    };

    consumed_flag = false;
    wl_callback_add_listener(wl_surface_frame(surface), &frame_listener, &consumed_flag);
    auto buffer = std::make_shared<wlcs::ShmBuffer>(client, 200, 200);
    wl_surface_attach(surface, *buffer, 0, 0);
    wl_surface_commit(surface);
}

void FrameSubmission::wait_for_frame(bool const& consumed_flag)
{
    // TODO timeout
    client.dispatch_until([&consumed_flag]() { return consumed_flag; });
}
}

TEST_F(FrameSubmission, post_one_frame_at_a_time)
{
    for (auto i = 0; i != 10; ++i)
    {
        auto frame_consumed = false;

        submit_frame(frame_consumed);
        wait_for_frame(frame_consumed);

        EXPECT_THAT(frame_consumed, Eq(true));
    }
}

// Regression test for https://github.com/MirServer/mir/issues/2960
TEST_F(FrameSubmission, test_buffer_can_be_deleted_after_attached)
{
    using namespace testing;

    wlcs::Client client{the_server()};
    auto surface = client.create_visible_surface(200, 200);

    auto buffer = std::make_shared<wlcs::ShmBuffer>(client, 200, 200);
    wl_surface_attach(surface, *buffer, 0, 0);
    buffer.reset();
    /* Check that the destroyed buffer doesn't crash the server
     * It's not clear what the *correct* behaviour is in this case:
     * Weston treats this as committing a null buffer, wlroots appears to
     * treat this as committing the content of the buffer before deletion.
     *
     * *Whatever* the correct behaviour is, "crash" is *definitely*
     * incorrect.
     */
    wl_surface_commit(surface);

    // Roundtrip to ensure the server has processed our weirdness
    client.roundtrip();
}

/* Firefox has a lovely habit of sending an endless stream of
 * wl_surface.frame() requests (maybe it's trying to estimate vsync?!)
 *
 * If a compositor responds immediately to the frame on commit, Firefox
 * ends up looping endlessly. If the compositor *doesn't* respond to the frame
 * request, Firefox decides not to draw anything. 🤷‍♀️
 */
TEST_F(FrameSubmission, when_client_endlessly_requests_frame_then_callbacks_are_throttled)
{
    using namespace testing;
    using namespace std::chrono_literals;

    wlcs::Client annoying_client{the_server()};
    auto surface = client.create_visible_surface(200, 200);

    bool frame_callback_called{false};

    wl_callback_listener const listener = {
        .done = [](void* data, struct wl_callback* callback, [[maybe_unused]]uint32_t timestamp)
        {
            wl_callback_destroy(callback);

            auto callback_called = static_cast<bool*>(data);
            *callback_called = true;
        }
    };

    auto const timeout = std::chrono::steady_clock::now() + 10s;

    do
    {
        frame_callback_called = false;
        wl_callback_add_listener(wl_surface_frame(surface), &listener, &frame_callback_called);
        wl_surface_commit(surface);
        client.roundtrip();
            /* This roundtrip ensures the server has processed everything prior.
             * In particular, if the server sends the frame callback in response to wl_surface.commit, that frame callback
             * will have been processed by now.
             */
    }
    while (frame_callback_called && std::chrono::steady_clock::now() < timeout);

    EXPECT_THAT(std::chrono::steady_clock::now(), Lt(timeout)) << "Timed out looping in frame callback storm";
}

/*
 * The above test checks that frame callbacks are throttled, but not that
 * they are sent at all.
 *
 * We also need to test that these type of buffer-less frame requests
 * *do* get a callback, eventually.
 */
TEST_F(FrameSubmission, frame_requests_without_buffer_are_called_back_eventually)
{
    std::array<bool, 5> called{};

    wlcs::Client client{the_server()};
    auto surface = client.create_visible_surface(640, 480);

    for (auto i = 0u; i < called.size(); ++i)
    {
        surface.add_frame_callback([&called, i](auto) { called[i] = true; });
        wl_surface_commit(surface);
    }

    client.dispatch_until([&called]() { return std::all_of(called.begin(), called.end(), [](bool value) { return value; }); });
}
