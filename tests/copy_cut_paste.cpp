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
struct StartedInProcessServer : InProcessServer
{
    StartedInProcessServer() { InProcessServer::SetUp(); }

    void SetUp() override {}
};

auto static const any_width = 100;
auto static const any_height = 100;

struct ClientWithAVisibleSurface : Client
{
    using Client::Client;
    Surface surface = create_visible_surface(any_width, any_height);
};

struct CopyCutPaste : StartedInProcessServer
{
    ClientWithAVisibleSurface source{the_server()};
    ClientWithAVisibleSurface sink{the_server()};
};
}

TEST_F(CopyCutPaste, empty)
{
    // TODO
}