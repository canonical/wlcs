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

#include "helpers.h"
#include "in_process_server.h"

#include <gmock/gmock.h>

using namespace testing;
using namespace wlcs;

namespace
{

struct FrameSubmission : StartedInProcessServer
{
    Client client{the_server()};
    Surface surface{client};

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
    wl_surface_damage(surface, 0, 0, 200, 200);
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
    ShmBuffer buffer{client, 200, 200};
    wl_surface_attach(surface, buffer, 0, 0);

    auto shell_surface = wl_shell_get_shell_surface(client.shell(), surface);
    wl_shell_surface_set_toplevel(shell_surface);

    for (auto i = 0; i != 10; ++i)
    {
        auto frame_consumed = false;

        submit_frame(frame_consumed);
        wait_for_frame(frame_consumed);

        EXPECT_THAT(frame_consumed, Eq(true));
    }
}
