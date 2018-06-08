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
#include "in_process_server.h"

#include <gmock/gmock.h>

#include <memory>

using namespace testing;
using namespace wlcs;

namespace
{
struct StartedInProcessServer : InProcessServer
{
    StartedInProcessServer() { InProcessServer::SetUp(); }

    void SetUp() override {}
};

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
    std::shared_ptr<ShmBuffer> const buffer1{std::make_shared<ShmBuffer>(client1, any_width, any_height)};
    wl_surface_attach(surface1, *buffer1, 0, 0);
    wl_surface_commit(surface1);

    client1.roundtrip();
}

TEST_F(SelfTest, given_second_client_when_first_creates_a_surface_nothing_bad_happens)
{
    Client client2{the_server()};

    Surface const surface1{client1.create_visible_surface(any_width, any_height)};
    std::shared_ptr<ShmBuffer> const buffer1{std::make_shared<ShmBuffer>(client1, any_width, any_height)};
    wl_surface_attach(surface1, *buffer1, 0, 0);
    wl_surface_commit(surface1);

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
    std::shared_ptr<ShmBuffer> const buffer1{std::make_shared<ShmBuffer>(client1, any_width, any_height)};
    wl_surface_attach(surface1, *buffer1, 0, 0);
    wl_surface_commit(surface1);

    Surface const surface2{client2.create_visible_surface(any_width, any_height)};
    std::shared_ptr<ShmBuffer> const buffer2{std::make_shared<ShmBuffer>(client2, any_width, any_height)};
    wl_surface_attach(surface2, *buffer2, 0, 0);
    wl_surface_commit(surface2);

    for (auto i = 0; i != 10; ++i)
    {
        client1.roundtrip();
        client2.roundtrip();
    }
}
