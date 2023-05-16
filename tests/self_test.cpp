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

#include "data_device.h"
#include "helpers.h"
#include "gtest_helpers.h"
#include "in_process_server.h"
#include "version_specifier.h"

#include <gmock/gmock.h>

#include <memory>

using namespace testing;
using namespace wlcs;

namespace
{

struct SelfTest : StartedInProcessServer
{
    Client client1{the_server()};
};

auto static const any_width = 100;
auto static const any_height = 100;
}

TEST_F(SelfTest, when_creating_second_client_nothing_bad_happens)
{
    Client client2{the_server()};
}

TEST_F(SelfTest, given_second_client_when_roundtripping_first_client_nothing_bad_happens)
{
    Client client2{the_server()};
    client1.roundtrip();
}

TEST_F(SelfTest, given_second_client_when_roundtripping_both_clients_nothing_bad_happens)
{
    Client client2{the_server()};

    for (auto i = 0; i != 10; ++i)
    {
        client1.roundtrip();
        client2.roundtrip();
    }
}

TEST_F(SelfTest, when_a_client_creates_a_surface_nothing_bad_happens)
{
    Surface const surface1{client1.create_visible_surface(any_width, any_height)};
    client1.roundtrip();
}

TEST_F(SelfTest, given_second_client_when_first_creates_a_surface_nothing_bad_happens)
{
    Client client2{the_server()};

    Surface const surface1{client1.create_visible_surface(any_width, any_height)};

    for (auto i = 0; i != 10; ++i)
    {
        client1.roundtrip();
        client2.roundtrip();
    }
}

TEST_F(SelfTest, given_second_client_when_both_create_a_surface_nothing_bad_happens)
{
    Client client2{the_server()};

    Surface const surface1{client1.create_visible_surface(any_width, any_height)};

    Surface const surface2{client2.create_visible_surface(any_width, any_height)};

    for (auto i = 0; i != 10; ++i)
    {
        client1.roundtrip();
        client2.roundtrip();
    }
}

TEST_F(SelfTest, xfail_failure_is_noted)
{
    ::testing::Test::RecordProperty("wlcs-skip-test", "Reason goes here");

    FAIL() << "This message shouldn't be seen";
}

TEST_F(SelfTest, expected_missing_extension_is_xfail)
{
    throw wlcs::ExtensionExpectedlyNotSupported("xdg_not_really_an_extension", wlcs::AtLeastVersion{1});
}

TEST_F(SelfTest, acquiring_unsupported_extension_is_xfail)
{
    auto const extension_list = the_server().supported_extensions();

    if (!extension_list)
    {
        ::testing::Test::RecordProperty("wlcs-skip-test", "Compositor Integration module is too old for expected extension failures");
        FAIL() << "Requires unsupported feature from module under test";
    }

    Client client{the_server()};

    wl_interface unsupported_interface = wl_shell_interface;
    unsupported_interface.name = "wlcs_non_existent_extension";
    client.bind_if_supported(unsupported_interface, AnyVersion);

    FAIL() << "We should have (x)failed at acquiring the interface";
}

TEST_F(SelfTest, acquiring_unsupported_extension_version_is_xfail)
{
    auto const extension_list = the_server().supported_extensions();

    if (!extension_list)
    {
        ::testing::Test::RecordProperty("wlcs-skip-test", "Compositor Integration module is too old for expected extension failures");
        FAIL() << "Requires unsupported feature from module under test";
    }

    Client client{the_server()};

    wl_interface interface_with_unsupported_version = wl_shell_interface;
    interface_with_unsupported_version.version += 1;
    client.bind_if_supported(
        interface_with_unsupported_version,
        AtLeastVersion{static_cast<uint32_t>(interface_with_unsupported_version.version)});

    FAIL() << "We should have (x)failed at acquiring the interface";
}

TEST_F(SelfTest, does_not_acquire_version_newer_than_wlcs_supports)
{
    auto const extension_list = the_server().supported_extensions();

    if (!extension_list)
    {
        ::testing::Test::RecordProperty("wlcs-skip-test", "Compositor Integration module is too old for expected extension failures");
        FAIL() << "Requires unsupported feature from module under test";
    }

    Client client{the_server()};

    auto const proxy_latest = static_cast<wl_seat*>(client.bind_if_supported(wl_seat_interface, AnyVersion));
    ASSERT_THAT(wl_seat_get_version(proxy_latest), Gt(1));

    wl_interface interface_with_old_version = wl_seat_interface;
    interface_with_old_version.version = wl_seat_get_version(proxy_latest) - 1;
    auto const proxy_old = static_cast<wl_seat*>(client.bind_if_supported(interface_with_old_version, AnyVersion));
    EXPECT_THAT(wl_seat_get_version(proxy_old), Eq(interface_with_old_version.version));

    wl_seat_destroy(proxy_latest);
    wl_seat_destroy(proxy_old);
}

TEST_F(SelfTest, dispatch_until_times_out_on_failure)
{
    Client client{the_server()};

    // Ensure that there's some events happening on the Wayland socket
    auto dummy = client.create_visible_surface(300, 300);
    dummy.attach_buffer(300, 300);
    wl_surface_commit(dummy);

    try
    {
        client.dispatch_until([]() { return false; }, std::chrono::seconds{1});
    }
    catch (wlcs::Timeout const&)
    {
        return;
    }
    FAIL() << "Dispatch did not raise a wlcs::Timeout exception";
}

TEST_F(SelfTest, dispatch_until_times_out_at_the_right_time)
{
    using namespace std::literals::chrono_literals;

    Client client{the_server()};

    auto const timeout = 5s;
    auto const expected_end = std::chrono::steady_clock::now() + timeout;
    try
    {
        client.dispatch_until([]() { return false; }, timeout);
    }
    catch (wlcs::Timeout const&)
    {
        EXPECT_THAT(std::chrono::steady_clock::now(), Gt(expected_end));
        EXPECT_THAT(std::chrono::steady_clock::now(), Lt(expected_end + 5s));
        return;
    }
    FAIL() << "Dispatch did not raise a wlcs::Timeout exception";
}
