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
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#include "gtk_primary_selection.h"

#include "in_process_server.h"

#include <gmock/gmock.h>
#include <sys/socket.h>

using namespace wlcs;
using namespace testing;

namespace
{
char const any_mime_type[] = "AnyMimeType";
char const any_mime_data[] = "AnyMimeData";

struct SourceApp : Client
{
    // Can't use "using Client::Client;" because Xenial
    explicit SourceApp(Server& server) : Client{server} {}

    WlProxy<gtk_primary_selection_device_manager> manager{*this};
    GtkPrimarySelectionSource source{manager};
    GtkPrimarySelectionDevice device{manager, seat()};

    void set_selection()
    {
        gtk_primary_selection_device_set_selection(device, source, 0);
        roundtrip();
    }

    void offer(char const* mime_type)
    {
        gtk_primary_selection_source_offer(source, mime_type);
        roundtrip();
    }
};

struct SinkApp : Client
{
    explicit SinkApp(Server& server) : Client{server} { roundtrip(); }

    WlProxy<gtk_primary_selection_device_manager> const manager{*this};
    GtkPrimarySelectionDevice device{manager, seat()};
};

struct GtkPrimarySelection : StartedInProcessServer
{
    CheckInterfaceExpected must_be_first{the_server(), gtk_primary_selection_device_manager_interface};
    SourceApp   source_app{the_server()};
    SinkApp     sink_app{the_server()};

    void TearDown() override
    {
        source_app.roundtrip();
        sink_app.roundtrip();
        StartedInProcessServer::TearDown();
    }
};

struct MockGtkPrimarySelectionDeviceListener : GtkPrimarySelectionDeviceListener
{
    using GtkPrimarySelectionDeviceListener::GtkPrimarySelectionDeviceListener;

    MOCK_METHOD2(data_offer, void(gtk_primary_selection_device* device, gtk_primary_selection_offer* offer));
    MOCK_METHOD2(selection, void(gtk_primary_selection_device* device, gtk_primary_selection_offer* offer));
};

struct MockGtkPrimarySelectionOfferListener : GtkPrimarySelectionOfferListener
{
    using GtkPrimarySelectionOfferListener::GtkPrimarySelectionOfferListener;

    MOCK_METHOD2(offer, void(gtk_primary_selection_offer* offer, const char* mime_type));
};

struct MockGtkPrimarySelectionSourceListener : GtkPrimarySelectionSourceListener
{
    using GtkPrimarySelectionSourceListener::GtkPrimarySelectionSourceListener;

    MOCK_METHOD3(send, void(gtk_primary_selection_source* source, const char* mime_type, int32_t fd));

    MOCK_METHOD1(cancelled, void(gtk_primary_selection_source*));
};

struct StubGtkPrimarySelectionDeviceListener : GtkPrimarySelectionDeviceListener
{
    StubGtkPrimarySelectionDeviceListener(
        gtk_primary_selection_device* device,
        GtkPrimarySelectionOfferListener& offer_listener) :
        GtkPrimarySelectionDeviceListener{device},
        offer_listener{offer_listener}
    {
    }

    void data_offer(gtk_primary_selection_device* device, gtk_primary_selection_offer* offer) override
    {
        offer_listener.listen_to(offer);
        GtkPrimarySelectionDeviceListener::data_offer(device, offer);
    }

    void selection(gtk_primary_selection_device* device, gtk_primary_selection_offer* offer) override
    {
        selected = offer;
        GtkPrimarySelectionDeviceListener::selection(device, offer);
    }

    GtkPrimarySelectionOfferListener& offer_listener;
    gtk_primary_selection_offer* selected = nullptr;
};

struct StubGtkPrimarySelectionSourceListener : GtkPrimarySelectionSourceListener
{
    using GtkPrimarySelectionSourceListener::GtkPrimarySelectionSourceListener;

    void send(gtk_primary_selection_source*, const char*, int32_t fd)
    {
        ASSERT_THAT(write(fd, any_mime_data, sizeof any_mime_data), Eq(ssize_t(sizeof any_mime_data)));
        close(fd);
    }
};

struct Pipe
{
    int source;
    int sink;

    Pipe() { socketpair(AF_LOCAL, SOCK_STREAM, 0, &source); }
    Pipe(Pipe const&) = delete;
    Pipe& operator=(Pipe const&) = delete;

    ~Pipe() { close(source); close(sink); }
};
}

TEST_F(GtkPrimarySelection, source_can_offer)
{
    source_app.offer(any_mime_type);
    source_app.set_selection();
}

TEST_F(GtkPrimarySelection, sink_can_listen)
{
    MockGtkPrimarySelectionDeviceListener  device_listener{sink_app.device};
    MockGtkPrimarySelectionOfferListener   offer_listener;

    InSequence seq;
    EXPECT_CALL(device_listener, data_offer(_, _))
        .WillOnce(Invoke([&](auto*, auto* id) { offer_listener.listen_to(id); }));

    EXPECT_CALL(offer_listener, offer(_, StrEq(any_mime_type)));

    EXPECT_CALL(device_listener, selection(_, _));

    source_app.offer(any_mime_type);
    source_app.set_selection();

    sink_app.roundtrip();
}

TEST_F(GtkPrimarySelection, sink_can_request)
{
    GtkPrimarySelectionOfferListener   offer_listener;
    StubGtkPrimarySelectionDeviceListener  device_listener{sink_app.device, offer_listener};
    source_app.offer(any_mime_type);
    source_app.set_selection();
    sink_app.roundtrip();
    ASSERT_THAT(device_listener.selected, NotNull());

    Pipe pipe;
    gtk_primary_selection_offer_receive(device_listener.selected, any_mime_type, pipe.source);
    sink_app.roundtrip();
}

TEST_F(GtkPrimarySelection, source_sees_request)
{
    MockGtkPrimarySelectionSourceListener  source_listener{source_app.source};
    GtkPrimarySelectionOfferListener   offer_listener;
    StubGtkPrimarySelectionDeviceListener  device_listener{sink_app.device, offer_listener};
    source_app.offer(any_mime_type);
    source_app.set_selection();
    sink_app.roundtrip();
    ASSERT_THAT(device_listener.selected, NotNull());

    EXPECT_CALL(source_listener, send(_, _, _))
        .Times(1)
        .WillRepeatedly(Invoke([&](auto*, auto*, int fd) { close(fd); }));

    Pipe pipe;
    gtk_primary_selection_offer_receive(device_listener.selected, any_mime_type, pipe.source);
    sink_app.roundtrip();
    source_app.roundtrip();
}

TEST_F(GtkPrimarySelection, source_can_supply_request)
{
    StubGtkPrimarySelectionSourceListener source_listener{source_app.source};
    GtkPrimarySelectionOfferListener   offer_listener;
    StubGtkPrimarySelectionDeviceListener  device_listener{sink_app.device, offer_listener};
    source_app.offer(any_mime_type);
    source_app.set_selection();
    sink_app.roundtrip();
    ASSERT_THAT(device_listener.selected, NotNull());

    Pipe pipe;
    gtk_primary_selection_offer_receive(device_listener.selected, any_mime_type, pipe.source);
    sink_app.roundtrip();
    source_app.roundtrip();

    char buffer[128];
    EXPECT_THAT(read(pipe.sink, buffer, sizeof buffer), Eq(ssize_t(sizeof any_mime_data)));
    EXPECT_THAT(buffer, StrEq(any_mime_data));
}