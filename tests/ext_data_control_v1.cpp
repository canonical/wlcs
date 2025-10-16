/*
 * Copyright © 2025 Canonical Ltd.
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
 * Authored by: Tarek Ismail <tarek.ismail@canonical.com>
 */

#include "copy_cut_paste.h"
#include "expect_protocol_error.h"
#include "generated/ext-data-control-v1-client.h"
#include "in_process_server.h"
#include "version_specifier.h"

#include <gmock/gmock.h>
#include <sys/socket.h>

using namespace testing;
using namespace wlcs;

namespace wlcs
{
WLCS_CREATE_INTERFACE_DESCRIPTOR(ext_data_control_manager_v1);
WLCS_CREATE_INTERFACE_DESCRIPTOR(ext_data_control_source_v1);
WLCS_CREATE_INTERFACE_DESCRIPTOR(ext_data_control_device_v1);
WLCS_CREATE_INTERFACE_DESCRIPTOR(ext_data_control_offer_v1);
}

namespace
{
static auto const test_message = "Hello from the other side";
static auto const test_mime_type = "text/plain";
static auto const test_source_fd = 42;

struct DataControlOfferWrapper
{
    WlHandle<ext_data_control_offer_v1> const offer;
    std::vector<std::string> mime_types;

    static void data_offer_offer(void* data, struct ext_data_control_offer_v1*, char const* mime_type)
    {
        auto* offer = reinterpret_cast<DataControlOfferWrapper*>(data);
        auto const mime_string = std::string{mime_type};
        offer->mime_types.push_back(mime_string);
    }

    static constexpr ext_data_control_offer_v1_listener offer_listener{.offer = data_offer_offer};

    DataControlOfferWrapper(ext_data_control_offer_v1* const offer) :
        offer{offer}
    {
        ext_data_control_offer_v1_add_listener(offer, &offer_listener, this);
    }
};

struct SinkClient : Client
{
    WlHandle<ext_data_control_manager_v1> sink_data_control_manager;
    WlHandle<ext_data_control_device_v1> sink_data_control_device;

    // Have to use a pointer since the offer wrapper uses `this` as the
    // userdata pointer, which with a normal object would be allocated on the
    // stack. Once the stack is cleaned up at the end of `data_offer`'s end,
    // the listener's pointer would be pointing at garbage.
    std::unique_ptr<DataControlOfferWrapper> current_offer;

    static void data_control_data_offer(
        void* data, struct ext_data_control_device_v1*, struct ext_data_control_offer_v1* id)
    {
        SinkClient* self = reinterpret_cast<SinkClient*>(data);

        if (id == nullptr)
            return;

        self->current_offer = std::make_unique<DataControlOfferWrapper>(id);
    }

    MOCK_METHOD(void, prepared_to_receive, ());

    static void data_control_selection(
        void* data, struct ext_data_control_device_v1*, struct ext_data_control_offer_v1* id)
    {
        SinkClient* self = reinterpret_cast<SinkClient*>(data);

        if (id == nullptr)
            return;

        EXPECT_THAT(self->current_offer, NotNull());
        EXPECT_THAT(self->current_offer->offer, Eq(id));
        EXPECT_THAT(self->current_offer->mime_types.size(), Gt(0));

        ext_data_control_offer_v1_receive(
            self->current_offer->offer, self->current_offer->mime_types[0].c_str(), test_source_fd);

        self->roundtrip(); // Make sure the server is notified of the receive request

        self->prepared_to_receive();
    }

    static constexpr ext_data_control_device_v1_listener sink_device_listener{
        .data_offer = data_control_data_offer,
        .selection = data_control_selection,
        .finished =
            [](auto...)
        {
        },
        .primary_selection =
            [](auto...)
        {
            FAIL() << "Unexpected request  to `primary_selection`";
        },
    };

    SinkClient(Server& the_server) :
        Client{the_server},
        sink_data_control_manager{bind_if_supported<ext_data_control_manager_v1>(AnyVersion)},
        sink_data_control_device{ext_data_control_manager_v1_get_data_device(sink_data_control_manager, seat())}

    {
        ext_data_control_device_v1_set_user_data(sink_data_control_device, this);
        // Set up the control device listener.
        // The next roundtrip should cause `selection` to be called with a null
        // id once the the control device is created on the server side.
        ext_data_control_device_v1_add_listener(sink_data_control_device, &sink_device_listener, this);

        // On the next roundtrip, the sink will receive notification of offer
        // on data control device -> Create offer and store it
        // Receive notification of offer selection/primary selection on control
        // device -> Send `send()` event with fd and mime type
    }

    operator ext_data_control_device_v1*()
    {
        return sink_data_control_device;
    }
};

struct SourceClient : Client
{
    WlHandle<ext_data_control_manager_v1> const source_client_data_control_manager;
    WlHandle<ext_data_control_device_v1> const source_client_data_control_device;
    WlHandle<ext_data_control_source_v1> const source_client_data_control_source;
    std::string const message;

    struct DataDeviceState
    {
        std::unique_ptr<DataControlOfferWrapper> current_offer;
    } data_device_state;

    std::unique_ptr<DataControlOfferWrapper> current_offer{nullptr};

    MOCK_METHOD(void, wrote_data, ());

    static void send(void* data, struct ext_data_control_source_v1*, char const* mime_type, int32_t)
    {
        SourceClient* self = reinterpret_cast<SourceClient*>(data);
        EXPECT_THAT(mime_type, StrEq(test_mime_type));
        self->wrote_data();
    }

    static constexpr ext_data_control_source_v1_listener source_client_source_listener{
        .send = send,
        .cancelled =
            [](auto...)
        {
            FAIL() << "Data control source received an unexpected `cancelled` event";
        },
    };

    SourceClient(Server& the_server, std::string message) :
        Client{the_server},
        source_client_data_control_manager{bind_if_supported<ext_data_control_manager_v1>(AnyVersion)},
        source_client_data_control_device{
            ext_data_control_manager_v1_get_data_device(source_client_data_control_manager, seat())},
        source_client_data_control_source{
            ext_data_control_manager_v1_create_data_source(source_client_data_control_manager)},
        message{message}
    {
        ext_data_control_source_v1_add_listener(
            source_client_data_control_source, &source_client_source_listener, this);

        // Offer plaintext
        ext_data_control_source_v1_offer(source_client_data_control_source, test_mime_type);

        // Set selection
        // For all current and future clients, this should notify them that
        // this source client is the active source when they create their
        // devices via the `selection()` event.
        ext_data_control_device_v1_set_selection(source_client_data_control_device, source_client_data_control_source);
    }
};

struct ExtDataControlV1Test : public wlcs::StartedInProcessServer
{
};
}

TEST_F(ExtDataControlV1Test, client_copies_into_clipboard_that_started_after_it)
{
    auto copying_client = SourceClient{the_server(), test_message};
    auto clipboard_client = SinkClient{the_server()};

    EXPECT_CALL(clipboard_client, prepared_to_receive());
    EXPECT_CALL(copying_client, wrote_data());

    // Notify the server that the copying client is the paste source
    copying_client.roundtrip();

    // To receive the offer and send the `send` request
    clipboard_client.roundtrip();

    // To receive the `send` event
    copying_client.roundtrip();
}

TEST_F(ExtDataControlV1Test, client_copies_into_clipboard_that_started_before_it)
{
    SinkClient clipboard_client{the_server()};
    SourceClient copying_client{the_server(), test_message};

    EXPECT_CALL(clipboard_client, prepared_to_receive());
    EXPECT_CALL(copying_client, wrote_data());

    // Set the the copying client as the current paste source
    copying_client.roundtrip();

    // Receive notification of the offer and send `send`
    clipboard_client.roundtrip();

    // Receive the send `event` and write the data
    copying_client.roundtrip();
}

TEST_F(ExtDataControlV1Test, client_pastes_from_clipboard_that_started_after_it)
{
    auto paste_client = SinkClient{the_server()};
    auto clipboard_client = SourceClient{the_server(), test_message};

    EXPECT_CALL(paste_client, prepared_to_receive());
    EXPECT_CALL(clipboard_client, wrote_data());

    paste_client.roundtrip();
    clipboard_client.roundtrip();
    paste_client.roundtrip();
    clipboard_client.roundtrip();
}

TEST_F(ExtDataControlV1Test, client_pastes_from_clipboard_that_started_before_it)
{
    // Create clipboard data control manager, device, and source
    auto clipboard = SourceClient{the_server(), test_message};
    auto paste_client = SinkClient{the_server()};

    EXPECT_CALL(paste_client, prepared_to_receive());
    EXPECT_CALL(clipboard, wrote_data());

    clipboard.roundtrip(); // Notify the server of the clipboard as a source;

    // Create paste client manager, and device
    //  The paste client device should be notified of the current offer from
    //  the clipboard, then the selection should be set, triggering a
    //  `receive()` request to the server, which the clipboard will receieve as
    //  a `send()` event.
    paste_client.roundtrip();

    // The clipboard should get the `send()` call
    // from the server and write into the fd
    clipboard.roundtrip();
}

TEST_F(ExtDataControlV1Test, setting_the_same_selection_twice_is_a_protocol_error)
{
    auto clipboard_client = SourceClient{the_server(), ""}; // We already set the selection in the ctor
    ext_data_control_device_v1_set_selection(
        clipboard_client.source_client_data_control_device, clipboard_client.source_client_data_control_source);

    EXPECT_PROTOCOL_ERROR(
        { clipboard_client.roundtrip(); },
        &ext_data_control_device_v1_interface,
        EXT_DATA_CONTROL_DEVICE_V1_ERROR_USED_SOURCE);
}

TEST_F(ExtDataControlV1Test, offering_mime_type_after_setting_selection_is_a_protocol_error)
{
    auto clipboard_client = SourceClient{the_server(), ""}; // We already set the selection in the ctor
    ext_data_control_source_v1_offer(clipboard_client.source_client_data_control_source, test_mime_type);

    EXPECT_PROTOCOL_ERROR(
        { clipboard_client.roundtrip(); },
        &ext_data_control_source_v1_interface,
        EXT_DATA_CONTROL_SOURCE_V1_ERROR_INVALID_OFFER);
}

TEST_F(ExtDataControlV1Test, copy_from_core_protocol_client_reaches_clipboard)
{
    CCnPSource source{the_server()};
    SinkClient clipboard{the_server()};

    EXPECT_CALL(clipboard, prepared_to_receive());
    EXPECT_CALL(source.data_source, wrote_data(_, _));

    source.offer(test_mime_type);

    clipboard.roundtrip();
    source.roundtrip();
}

TEST_F(ExtDataControlV1Test, paste_from_clipboard_reaches_core_protocol_client)
{
    SourceClient clipboard{the_server(), test_message};

    CCnPSink sink{the_server()};

    // Client should be able to paste once it gets `selection`
    EXPECT_CALL(sink.listener, selection(_, _));

    auto f = sink.create_surface_with_focus(); // To get focus
    sink.roundtrip();

    EXPECT_CALL(clipboard, wrote_data());

    MockDataOfferListener mdol;
    EXPECT_CALL(sink.listener, data_offer(_, _))
        .WillOnce(Invoke(
            [&](struct wl_data_device*, struct wl_data_offer* id)
            {
                mdol.listen_to(id);
            }));

    EXPECT_CALL(mdol, offer(_, _))
        .WillOnce(Invoke(
            [&](struct wl_data_offer* offer, char const* mime)
            {
                wl_data_offer_receive(offer, mime, test_source_fd);

                clipboard.roundtrip();
                sink.roundtrip();
            }));

    clipboard.roundtrip();
    sink.roundtrip();

    clipboard.roundtrip();
    sink.roundtrip();
}
