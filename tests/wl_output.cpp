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

#include "helpers.h"
#include "in_process_server.h"
#include "version_specifier.h"

#include <gmock/gmock.h>

using namespace testing;
using namespace wlcs;

namespace
{
/// Forwards wl_output events to virtual methods so they can be mocked.
struct WlOutputListener
{
    WlOutputListener(wl_output* output)
    {
        wl_output_add_listener(output, &thunks, this);
    }

    virtual ~WlOutputListener() = default;

    WlOutputListener(WlOutputListener const&) = delete;
    WlOutputListener& operator=(WlOutputListener const&) = delete;

    virtual void geometry(
        wl_output* output,
        int32_t x,
        int32_t y,
        int32_t physical_width,
        int32_t physical_height,
        int32_t subpixel,
        char const* make,
        char const* model,
        int32_t transform) = 0;
    virtual void mode(
        wl_output* output,
        uint32_t flags,
        int32_t width,
        int32_t height,
        int32_t refresh) = 0;
    virtual void done(wl_output* output) = 0;
    virtual void scale(wl_output* output, int32_t factor) = 0;
    virtual void name(wl_output* output, char const* name) = 0;
    virtual void description(wl_output* output, char const* description) = 0;

private:
    static void geometry_thunk(
        void* data,
        wl_output* output,
        int32_t x,
        int32_t y,
        int32_t physical_width,
        int32_t physical_height,
        int32_t subpixel,
        char const* make,
        char const* model,
        int32_t transform)
    {
        static_cast<WlOutputListener*>(data)->geometry(
            output, x, y, physical_width, physical_height, subpixel, make, model, transform);
    }

    static void mode_thunk(
        void* data,
        wl_output* output,
        uint32_t flags,
        int32_t width,
        int32_t height,
        int32_t refresh)
    {
        static_cast<WlOutputListener*>(data)->mode(output, flags, width, height, refresh);
    }

    static void done_thunk(void* data, wl_output* output)
    {
        static_cast<WlOutputListener*>(data)->done(output);
    }

    static void scale_thunk(void* data, wl_output* output, int32_t factor)
    {
        static_cast<WlOutputListener*>(data)->scale(output, factor);
    }

    static void name_thunk(void* data, wl_output* output, char const* name)
    {
        static_cast<WlOutputListener*>(data)->name(output, name);
    }

    static void description_thunk(void* data, wl_output* output, char const* description)
    {
        static_cast<WlOutputListener*>(data)->description(output, description);
    }

    constexpr static wl_output_listener thunks = {
        &geometry_thunk,
        &mode_thunk,
        &done_thunk,
        &scale_thunk,
        &name_thunk,
        &description_thunk,
    };
};

constexpr wl_output_listener WlOutputListener::thunks;

struct MockWlOutputListener : WlOutputListener
{
    using WlOutputListener::WlOutputListener;

    MOCK_METHOD(void, geometry,
        (wl_output*, int32_t, int32_t, int32_t, int32_t, int32_t, char const*, char const*, int32_t),
        (override));
    MOCK_METHOD(void, mode, (wl_output*, uint32_t, int32_t, int32_t, int32_t), (override));
    MOCK_METHOD(void, done, (wl_output*), (override));
    MOCK_METHOD(void, scale, (wl_output*, int32_t), (override));
    MOCK_METHOD(void, name, (wl_output*, char const*), (override));
    MOCK_METHOD(void, description, (wl_output*, char const*), (override));
};

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
    void receive_initial_properties(wlcs::Client& client, MockWlOutputListener& listener)
    {
        bool done = false;
        EXPECT_CALL(listener, done(_)).WillRepeatedly(Invoke([&done](auto) { done = true; }));
        client.dispatch_until([&done]() { return done; });
    }
};

TEST_F(WlOutputTest, sends_done_after_initial_properties)
{
    wlcs::Client client{the_server()};

    auto const output = client.bind_if_supported<wl_output>(wlcs::AnyVersion);
    NiceMock<MockWlOutputListener> listener{output};

    EXPECT_CALL(listener, done(_)).Times(1);

    // A single roundtrip delivers the whole initial advertisement, so any
    // additional done events would also have arrived by now.
    client.roundtrip();
}

TEST_F(WlOutputTest, advertises_geometry)
{
    wlcs::Client client{the_server()};

    auto const output = client.bind_if_supported<wl_output>(wlcs::AnyVersion);
    NiceMock<MockWlOutputListener> listener{output};

    EXPECT_CALL(listener, geometry(
        _, _, _,
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
    NiceMock<MockWlOutputListener> listener{output};

    EXPECT_CALL(listener, mode(_, IsCurrentMode(), Gt(0), Gt(0), Ge(0)));

    receive_initial_properties(client, listener);
}

TEST_F(WlOutputTest, advertises_scale)
{
    wlcs::Client client{the_server()};

    auto const output = client.bind_if_supported<wl_output>(wlcs::AnyVersion);
    NiceMock<MockWlOutputListener> listener{output};

    EXPECT_CALL(listener, scale(_, Ge(1)));

    receive_initial_properties(client, listener);
}

TEST_F(WlOutputTest, advertises_name)
{
    wlcs::Client client{the_server()};

    auto const output = client.bind_if_supported<wl_output>(wlcs::AtLeastVersion{WL_OUTPUT_NAME_SINCE_VERSION});
    NiceMock<MockWlOutputListener> listener{output};

    EXPECT_CALL(listener, name(_, StrNe("")));

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
