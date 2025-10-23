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
#include "generated/primary-selection-unstable-v1-client.h"
#include "in_process_server.h"
#include "primary_selection.h"
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
static auto const bogus_source_fd = 42;

class Pipe
{
    std::array<int, 2> const pipe_ends;

public:
    Pipe() :
        pipe_ends{[&]
                  {
                      int pipe[2];
                      auto const ret = socketpair(AF_LOCAL, SOCK_STREAM, 0, pipe);

                      if (ret != 0)
                      {
                          std::cerr << "Failed to create socket, errno=" << errno
                                    << " error message=" << std::strerror(errno) << '\n';
                          std::abort();
                      }

                      return std::array{pipe[0], pipe[1]};
                  }()}
    {

    }

    ~Pipe()
    {
        close(source);
        close(sink);
    }

    int const &source = pipe_ends[0], sink = pipe_ends[1];
};

struct DataControlOfferWrapper
{
    WlHandle<ext_data_control_offer_v1> const offer;
    std::vector<std::string> mime_types;

    static void data_offer_offer(void* data, struct ext_data_control_offer_v1*, char const* mime_type)
    {
        auto* offer = static_cast<DataControlOfferWrapper*>(data);
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

enum SelectionType
{
    normal,
    primary
};

struct ExtDataControlClient: public Client
{
    WlHandle<ext_data_control_manager_v1> const data_control_manager;
    WlHandle<ext_data_control_device_v1> const data_control_device;
    WlHandle<ext_data_control_source_v1> const source_data_control_source;

    // Have to use a pointer since the offer wrapper uses `this` as the
    // userdata pointer, which with a normal object would be allocated on the
    // stack. Once the stack is cleaned up at the end of `data_offer`'s end,
    // the listener's pointer would be pointing at garbage.
    std::unique_ptr<DataControlOfferWrapper> current_offer;

    std::optional<Pipe> receiving_pipe;
    std::optional<std::string> received_message;

    MOCK_METHOD(void, prepared_to_receive, ());
    MOCK_METHOD(void, wrote_data, ());

    static void data_control_data_offer(
        void* data, struct ext_data_control_device_v1*, struct ext_data_control_offer_v1* id)
    {
        auto* self = static_cast<ExtDataControlClient*>(data);

        if (id == nullptr)
            return;

        self->current_offer = std::make_unique<DataControlOfferWrapper>(id);
    }


    static void data_control_selection(
        void* data, struct ext_data_control_device_v1*, struct ext_data_control_offer_v1* id)
    {
        auto* self = static_cast<ExtDataControlClient*>(data);

        if (id == nullptr)
            return;

        EXPECT_THAT(self->current_offer, NotNull());
        EXPECT_THAT(self->current_offer->offer, Eq(id));
        EXPECT_THAT(self->current_offer->mime_types.size(), Gt(0));

        ext_data_control_offer_v1_receive(
            self->current_offer->offer, self->current_offer->mime_types[0].c_str(), self->receiving_pipe->source);

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
        .primary_selection = data_control_selection,
    };

    static void send(void* data, struct ext_data_control_source_v1*, char const* mime_type, int32_t fd)
    {
        auto* self = static_cast<ExtDataControlClient*>(data);
        EXPECT_THAT(mime_type, StrEq(test_mime_type));

        auto const message = self->received_message.value_or(std::string(test_message));

        write(fd, message.c_str(), message.size());

        self->wrote_data();
    }

    static constexpr ext_data_control_source_v1_listener source_source_listener{
        .send = send,
        .cancelled =
            [](auto...)
        {
            FAIL() << "Data control source received an unexpected `cancelled` event";
        },
    };

    ExtDataControlClient(Server& the_server) :
        Client{the_server},
        data_control_manager{bind_if_supported<ext_data_control_manager_v1>(AnyVersion)},
        data_control_device{ext_data_control_manager_v1_get_data_device(data_control_manager, seat())},
        source_data_control_source{ext_data_control_manager_v1_create_data_source(data_control_manager)}
    {
    }

    void as_sink()
    {
        receiving_pipe.emplace();

        ext_data_control_device_v1_set_user_data(data_control_device, this);
        // Set up the control device listener.
        // The next roundtrip should cause `selection` to be called with a null
        // id once the the control device is created on the server side.
        ext_data_control_device_v1_add_listener(data_control_device, &sink_device_listener, this);

        // On the next roundtrip, the sink will receive notification of offer
        // on data control device -> Create offer and store it
        // Receive notification of offer selection/primary selection on control
        // device -> Send `send()` event with fd and mime type
    }

    auto try_read() -> std::string
    {
        char buf[128];
        auto const read_chars = read(receiving_pipe->sink, buf, sizeof(buf));
        received_message = std::string{buf, static_cast<size_t>(read_chars)};
        return *received_message;
    }

    void as_source(SelectionType selection = SelectionType::normal, std::optional<std::string> message = {})
    {
        if(message)
            received_message = message;

        ext_data_control_source_v1_add_listener(
            source_data_control_source, &source_source_listener, this);

        // Offer plaintext
        ext_data_control_source_v1_offer(source_data_control_source, test_mime_type);

        switch (selection)
        {
        case SelectionType::normal:

            // Set selection
            // For all current and future clients, this should notify them that
            // this source client is the active source when they create their
            // devices via the `selection()` event.
            ext_data_control_device_v1_set_selection(data_control_device, source_data_control_source);
            break;
        case SelectionType::primary:
            ext_data_control_device_v1_set_primary_selection(data_control_device, source_data_control_source);
            break;
        }
    }
};

struct ExtDataControlV1Test : public wlcs::StartedInProcessServer
{
};
}

TEST_F(ExtDataControlV1Test, client_copies_into_clipboard_that_started_after_it)
{
    auto copying_client = ExtDataControlClient{the_server()};
    copying_client.as_source();

    auto clipboard_client = ExtDataControlClient{the_server()};
    clipboard_client.as_sink();

    EXPECT_CALL(clipboard_client, prepared_to_receive());
    EXPECT_CALL(copying_client, wrote_data())
        .WillOnce(
            [&]
            {
                auto read_string = clipboard_client.try_read();
                EXPECT_THAT(read_string, StrEq(test_message));
            });

    // Notify the server that the copying client is the paste source
    copying_client.roundtrip();

    // To receive the offer and send the `send` request
    clipboard_client.roundtrip();

    // To receive the `send` event
    copying_client.roundtrip();
}

TEST_F(ExtDataControlV1Test, client_copies_into_clipboard_that_started_before_it)
{
    ExtDataControlClient clipboard_client{the_server()};
    clipboard_client.as_sink();

    ExtDataControlClient copying_client{the_server()};
    copying_client.as_source();

    EXPECT_CALL(clipboard_client, prepared_to_receive());
    EXPECT_CALL(copying_client, wrote_data())
        .WillOnce(
            [&]
            {
                auto read_string = clipboard_client.try_read();
                EXPECT_THAT(read_string, StrEq(test_message));
            });

    // Set the the copying client as the current paste source
    copying_client.roundtrip();

    // Receive notification of the offer and send `send`
    clipboard_client.roundtrip();

    // Receive the send `event` and write the data
    copying_client.roundtrip();
}

TEST_F(ExtDataControlV1Test, client_pastes_from_clipboard_that_started_after_it)
{
    auto paste_client = ExtDataControlClient{the_server()};
    paste_client.as_sink();

    auto clipboard_client = ExtDataControlClient{the_server()};
    clipboard_client.as_source();

    EXPECT_CALL(paste_client, prepared_to_receive());
    EXPECT_CALL(clipboard_client, wrote_data())
        .WillOnce(
            [&]
            {
                auto read_string = paste_client.try_read();
                EXPECT_THAT(read_string, StrEq(test_message));
            });

    paste_client.roundtrip();
    clipboard_client.roundtrip();
    paste_client.roundtrip();
    clipboard_client.roundtrip();
}

TEST_F(ExtDataControlV1Test, client_pastes_from_clipboard_that_started_before_it)
{
    // Create clipboard data control manager, device, and source
    auto clipboard = ExtDataControlClient{the_server()};
    clipboard.as_source();

    auto paste_client = ExtDataControlClient{the_server()};
    paste_client.as_sink();

    EXPECT_CALL(paste_client, prepared_to_receive());
    EXPECT_CALL(clipboard, wrote_data())
        .WillOnce(
            [&]
            {
                auto read_string = paste_client.try_read();
                EXPECT_THAT(read_string, StrEq(test_message));
            });

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
    auto clipboard_client = ExtDataControlClient{the_server()};
    clipboard_client.as_source(); // Calls `set_selection`

    ext_data_control_device_v1_set_selection(
        clipboard_client.data_control_device, clipboard_client.source_data_control_source);

    EXPECT_PROTOCOL_ERROR(
        { clipboard_client.roundtrip(); },
        &ext_data_control_device_v1_interface,
        EXT_DATA_CONTROL_DEVICE_V1_ERROR_USED_SOURCE);
}

TEST_F(ExtDataControlV1Test, offering_mime_type_after_setting_selection_is_a_protocol_error)
{
    auto clipboard_client = ExtDataControlClient{the_server()};
    clipboard_client.as_source(); // Calls `offer`

    ext_data_control_source_v1_offer(clipboard_client.source_data_control_source, test_mime_type);

    EXPECT_PROTOCOL_ERROR(
        { clipboard_client.roundtrip(); },
        &ext_data_control_source_v1_interface,
        EXT_DATA_CONTROL_SOURCE_V1_ERROR_INVALID_OFFER);
}

TEST_F(ExtDataControlV1Test, copy_from_core_protocol_client_reaches_clipboard)
{
    CCnPSource source{the_server()};
    ExtDataControlClient clipboard{the_server()};
    clipboard.as_sink();

    EXPECT_CALL(clipboard, prepared_to_receive());
    EXPECT_CALL(source.data_source, wrote_data(_, _));

    source.offer(test_mime_type);

    clipboard.roundtrip();
    source.roundtrip();
}

TEST_F(ExtDataControlV1Test, paste_from_clipboard_reaches_core_protocol_client)
{
    ExtDataControlClient clipboard{the_server()};
    clipboard.as_source();

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
                wl_data_offer_receive(offer, mime, bogus_source_fd);

                clipboard.roundtrip();
                sink.roundtrip();
            }));

    clipboard.roundtrip();
    sink.roundtrip();

    clipboard.roundtrip();
    sink.roundtrip();
}

TEST_F(ExtDataControlV1Test, copy_from_primary_selection_client_reaches_clipboard)
{
    auto source_client = Client{the_server()};
    auto source_device_manager = source_client.bind_if_supported<zwp_primary_selection_device_manager_v1>(AnyVersion);
    auto source_device = PrimarySelectionDevice{source_device_manager, source_client.seat()};
    auto source_source = PrimarySelectionSource{source_device_manager};
    auto source_listener = MockPrimarySelectionSourceListener{source_source};

    auto clipboard = ExtDataControlClient{the_server()};
    clipboard.as_sink();

    // make offer
    zwp_primary_selection_source_v1_offer(source_source, test_mime_type);
    zwp_primary_selection_device_v1_set_selection(source_device, source_source, 0);

    // Notify the server of the offer and selection
    source_client.roundtrip();

    InSequence seq;
    EXPECT_CALL(clipboard, prepared_to_receive())
        .WillOnce(
            [&](auto...)
            {
                // Let the source client know so it can receive the `send` event
                source_client.roundtrip();
            });

    // Listen to `send` on the source
    EXPECT_CALL(source_listener, send(_, _, _));

    // Receive the selection event and request receive
    clipboard.roundtrip();
}

TEST_F(ExtDataControlV1Test, paste_from_clipboard_reaches_primary_selection_client)
{
    auto clipboard = ExtDataControlClient{the_server()};
    clipboard.as_source(SelectionType::primary);

    auto sink_client = Client{the_server()};
    auto sink_device_manager = sink_client.bind_if_supported<zwp_primary_selection_device_manager_v1>(AnyVersion);
    auto sink_device = PrimarySelectionDevice{sink_device_manager, sink_client.seat()};
    auto sink_source = PrimarySelectionSource{sink_device_manager};
    auto focused_surface = sink_client.create_visible_surface(42, 42);
    auto mpsol = MockPrimarySelectionOfferListener{};
    auto listener = MockPrimarySelectionDeviceListener{sink_device};

    zwp_primary_selection_offer_v1* current_offer = nullptr;
    char const* current_mime = nullptr;
    InSequence seq;
    EXPECT_CALL(listener, data_offer(_, _))
        .WillOnce(Invoke(
            [&](struct zwp_primary_selection_device_v1*, struct zwp_primary_selection_offer_v1* id)
            {
                mpsol.listen_to(id);
                current_offer = id;
            }));

    EXPECT_CALL(mpsol, offer(_, _))
        .WillOnce(Invoke(
            [&](struct zwp_primary_selection_offer_v1* offer, char const* mime)
            {
                EXPECT_THAT(offer, Eq(current_offer));
                current_mime = mime;
            }));

    EXPECT_CALL(listener, selection(_, _))
        .WillOnce(
            [&](zwp_primary_selection_device_v1*, zwp_primary_selection_offer_v1* offer)
            {
                EXPECT_THAT(offer, Eq(current_offer));
                zwp_primary_selection_offer_v1_receive(offer, current_mime, bogus_source_fd);

                clipboard.roundtrip();
                sink_client.roundtrip();
            });

    EXPECT_CALL(clipboard, wrote_data());

    // Set clipboard as selection
    clipboard.roundtrip();

    sink_client.roundtrip();
    clipboard.roundtrip();
    sink_client.roundtrip();
}

TEST_F(ExtDataControlV1Test, data_copied_into_clipboard_is_the_same_as_data_pasted_from_clipboard)
{
    auto clipboard = ExtDataControlClient{the_server()};
    auto const message = "Heya!";
    std::string copied_message;
    {
        clipboard.as_sink();

        ExtDataControlClient copying_client{the_server()};
        copying_client.as_source(SelectionType::normal, message);

        EXPECT_CALL(clipboard, prepared_to_receive());
        EXPECT_CALL(copying_client, wrote_data())
            .WillOnce(
                [&]
                {
                    auto const received = clipboard.try_read();
                    EXPECT_THAT(received, StrEq(message));
                });

        copying_client.roundtrip();
        clipboard.roundtrip();
        copying_client.roundtrip();

    }

    {
        clipboard.as_source();

        ExtDataControlClient pasting_client{the_server()};
        pasting_client.as_sink();

        EXPECT_CALL(pasting_client, prepared_to_receive());
        EXPECT_CALL(clipboard, wrote_data())
            .WillOnce(
                [&]
                {
                    auto const received = pasting_client.try_read();
                    EXPECT_THAT(received, StrEq(message));
                });

        clipboard.roundtrip();
        pasting_client.roundtrip();
        clipboard.roundtrip();
    }
}
