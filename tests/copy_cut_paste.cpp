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
#include "wl_handle.h"
#include "version_specifier.h"

#include <gmock/gmock.h>

#include <memory>

using namespace testing;
using namespace wlcs;

namespace
{

auto static const any_width = 100;
auto static const any_height = 100;
auto static const any_mime_type = "AnyMimeType";

struct CCnPSource : Client
{
    using Client::Client;

    Surface const surface{create_visible_surface(any_width, any_height)};
    WlHandle<wl_data_device_manager> const manager{
        this->bind_if_supported<wl_data_device_manager>(AnyVersion)};
    DataDevice data_device{wl_data_device_manager_get_data_device(manager,seat())};
    DataSource data{wl_data_device_manager_create_data_source(manager)};

    void offer(char const* mime_type)
    {
        wl_data_source_offer(data, mime_type);
        // TODO: collect a serial from the "event that triggered" the selection
        // (This works while Mir fails to validate the serial)
        uint32_t const serial = 0;
        wl_data_device_set_selection(data_device, data, serial);
        roundtrip();
    }
};

struct MockDataDeviceListener : DataDeviceListener
{
    using DataDeviceListener::DataDeviceListener;

    MOCK_METHOD2(data_offer, void(struct wl_data_device* wl_data_device, struct wl_data_offer* id));
};

struct MockDataOfferListener : DataOfferListener
{
    using DataOfferListener::DataOfferListener;

    MOCK_METHOD2(offer, void(struct wl_data_offer* data_offer, char const* MimeType));
};

struct CCnPSink : Client
{
    using Client::Client;

    WlHandle<wl_data_device_manager> const manager{
        this->bind_if_supported<wl_data_device_manager>(AnyVersion)};
    DataDevice sink_data{wl_data_device_manager_get_data_device(manager, seat())};
    MockDataDeviceListener listener{sink_data};

    Surface create_surface_with_focus()
    {
        return Surface{create_visible_surface(any_width, any_height)};
    }
};

struct CopyCutPaste : StartedInProcessServer
{
    CCnPSource source{the_server()};
    CCnPSink sink{the_server()};

    MockDataOfferListener mdol;

    void TearDown() override
    {
        source.roundtrip();
        sink.roundtrip();
        StartedInProcessServer::TearDown();
    }
};
}

TEST_F(CopyCutPaste, given_source_has_offered_when_sink_gets_focus_it_sees_offer)
{
    source.offer(any_mime_type);

    EXPECT_CALL(mdol, offer(_, StrEq(any_mime_type)));
    EXPECT_CALL(sink.listener, data_offer(_,_))
        .WillOnce(Invoke([&](struct wl_data_device*, struct wl_data_offer* id)
        { mdol.listen_to(id); }));

    sink.create_surface_with_focus();
}

TEST_F(CopyCutPaste, given_sink_has_focus_when_source_makes_offer_sink_sees_offer)
{
    auto sink_surface_with_focus = sink.create_surface_with_focus();

    EXPECT_CALL(mdol, offer(_, StrEq(any_mime_type)));
    EXPECT_CALL(sink.listener, data_offer(_,_))
        .WillOnce(Invoke([&](struct wl_data_device*, struct wl_data_offer* id)
        { mdol.listen_to(id); }));

    source.offer(any_mime_type);
}
