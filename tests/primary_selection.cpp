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

#include "generated/primary-selection-unstable-v1-client.h"

#include "active_listeners.h"

#include <memory>
#include <mutex>
#include <set>

namespace wlcs
{
class PrimarySelectionSource
{
public:
    using WrappedType = zwp_primary_selection_source_v1;
    
    PrimarySelectionSource() = default;

    explicit PrimarySelectionSource(zwp_primary_selection_device_manager_v1* manager) :
        self{zwp_primary_selection_device_manager_v1_create_source(manager), deleter} {}

    operator WrappedType*() const { return self.get(); }

    void reset() { self.reset(); }

    void reset(WrappedType* source) { self.reset(source, deleter); }

    friend void zwp_primary_selection_source_v1_destroy(PrimarySelectionSource const&) = delete;

private:
    static void deleter(WrappedType* source) { zwp_primary_selection_source_v1_destroy(source); }

    std::shared_ptr<WrappedType> self;
};


class PrimarySelectionDevice
{
public:
    using WrappedType = zwp_primary_selection_device_v1;

    PrimarySelectionDevice() = default;

    PrimarySelectionDevice(zwp_primary_selection_device_manager_v1* manager, wl_seat* seat) :
        self{zwp_primary_selection_device_manager_v1_get_device(manager, seat), deleter} {}

    operator WrappedType*() const { return self.get(); }

    void reset() { self.reset(); }

    void reset(WrappedType* device) { self.reset(device, deleter); }

    friend void zwp_primary_selection_device_v1_destroy(PrimarySelectionDevice const&) = delete;

private:
    static void deleter(WrappedType* device) { zwp_primary_selection_device_v1_destroy(device); }

    std::shared_ptr<WrappedType> self;
};

struct PrimarySelectionDeviceListener
{
    PrimarySelectionDeviceListener(zwp_primary_selection_device_v1* device)
    {
        active_listeners.add(this);
        zwp_primary_selection_device_v1_add_listener(device, &thunks, this);
    }

    virtual ~PrimarySelectionDeviceListener() { active_listeners.del(this); }

    PrimarySelectionDeviceListener(PrimarySelectionDeviceListener const&) = delete;
    PrimarySelectionDeviceListener& operator=(PrimarySelectionDeviceListener const&) = delete;

    virtual void data_offer(zwp_primary_selection_device_v1* device, zwp_primary_selection_offer_v1* offer);

    virtual void selection(zwp_primary_selection_device_v1* device, zwp_primary_selection_offer_v1* offer);

private:
    static void data_offer(void* data, zwp_primary_selection_device_v1* device, zwp_primary_selection_offer_v1* offer);

    static void selection(void* data, zwp_primary_selection_device_v1* device, zwp_primary_selection_offer_v1* offer);

    static ActiveListeners active_listeners;
    constexpr static zwp_primary_selection_device_v1_listener thunks =
        {
            &data_offer,
            &selection
        };
};

struct PrimarySelectionOfferListener
{
    PrimarySelectionOfferListener() { active_listeners.add(this); }
    virtual ~PrimarySelectionOfferListener() { active_listeners.del(this); }

    PrimarySelectionOfferListener(PrimarySelectionOfferListener const&) = delete;
    PrimarySelectionOfferListener& operator=(PrimarySelectionOfferListener const&) = delete;

    void listen_to(zwp_primary_selection_offer_v1* offer)
    {
        zwp_primary_selection_offer_v1_add_listener(offer, &thunks, this);
    }

    virtual void offer(zwp_primary_selection_offer_v1* offer, const char* mime_type);

private:
    static void offer(void* data, zwp_primary_selection_offer_v1* offer, const char* mime_type);

    static ActiveListeners active_listeners;
    constexpr static zwp_primary_selection_offer_v1_listener thunks = { &offer, };
};

wlcs::ActiveListeners wlcs::PrimarySelectionDeviceListener::active_listeners;
constexpr zwp_primary_selection_device_v1_listener wlcs::PrimarySelectionDeviceListener::thunks;

wlcs::ActiveListeners wlcs::PrimarySelectionOfferListener::active_listeners;
constexpr zwp_primary_selection_offer_v1_listener wlcs::PrimarySelectionOfferListener::thunks;


void wlcs::PrimarySelectionDeviceListener::data_offer(
    void* data,
    zwp_primary_selection_device_v1* device,
    zwp_primary_selection_offer_v1* offer)
{
    if (active_listeners.includes(data))
        static_cast<PrimarySelectionDeviceListener*>(data)->data_offer(device, offer);
}

void wlcs::PrimarySelectionDeviceListener::data_offer(zwp_primary_selection_device_v1*, zwp_primary_selection_offer_v1*)
{
}

void wlcs::PrimarySelectionDeviceListener::selection(zwp_primary_selection_device_v1*, zwp_primary_selection_offer_v1*)
{
}

void wlcs::PrimarySelectionDeviceListener::selection(
    void* data,
    zwp_primary_selection_device_v1* device,
    zwp_primary_selection_offer_v1* offer)
{
    if (active_listeners.includes(data))
        static_cast<PrimarySelectionDeviceListener*>(data)->selection(device, offer);
}

void wlcs::PrimarySelectionOfferListener::offer(zwp_primary_selection_offer_v1*, const char*)
{
}

void wlcs::PrimarySelectionOfferListener::offer(
    void* data, zwp_primary_selection_offer_v1* offer, const char* mime_type)
{
    if (active_listeners.includes(data))
        static_cast<PrimarySelectionOfferListener*>(data)->offer(offer, mime_type);
}
}


#include "in_process_server.h"

#include <gmock/gmock.h>
#include <sys/socket.h>

using namespace wlcs;
using namespace testing;

namespace
{
auto const any_width = 100;
auto const any_height = 100;
auto const any_mime_type = "AnyMimeType";

struct SourceApp : Client
{
    // Can't use "using Client::Client;" because Xenial
    explicit SourceApp(Server& server) : Client{server} {}

    Surface const surface{create_visible_surface(any_width, any_height)};
    PrimarySelectionSource source{primary_selection_device_manager()};
    PrimarySelectionDevice device{primary_selection_device_manager(), seat()};

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
    // Can't use "using Client::Client;" because Xenial
    explicit SinkApp(Server& server) : Client{server} {}

    PrimarySelectionDevice device{primary_selection_device_manager(), seat()};
};

struct PrimarySelection : StartedInProcessServer
{
    SourceApp   source_app{the_server()};
    SinkApp     sink_app{the_server()};

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

    enum { source, sink, fd_count };
    int data_fd[fd_count];

    socketpair(AF_LOCAL, SOCK_STREAM, 0, data_fd);

    zwp_primary_selection_offer_v1_receive(device_listener.selected, any_mime_type, data_fd[source]);

    for (auto fd : data_fd) close(fd);
}
