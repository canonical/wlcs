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

    auto source() const -> int
    {
        return pipe_ends[0];
    }

    auto sink() const -> int
    {
        return pipe_ends[1];
    }

    ~Pipe()
    {
        close(pipe_ends[0]);
        close(pipe_ends[1]);
    }
};

struct DataControlOfferWrapper
{
    WlHandle<ext_data_control_offer_v1> const offer;
    std::vector<std::string> mime_types;

    static void data_offer_offer(void* data, struct ext_data_control_offer_v1*, char const* mime_type)
    {
        auto* offer = static_cast<DataControlOfferWrapper*>(data);
        std::string const mime_string{mime_type};
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
    // Works as the message if source, or as store for received messages if sink.
    std::optional<std::string> received_message{std::nullopt};

    WlHandle<ext_data_control_source_v1> const source_data_control_source;
    std::function<void()> source_content_sent{[]{}};

    // Have to use a pointer since the offer wrapper uses `this` as the
    // userdata pointer, which with a normal object would be allocated on the
    // stack. Once the stack is cleaned up at the end of `data_offer`'s end,
    // the listener's pointer would be pointing at garbage.
    std::unique_ptr<DataControlOfferWrapper> sink_current_offer{nullptr};
    std::optional<Pipe> sink_receiving_pipe{std::nullopt};

    MOCK_METHOD(void, offer_received, ());
    MOCK_METHOD(void, selection_set, ());

    static void data_control_data_offer(
        void* data, struct ext_data_control_device_v1*, struct ext_data_control_offer_v1* id)
    {
        auto* self = static_cast<ExtDataControlClient*>(data);

        if (id == nullptr)
            return;

        self->sink_current_offer = std::make_unique<DataControlOfferWrapper>(id);
        self->offer_received();
    }


    static void data_control_selection(
        void* data, struct ext_data_control_device_v1*, struct ext_data_control_offer_v1* id)
    {
        auto* self = static_cast<ExtDataControlClient*>(data);

        if (id == nullptr)
            return;

        EXPECT_THAT(self->sink_current_offer, NotNull());
        EXPECT_THAT(self->sink_current_offer->offer, Eq(id));
        EXPECT_THAT(self->sink_current_offer->mime_types.size(), Gt(0));

        ext_data_control_offer_v1_receive(
            self->sink_current_offer->offer,
            self->sink_current_offer->mime_types[0].c_str(),
            self->sink_receiving_pipe->source());

        self->roundtrip(); // Make sure the server is notified of the receive request

        self->selection_set();
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

        self->source_content_sent();
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
        sink_receiving_pipe.emplace();

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
        auto const read_chars = read(sink_receiving_pipe->sink(), buf, sizeof(buf));
        received_message = std::string{buf, static_cast<size_t>(read_chars)};
        return *received_message;
    }

    struct SourceOptions
    {
        SelectionType selection = SelectionType::normal;
        std::optional<std::string> message = {};
        std::function<void()> when_content_sent = []{};
    };

    void as_source(SourceOptions const& options)
    {
        if (options.message)
            received_message = options.message;

        this->source_content_sent = options.when_content_sent;

        ext_data_control_source_v1_add_listener(
            source_data_control_source, &source_source_listener, this);

        // Offer plaintext
        ext_data_control_source_v1_offer(source_data_control_source, test_mime_type);

        switch (options.selection)
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
    ExtDataControlClient clipboard{the_server()};
};
}

TEST_F(ExtDataControlV1Test, client_copies_into_clipboard_that_started_after_it)
{
    ExtDataControlClient copying_client{the_server()};

    InSequence seq;
    EXPECT_CALL(clipboard, offer_received());
    EXPECT_CALL(clipboard, selection_set());

    // Notify the server that the copying client is the paste source
    copying_client.as_source({
        .when_content_sent =
            [&]
        {
            auto read_string = clipboard.try_read();
            EXPECT_THAT(read_string, StrEq(test_message));
        },
    });
    copying_client.roundtrip();

    // To receive the offer and send the `send` request
    clipboard.as_sink();
    clipboard.roundtrip();

    // To receive the `send` event
    copying_client.roundtrip();
}

TEST_F(ExtDataControlV1Test, client_copies_into_clipboard_that_started_before_it)
{
    ExtDataControlClient copying_client{the_server()};

    InSequence seq;
    EXPECT_CALL(clipboard, offer_received());
    EXPECT_CALL(clipboard, selection_set());

    clipboard.as_sink();
    clipboard.roundtrip();

    // Set the the copying client as the current paste source
    copying_client.as_source({
        .when_content_sent =
            [&]
        {
            auto read_string = clipboard.try_read();
            EXPECT_THAT(read_string, StrEq(test_message));
        },
    });
    copying_client.roundtrip();

    // Receive notification of the offer and the selection
    // Will request to receive from the client
    clipboard.roundtrip();

    // Receive the send `event` and write the data
    copying_client.roundtrip();
}

TEST_F(ExtDataControlV1Test, client_pastes_from_clipboard_that_started_after_it)
{
    ExtDataControlClient paste_client{the_server()};

    InSequence seq;
    EXPECT_CALL(paste_client, offer_received());
    EXPECT_CALL(paste_client, selection_set());

    paste_client.as_sink();
    paste_client.roundtrip();

    clipboard.as_source({
        .when_content_sent =
            [&]
        {
            auto read_string = paste_client.try_read();
            EXPECT_THAT(read_string, StrEq(test_message));
        },
    });
    clipboard.roundtrip();

    paste_client.roundtrip();
    clipboard.roundtrip();
}

TEST_F(ExtDataControlV1Test, client_pastes_from_clipboard_that_started_before_it)
{
    ExtDataControlClient clipboard{the_server()};
    ExtDataControlClient paste_client{the_server()};

    InSequence seq;
    EXPECT_CALL(paste_client, offer_received());
    EXPECT_CALL(paste_client, selection_set());

    // Create clipboard data control manager, device, and source
    clipboard.as_source({
        .when_content_sent =
            [&]
        {
            auto read_string = paste_client.try_read();
            EXPECT_THAT(read_string, StrEq(test_message));
        },
    });
    clipboard.roundtrip(); // Notify the server of the clipboard as a source;

    // The paste client device should be notified of the current offer from
    // the clipboard, then the selection should be set, triggering a
    // `receive()` request to the server, which the clipboard will receieve as
    // a `send()` event.
    paste_client.as_sink();
    paste_client.roundtrip();

    // The clipboard should get the `send()` call
    // from the server and write into the fd
    clipboard.roundtrip();
}

TEST_F(ExtDataControlV1Test, setting_the_same_selection_twice_is_a_protocol_error)
{
    clipboard.as_source({}); // Calls `set_selection`

    ext_data_control_device_v1_set_selection(
        clipboard.data_control_device, clipboard.source_data_control_source);

    EXPECT_PROTOCOL_ERROR(
        { clipboard.roundtrip(); },
        &ext_data_control_device_v1_interface,
        EXT_DATA_CONTROL_DEVICE_V1_ERROR_USED_SOURCE);
}

TEST_F(ExtDataControlV1Test, offering_mime_type_after_setting_selection_is_a_protocol_error)
{
    clipboard.as_source({}); // Calls `offer`

    ext_data_control_source_v1_offer(clipboard.source_data_control_source, test_mime_type);

    EXPECT_PROTOCOL_ERROR(
        { clipboard.roundtrip(); },
        &ext_data_control_source_v1_interface,
        EXT_DATA_CONTROL_SOURCE_V1_ERROR_INVALID_OFFER);
}

TEST_F(ExtDataControlV1Test, copy_from_core_protocol_client_reaches_clipboard)
{
    CCnPSource source{the_server()};

    InSequence seq;
    EXPECT_CALL(clipboard, offer_received());
    EXPECT_CALL(clipboard, selection_set());
    EXPECT_CALL(source.data_source, send(_, _));

    source.offer(test_mime_type); // Has a roundtrip built-in

    clipboard.as_sink();
    clipboard.roundtrip();

    source.roundtrip();
}

TEST_F(ExtDataControlV1Test, paste_from_clipboard_reaches_core_protocol_client)
{
    CCnPSink sink{the_server()};
    auto const surf = sink.create_surface_with_focus();

    MockDataOfferListener mdol;
    std::string current_mime;
    wl_data_offer* current_offer = nullptr;

    InSequence seq;
    EXPECT_CALL(sink.listener, data_offer(_, _))
        .WillOnce(Invoke(
            [&mdol, &current_offer](struct wl_data_device*, struct wl_data_offer* id)
            {
                mdol.listen_to(id);
                current_offer = id;
            }));
    EXPECT_CALL(mdol, offer(_, StrEq(test_mime_type)))
        .WillOnce(
            [&current_mime](auto, auto mime)
            {
                current_mime = std::string{mime};
            });

    Pipe pipe;
    EXPECT_CALL(sink.listener, selection(_, _))
        .WillOnce(
            [&current_offer, &current_mime, &pipe](auto, auto* offer)
            {
                EXPECT_THAT(offer, Eq(current_offer));
                EXPECT_THAT(current_mime, StrEq(test_mime_type));
                wl_data_offer_receive(offer, current_mime.c_str(), pipe.sink());
            });

    auto const msg =  "Hello, core protocol client!";
    auto mock_when_content_sent = MockFunction<void()>{};
    EXPECT_CALL(mock_when_content_sent, Call())
        .WillOnce(
            [&]
            {
                char buf[128];
                auto const read_chars = read(pipe.source(), buf, sizeof(buf));
                auto const read_string = std::string{buf, static_cast<size_t>(read_chars)};

                EXPECT_THAT(read_chars, Eq(strlen(msg)));
                EXPECT_THAT(read_string, StrEq(msg));
            });

    clipboard.as_source({
        .selection = SelectionType::normal,
        .message = msg,
        .when_content_sent = mock_when_content_sent.AsStdFunction(),
    });

    clipboard.roundtrip();
    sink.roundtrip();
    clipboard.roundtrip();
    sink.roundtrip();
}

TEST_F(ExtDataControlV1Test, copy_from_primary_selection_client_reaches_clipboard)
{
    Client source_client{the_server()};
    auto source_device_manager = source_client.bind_if_supported<zwp_primary_selection_device_manager_v1>(AnyVersion);
    PrimarySelectionDevice source_device{source_device_manager, source_client.seat()};
    PrimarySelectionSource source_source{source_device_manager};
    MockPrimarySelectionSourceListener source_listener{source_source};

    // make offer
    zwp_primary_selection_source_v1_offer(source_source, test_mime_type);
    zwp_primary_selection_device_v1_set_selection(source_device, source_source, 0);

    // Notify the server of the offer and selection
    source_client.roundtrip();

    InSequence seq;
    EXPECT_CALL(clipboard, offer_received());
    EXPECT_CALL(clipboard, selection_set())
        .WillOnce(
            [&](auto...)
            {
                // Let the source client know so it can receive the `send` event
                source_client.roundtrip();
            });

    // Listen to `send` on the source
    EXPECT_CALL(source_listener, send(_, _, _));

    // Receive the selection event and request receive
    clipboard.as_sink();
    clipboard.roundtrip();
}

TEST_F(ExtDataControlV1Test, paste_from_clipboard_reaches_primary_selection_client)
{
    Client sink_client{the_server()};

    auto sink_device_manager = sink_client.bind_if_supported<zwp_primary_selection_device_manager_v1>(AnyVersion);
    PrimarySelectionDevice sink_device{sink_device_manager, sink_client.seat()};
    auto focused_surface = sink_client.create_visible_surface(42, 42);

    MockPrimarySelectionOfferListener mpsol{};
    MockPrimarySelectionDeviceListener listener{sink_device};
    zwp_primary_selection_offer_v1* current_offer = nullptr;
    std::string current_mime;
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
                current_mime = std::string{mime};
            }));

    Pipe pipe;
    EXPECT_CALL(listener, selection(_, _))
        .WillOnce(
            [&](zwp_primary_selection_device_v1*, zwp_primary_selection_offer_v1* offer)
            {
                EXPECT_THAT(offer, Eq(current_offer));
                zwp_primary_selection_offer_v1_receive(offer, current_mime.c_str(), pipe.source());
            });

    auto const message = "message from primary clipboard";
    auto mock_when_content_sent = MockFunction<void()>{};
    EXPECT_CALL(mock_when_content_sent, Call())
        .WillOnce(
            [&]
            {
                char buffer[128];
                auto const read_chars = read(pipe.sink(), buffer, sizeof(buffer));
                auto const read_message = std::string{buffer, static_cast<size_t>(read_chars)};

                EXPECT_THAT(read_message, StrEq(message));
            });

    // Set clipboard as selection
    clipboard.as_source({
        .selection = SelectionType::primary,
        .message = message,
        .when_content_sent = mock_when_content_sent.AsStdFunction(),
    });

    clipboard.roundtrip();
    sink_client.roundtrip();
    clipboard.roundtrip();
    sink_client.roundtrip();
}

TEST_F(ExtDataControlV1Test, data_copied_into_clipboard_is_the_same_as_data_pasted_from_clipboard)
{
    auto const message = "Heya!";
    std::string copied_message;
    {
        ExtDataControlClient copying_client{the_server()};

        EXPECT_CALL(clipboard, offer_received());
        EXPECT_CALL(clipboard, selection_set());

        copying_client.as_source({
            .selection = SelectionType::normal,
            .message = message,
            .when_content_sent =
                [&]
            {
                auto const received = clipboard.try_read();
                EXPECT_THAT(received, StrEq(message));
            },
        });
        copying_client.roundtrip();

        clipboard.as_sink();
        clipboard.roundtrip();

        copying_client.roundtrip();

    }

    {
        ExtDataControlClient pasting_client{the_server()};

        EXPECT_CALL(pasting_client, offer_received());
        EXPECT_CALL(pasting_client, selection_set());

        clipboard.as_source({
            .when_content_sent =
                [&]
            {
                auto const received = pasting_client.try_read();
                EXPECT_THAT(received, StrEq(message));
            },
        });
        clipboard.roundtrip();

        pasting_client.as_sink();
        pasting_client.roundtrip();

        clipboard.roundtrip();
    }
}
