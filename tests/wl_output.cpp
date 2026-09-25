/*
 * Copyright © 2019 Canonical Ltd.
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
 * Authored by: William Wold <william.wold@canonical.com>
 */

#include "in_process_server.h"
#include "version_specifier.h"

#include <gmock/gmock.h>

using namespace testing;
using namespace wlcs;

namespace
{
/// Forwards wl_output events to mock methods.
class WlOutputListener
{
public:
    WlOutputListener(wl_output* output);

    MOCK_METHOD(void, geometry,
        (int32_t x, int32_t y, int32_t physical_width, int32_t physical_height,
         int32_t subpixel, char const* make, char const* model, int32_t transform));
    MOCK_METHOD(void, mode, (uint32_t flags, int32_t width, int32_t height, int32_t refresh));
    MOCK_METHOD(void, done, ());
    MOCK_METHOD(void, scale, (int32_t factor));
    MOCK_METHOD(void, name, (char const* name));
    MOCK_METHOD(void, description, (char const* description));
};

WlOutputListener::WlOutputListener(wl_output* output)
{
#define FORWARD_TO_MOCK(method) \
    [](void* data, wl_output*, auto... args){ static_cast<WlOutputListener*>(data)->method(args...); }
    static const wl_output_listener listener = {
        FORWARD_TO_MOCK(geometry),
        FORWARD_TO_MOCK(mode),
        FORWARD_TO_MOCK(done),
        FORWARD_TO_MOCK(scale),
        FORWARD_TO_MOCK(name),
        FORWARD_TO_MOCK(description),
    };
#undef FORWARD_TO_MOCK
    wl_output_add_listener(output, &listener, this);
}

/// True if the mode flags mark it as the output's current mode.
MATCHER(IsCurrentMode, "")
{
    return arg & WL_OUTPUT_MODE_CURRENT;
}
}

struct WlOutputTest : wlcs::InProcessServer
{
    /// Binds a fresh wl_output and dispatches until the server has finished
    /// advertising its properties (i.e. it has sent the initial wl_output.done).
    void receive_initial_properties(wlcs::Client& client, WlOutputListener& listener)
    {
        bool done = false;
        ON_CALL(listener, done()).WillByDefault(Invoke([&done]() { done = true; }));
        client.dispatch_until([&done]() { return done; });
    }
};

TEST_F(WlOutputTest, sends_done_after_initial_properties)
{
    wlcs::Client client{the_server()};

    auto const output = client.bind_if_supported<wl_output>(wlcs::AnyVersion);
    NiceMock<WlOutputListener> listener{output};

    EXPECT_CALL(listener, done()).Times(1);

    // A single roundtrip delivers the whole initial advertisement, so any
    // additional done events would also have arrived by now.
    client.roundtrip();
}

TEST_F(WlOutputTest, advertises_geometry)
{
    wlcs::Client client{the_server()};

    auto const output = client.bind_if_supported<wl_output>(wlcs::AnyVersion);
    NiceMock<WlOutputListener> listener{output};

    EXPECT_CALL(listener, geometry(
        _, _,
        Ge(0),  // physical_width
        Ge(0),  // physical_height
        AllOf(Ge(WL_OUTPUT_SUBPIXEL_UNKNOWN), Le(WL_OUTPUT_SUBPIXEL_VERTICAL_BGR)),
        _, _,
        AllOf(Ge(WL_OUTPUT_TRANSFORM_NORMAL), Le(WL_OUTPUT_TRANSFORM_FLIPPED_270))));

    receive_initial_properties(client, listener);
}

TEST_F(WlOutputTest, advertises_a_current_mode)
{
    wlcs::Client client{the_server()};

    auto const output = client.bind_if_supported<wl_output>(wlcs::AnyVersion);
    NiceMock<WlOutputListener> listener{output};

    EXPECT_CALL(listener, mode(IsCurrentMode(), Gt(0), Gt(0), Ge(0)));

    receive_initial_properties(client, listener);
}

TEST_F(WlOutputTest, advertises_scale)
{
    wlcs::Client client{the_server()};

    auto const output = client.bind_if_supported<wl_output>(wlcs::AnyVersion);
    NiceMock<WlOutputListener> listener{output};

    EXPECT_CALL(listener, scale(Ge(1)));

    receive_initial_properties(client, listener);
}

TEST_F(WlOutputTest, advertises_name)
{
    wlcs::Client client{the_server()};

    auto const output = client.bind_if_supported<wl_output>(wlcs::AtLeastVersion{WL_OUTPUT_NAME_SINCE_VERSION});
    NiceMock<WlOutputListener> listener{output};

    EXPECT_CALL(listener, name(StrNe("")));

    receive_initial_properties(client, listener);
}

TEST_F(WlOutputTest, release)
{
    wlcs::Client client{the_server()};

    {
        // Acquire *any* wl_output; we don't care which
        auto const output =
            client.bind_if_supported<wl_output>(wlcs::AtLeastVersion{WL_OUTPUT_RELEASE_SINCE_VERSION});
        client.roundtrip();
    }
    // output is now released

    client.roundtrip();
}
