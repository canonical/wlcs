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
    using WrappedType = struct zwp_primary_selection_source_v1;
    
    PrimarySelectionSource() = default;

    explicit PrimarySelectionSource(WrappedType* ds) : self{ds, deleter} {}

    operator WrappedType*() const { return self.get(); }

    void reset() { self.reset(); }

    void reset(WrappedType* ds) { self.reset(ds, deleter); }

    friend void zwp_primary_selection_source_v1_destroy(PrimarySelectionSource const&) = delete;

private:
    static void deleter(WrappedType* ds) { zwp_primary_selection_source_v1_destroy(ds); }

    std::shared_ptr<WrappedType> self;
};


class PrimarySelectionDevice
{
public:
    using WrappedType = struct zwp_primary_selection_device_v1;

    PrimarySelectionDevice() = default;

    explicit PrimarySelectionDevice(WrappedType* ds) : self{ds, deleter} {}

    operator WrappedType*() const { return self.get(); }

    void reset() { self.reset(); }

    void reset(WrappedType* ds) { self.reset(ds, deleter); }

    friend void zwp_primary_selection_device_v1_destroy(PrimarySelectionDevice const&) = delete;

private:
    static void deleter(WrappedType* ds) { zwp_primary_selection_device_v1_destroy(ds); }

    std::shared_ptr<WrappedType> self;
};

struct PrimarySelectionDeviceListener
{
    PrimarySelectionDeviceListener(struct zwp_primary_selection_device_v1* data_device)
    {
        active_listeners.add(this);
        zwp_primary_selection_device_v1_add_listener(data_device, &thunks, this);
    }

    virtual ~PrimarySelectionDeviceListener() { active_listeners.del(this); }

    PrimarySelectionDeviceListener(PrimarySelectionDeviceListener const&) = delete;
    PrimarySelectionDeviceListener& operator=(PrimarySelectionDeviceListener const&) = delete;

    virtual void data_offer(
        struct zwp_primary_selection_device_v1* zwp_primary_selection_device_v1,
        struct zwp_primary_selection_offer_v1* id);

    virtual void selection(
        struct zwp_primary_selection_device_v1* zwp_primary_selection_device_v1,
        struct zwp_primary_selection_offer_v1* id);

private:
    static void data_offer(
        void* data,
        struct zwp_primary_selection_device_v1* zwp_primary_selection_device_v1,
        struct zwp_primary_selection_offer_v1* id);

    static void selection(
        void* data,
        struct zwp_primary_selection_device_v1* zwp_primary_selection_device_v1,
        struct zwp_primary_selection_offer_v1* id);

    static ActiveListeners active_listeners;
    constexpr static zwp_primary_selection_device_v1_listener thunks =
        {
            &data_offer,
            &selection
        };
};

}

wlcs::ActiveListeners wlcs::PrimarySelectionDeviceListener::active_listeners;
zwp_primary_selection_device_v1_listener const wlcs::PrimarySelectionDeviceListener::thunks;

void wlcs::PrimarySelectionDeviceListener::data_offer(
    void* data,
    struct zwp_primary_selection_device_v1* offer,
    struct zwp_primary_selection_offer_v1* id)
{
    if (active_listeners.includes(data))
        static_cast<PrimarySelectionDeviceListener*>(data)->data_offer(offer, id);
}

void wlcs::PrimarySelectionDeviceListener::data_offer(
    struct zwp_primary_selection_device_v1*, struct zwp_primary_selection_offer_v1*)
{
}

void wlcs::PrimarySelectionDeviceListener::selection(
    struct zwp_primary_selection_device_v1*, struct zwp_primary_selection_offer_v1*)
{
}

void wlcs::PrimarySelectionDeviceListener::selection(
    void* data, struct zwp_primary_selection_device_v1* zwp_primary_selection_device_v1,
    struct zwp_primary_selection_offer_v1* id)
{
    if (active_listeners.includes(data))
        static_cast<PrimarySelectionDeviceListener*>(data)->selection(zwp_primary_selection_device_v1, id);
}

#include "in_process_server.h"

#include <gmock/gmock.h>

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
    PrimarySelectionSource source{zwp_primary_selection_device_manager_v1_create_source(primary_selection_device_manager())};
    PrimarySelectionDevice device{zwp_primary_selection_device_manager_v1_get_device(primary_selection_device_manager(), seat())};

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

    PrimarySelectionDevice sink_data{zwp_primary_selection_device_manager_v1_get_device(primary_selection_device_manager(), seat())};

    Surface create_surface_with_focus()
    {
        return Surface{create_visible_surface(any_width, any_height)};
    }
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

    MOCK_METHOD2(data_offer, void(struct zwp_primary_selection_device_v1* zwp_primary_selection_device_v1,
        struct zwp_primary_selection_offer_v1* id));

    MOCK_METHOD2(selection, void(struct zwp_primary_selection_device_v1* zwp_primary_selection_device_v1,
        struct zwp_primary_selection_offer_v1* id));
};

}

TEST_F(PrimarySelection, source_can_offer)
{
    source_app.set_selection();
    source_app.offer(any_mime_type);
}


TEST_F(PrimarySelection, sink_can_listen)
{
    MockPrimarySelectionDeviceListener listener{sink_app.sink_data};
    InSequence seq;
    EXPECT_CALL(listener, data_offer(_, _));
    EXPECT_CALL(listener, selection(_, _));

    source_app.set_selection();
    source_app.offer(any_mime_type);

    sink_app.roundtrip();
}
