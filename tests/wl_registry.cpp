/*
 * Copyright © 2024 Canonical Ltd.
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
 */

#include "in_process_server.h"

#include <gmock/gmock.h>

#include <algorithm>
#include <string>
#include <vector>

using namespace testing;

namespace
{
struct AdvertisedGlobal
{
    uint32_t name;
    std::string interface;
    uint32_t version;
};

struct RegistryTest : wlcs::StartedInProcessServer
{
    wlcs::Client client{the_server()};

    std::vector<AdvertisedGlobal> enumerate_globals(wl_registry* registry)
    {
        globals.clear();

        static wl_registry_listener const listener{
            [](void* data, wl_registry*, uint32_t name, char const* interface, uint32_t version)
            {
                auto& globals = *static_cast<std::vector<AdvertisedGlobal>*>(data);
                globals.push_back({name, interface, version});
            },
            [](void*, wl_registry*, uint32_t) {}};

        wl_registry_add_listener(registry, &listener, &globals);
        client.roundtrip();

        return globals;
    }

    std::vector<AdvertisedGlobal> globals;
};

std::vector<std::string> interface_names(std::vector<AdvertisedGlobal> const& globals)
{
    std::vector<std::string> names;
    for (auto const& global : globals)
        names.push_back(global.interface);
    return names;
}
}

// wl_display.get_registry must return a registry that advertises the server's globals.
TEST_F(RegistryTest, get_registry_returns_globals)
{
    auto registry = wl_display_get_registry(client);

    auto const globals = enumerate_globals(registry);

    EXPECT_THAT(globals, Not(IsEmpty()));
    // Every conformant compositor must expose these core globals.
    EXPECT_THAT(interface_names(globals), Contains(Eq("wl_compositor")));
    EXPECT_THAT(interface_names(globals), Contains(Eq("wl_shm")));

    wl_registry_destroy(registry);
}

// wl_fixes.destroy_registry provides a way to destroy a registry server-side.
TEST_F(RegistryTest, registry_can_be_destroyed_with_wl_fixes)
{
    auto registry = wl_display_get_registry(client);
    auto const globals = enumerate_globals(registry);

    auto const fixes_global = std::find_if(
        globals.begin(), globals.end(),
        [](AdvertisedGlobal const& global) { return global.interface == "wl_fixes"; });

    if (fixes_global == globals.end())
    {
        wl_registry_destroy(registry);
        GTEST_SKIP() << "Compositor does not support wl_fixes";
    }

    auto fixes = static_cast<wl_fixes*>(
        wl_registry_bind(registry, fixes_global->name, &wl_fixes_interface, 1));
    ASSERT_THAT(fixes, NotNull());

    // Destroy a separate registry via wl_fixes; the client must not use it after.
    auto doomed_registry = wl_display_get_registry(client);
    client.roundtrip();
    auto const doomed_id = wl_proxy_get_id(reinterpret_cast<wl_proxy*>(doomed_registry));
    wl_fixes_destroy_registry(fixes, doomed_registry);
    client.roundtrip();

    // The compositor must emit wl_display.delete_id for the destroyed registry.
    // We cannot observe delete_id directly (libwayland handles it internally), but
    // a freed ID is re-used by the next object the client creates. If delete_id had
    // not been emitted, the ID would remain reserved and a fresh ID would be used.
    wl_registry_destroy(doomed_registry);
    auto reused_registry = wl_display_get_registry(client);
    EXPECT_THAT(wl_proxy_get_id(reinterpret_cast<wl_proxy*>(reused_registry)), Eq(doomed_id))
        << "Compositor did not emit wl_display.delete_id for the destroyed registry";
    wl_registry_destroy(reused_registry);

    wl_fixes_destroy(fixes);
    wl_registry_destroy(registry);
}
