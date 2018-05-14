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
#include "wayland-client-protocol.h"

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

auto static const any_width = 100;
auto static const any_height = 100;
auto static const any_mime_type = "AnyMimeType";

struct CCnPClient : Client
{
    CCnPClient(Server& server)
        : Client(server),
          surface{*this},
          shell_surface{wl_shell_get_shell_surface(shell(), surface)}
    {
        wl_shell_surface_set_toplevel(shell_surface);
        wl_surface_commit(surface);

        wl_surface_attach(surface, *buffer, 0, 0);
        wl_surface_commit(surface);
    }

    ~CCnPClient()
    {
        wl_shell_surface_destroy(shell_surface);
    }

    Surface const surface;
    wl_shell_surface* const shell_surface;
    std::shared_ptr<ShmBuffer> const buffer{std::make_shared<ShmBuffer>(*this, any_width, any_height)};
};

struct CopyCutPaste : StartedInProcessServer
{
    CCnPClient source{the_server()};
};

struct MockDataDeviceListener : DataDeviceListener
{
    using DataDeviceListener::DataDeviceListener;

    MOCK_METHOD2(data_offer, void(struct wl_data_device* wl_data_device, struct wl_data_offer* id));
};
}

TEST_F(CopyCutPaste, given_source_has_offered_data_sink_sees_offer)
{
    DataSource source_data{wl_data_device_manager_create_data_source(source.data_device_manager())};
    wl_data_source_offer(source_data, any_mime_type);
    source.roundtrip();

    CCnPClient sink{the_server()};
    DataDevice sink_data{wl_data_device_manager_get_data_device(sink.data_device_manager(), sink.seat())};
    MockDataDeviceListener listener{sink_data};
    EXPECT_CALL(listener, data_offer(_,_));

    sink.roundtrip();
    sink.roundtrip();
}
