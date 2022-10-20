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

#include "primary_selection.h"

#include "in_process_server.h"
#include "version_specifier.h"

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

    WlHandle<zwp_primary_selection_device_manager_v1> const manager{
        this->bind_if_supported<zwp_primary_selection_device_manager_v1>(AnyVersion)};
    PrimarySelectionSource source{manager};
    PrimarySelectionDevice device{manager, seat()};

    void set_selection()
    {
        zwp_primary_selection_device_v1_set_selection(device, source, 0);
        roundtrip();
    }

    void offer(char const* mime_type)
    {
        zwp_primary_selection_source_v1_offer(source, mime_type);
        roundtrip();
    }
};

struct SinkApp : Client
{
    explicit SinkApp(Server& server) : Client{server} { roundtrip(); }

    WlHandle<zwp_primary_selection_device_manager_v1> const manager{
        this->bind_if_supported<zwp_primary_selection_device_manager_v1>(AnyVersion)};
    PrimarySelectionDevice device{manager, seat()};
};

struct PrimarySelection : StartedInProcessServer
{
    SourceApp   source_app{the_server()};
    SinkApp     sink_app{the_server()};
    Surface     surface{sink_app.create_visible_surface(10, 10)};

    void TearDown() override
    {
        source_app.roundtrip();
        sink_app.roundtrip();
        StartedInProcessServer::TearDown();
    }
};

struct MockPrimarySelectionDeviceListener : PrimarySelectionDeviceListener
{
    using PrimarySelectionDeviceListener::PrimarySelectionDeviceListener;

    MOCK_METHOD2(data_offer, void(zwp_primary_selection_device_v1* device, zwp_primary_selection_offer_v1* offer));
    MOCK_METHOD2(selection, void(zwp_primary_selection_device_v1* device, zwp_primary_selection_offer_v1* offer));
};

struct MockPrimarySelectionOfferListener : PrimarySelectionOfferListener
{
    using PrimarySelectionOfferListener::PrimarySelectionOfferListener;

    MOCK_METHOD2(offer, void(zwp_primary_selection_offer_v1* offer, const char* mime_type));
};

struct MockPrimarySelectionSourceListener : PrimarySelectionSourceListener
{
    using PrimarySelectionSourceListener::PrimarySelectionSourceListener;

    MOCK_METHOD3(send, void(zwp_primary_selection_source_v1* source, const char* mime_type, int32_t fd));

    MOCK_METHOD1(cancelled, void(zwp_primary_selection_source_v1*));
};

struct StubPrimarySelectionDeviceListener : PrimarySelectionDeviceListener
{
    StubPrimarySelectionDeviceListener(
        zwp_primary_selection_device_v1* device,
        PrimarySelectionOfferListener& offer_listener) :
        PrimarySelectionDeviceListener{device},
        offer_listener{offer_listener}
    {
    }

    void data_offer(zwp_primary_selection_device_v1* device, zwp_primary_selection_offer_v1* offer) override
    {
        offer_listener.listen_to(offer);
        PrimarySelectionDeviceListener::data_offer(device, offer);
    }

    void selection(zwp_primary_selection_device_v1* device, zwp_primary_selection_offer_v1* offer) override
    {
        selected = offer;
        PrimarySelectionDeviceListener::selection(device, offer);
    }

    PrimarySelectionOfferListener& offer_listener;
    zwp_primary_selection_offer_v1* selected = nullptr;
};

struct StubPrimarySelectionSourceListener : PrimarySelectionSourceListener
{
    using PrimarySelectionSourceListener::PrimarySelectionSourceListener;

    void send(zwp_primary_selection_source_v1*, const char*, int32_t fd)
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

TEST_F(PrimarySelection, source_can_offer)
{
    source_app.offer(any_mime_type);
    source_app.set_selection();
}

TEST_F(PrimarySelection, sink_can_listen)
{
    MockPrimarySelectionDeviceListener  device_listener{sink_app.device};
    MockPrimarySelectionOfferListener   offer_listener;

    InSequence seq;
    EXPECT_CALL(device_listener, data_offer(_, _))
        .WillOnce(Invoke([&](auto*, auto* id) { offer_listener.listen_to(id); }));

    EXPECT_CALL(offer_listener, offer(_, StrEq(any_mime_type)));

    EXPECT_CALL(device_listener, selection(_, _));

    source_app.offer(any_mime_type);
    source_app.set_selection();

    sink_app.roundtrip();
}

TEST_F(PrimarySelection, sink_can_request)
{
    PrimarySelectionOfferListener   offer_listener;
    StubPrimarySelectionDeviceListener  device_listener{sink_app.device, offer_listener};
    source_app.offer(any_mime_type);
    source_app.set_selection();
    sink_app.roundtrip();
    ASSERT_THAT(device_listener.selected, NotNull());

    Pipe pipe;
    zwp_primary_selection_offer_v1_receive(device_listener.selected, any_mime_type, pipe.source);
    sink_app.roundtrip();
}

TEST_F(PrimarySelection, source_sees_request)
{
    MockPrimarySelectionSourceListener  source_listener{source_app.source};
    PrimarySelectionOfferListener   offer_listener;
    StubPrimarySelectionDeviceListener  device_listener{sink_app.device, offer_listener};
    source_app.offer(any_mime_type);
    source_app.set_selection();
    sink_app.roundtrip();
    ASSERT_THAT(device_listener.selected, NotNull());

    EXPECT_CALL(source_listener, send(_, _, _))
        .Times(1)
        .WillRepeatedly(Invoke([&](auto*, auto*, int fd) { close(fd); }));

    Pipe pipe;
    zwp_primary_selection_offer_v1_receive(device_listener.selected, any_mime_type, pipe.source);
    sink_app.roundtrip();
    source_app.roundtrip();
}

TEST_F(PrimarySelection, source_can_supply_request)
{
    StubPrimarySelectionSourceListener source_listener{source_app.source};
    PrimarySelectionOfferListener   offer_listener;
    StubPrimarySelectionDeviceListener  device_listener{sink_app.device, offer_listener};
    source_app.offer(any_mime_type);
    source_app.set_selection();
    sink_app.roundtrip();
    ASSERT_THAT(device_listener.selected, NotNull());

    Pipe pipe;
    zwp_primary_selection_offer_v1_receive(device_listener.selected, any_mime_type, pipe.source);
    sink_app.roundtrip();
    source_app.roundtrip();

    char buffer[128];
    EXPECT_THAT(read(pipe.sink, buffer, sizeof buffer), Eq(ssize_t(sizeof any_mime_data)));
    EXPECT_THAT(buffer, StrEq(any_mime_data));
}
