/*
 * Copyright © Canonical Ltd.
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
 */

#include "expect_protocol_error.h"
#include "ext_foreign_buffer_v1.h"
#include "ext_foreign_toplevel_list_v1.h"
#include "ext_image_copy_capture_v1.h"
#include "geometry/size.h"
#include "helpers.h"
#include "in_process_server.h"
#include "version_specifier.h"
#include "xdg_shell_stable.h"
#include "generated/viewporter-client.h"

#include <gmock/gmock.h>

#include <sys/socket.h>

#include <chrono>
#include <cstring>
#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

using namespace testing;
using SizeEvent = wlcs::ExtForeignBufferSessionV1::SizeEvent;

namespace wlcs
{
WLCS_CREATE_INTERFACE_DESCRIPTOR(wp_viewporter)
WLCS_CREATE_INTERFACE_DESCRIPTOR(wp_viewport)
}

namespace
{
uint32_t constexpr red = 0xffff0000;
uint32_t constexpr green = 0xff00ff00;
uint32_t constexpr blue = 0xff0000ff;
uint32_t constexpr yellow = 0xffffff00;
uint32_t constexpr magenta = 0xffff00ff;
uint32_t constexpr cyan = 0xff00ffff;
uint32_t constexpr white = 0xffffffff;

void fill(wlcs::ShmBuffer& buffer, uint32_t color)
{
    auto const data = buffer.data();
    for (size_t offset = 0; offset + sizeof(color) <= data.size(); offset += sizeof(color))
    {
        std::memcpy(data.data() + offset, &color, sizeof(color));
    }
}

void commit_buffer(wl_surface* surface, wl_buffer* buffer)
{
    wl_surface_attach(surface, buffer, 0, 0);
    wl_surface_damage_buffer(surface, 0, 0, INT32_MAX, INT32_MAX);
    wl_surface_commit(surface);
}

void dispatch_for(wlcs::Client& client, std::chrono::seconds duration)
{
    try
    {
        client.dispatch_until([] { return false; }, duration);
    }
    catch (wlcs::Timeout const&)
    {
    }
}

class Window
{
public:
    Window(wlcs::Client& client, std::string const& app_id)
        : client{client},
          surface{client},
          shell_surface{std::make_unique<NiceMock<wlcs::XdgSurfaceStable>>(client, surface)},
          toplevel{std::make_unique<NiceMock<wlcs::XdgToplevelStable>>(*shell_surface)}
    {
        ON_CALL(*shell_surface, configure(_)).WillByDefault([this](uint32_t serial)
            {
                xdg_surface_ack_configure(*shell_surface, serial);
                configured = true;
            });
        xdg_toplevel_set_app_id(*toplevel, app_id.c_str());
        wl_surface_commit(surface);
        client.dispatch_until([this] { return configured; });
    }

    Window(Window const&) = delete;
    Window& operator=(Window const&) = delete;

    void show(uint32_t color, int width = 100, int height = 100)
    {
        surface.attach_visible_buffer(new_buffer(color, width, height));
    }

    void show_pattern(std::function<uint32_t(int, int)> const& color_at, int width = 100, int height = 100)
    {
        auto& buffer = *buffers.emplace_back(std::make_unique<wlcs::ShmBuffer>(client, width, height));
        auto const data = buffer.data();
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                auto const color = color_at(x, y);
                std::memcpy(data.data() + (y * width + x) * sizeof(color), &color, sizeof(color));
            }
        }
        surface.attach_visible_buffer(buffer);
    }

    void commit_color(uint32_t color, int width = 100, int height = 100)
    {
        commit_buffer(surface, new_buffer(color, width, height));
        client.roundtrip();
    }

    void attach(wl_buffer* buffer)
    {
        commit_buffer(surface, buffer);
        client.roundtrip();
    }

    operator wl_surface*() const { return surface; }
    operator xdg_toplevel*() const { return *toplevel; }

    wlcs::Client& client;
    wlcs::Surface surface;

private:
    auto new_buffer(uint32_t color, int width, int height) -> wlcs::ShmBuffer&
    {
        auto& buffer = *buffers.emplace_back(std::make_unique<wlcs::ShmBuffer>(client, width, height));
        fill(buffer, color);
        return buffer;
    }

    std::unique_ptr<NiceMock<wlcs::XdgSurfaceStable>> const shell_surface;
    std::unique_ptr<NiceMock<wlcs::XdgToplevelStable>> const toplevel;
    std::vector<std::unique_ptr<wlcs::ShmBuffer>> buffers;
    bool configured{false};
};

struct CapturedFrame
{
    wlcs::Size size;
    std::vector<uint32_t> pixels;

    auto at(int x, int y) const -> uint32_t { return pixels[y * size.width.as_int() + x]; }
    auto centre() const -> uint32_t { return at(size.width.as_int() / 2, size.height.as_int() / 2); }
};

auto untransform(uint32_t transform, wlcs::Size buffer_size, std::vector<uint32_t> buffer) -> CapturedFrame
{
    if (transform == WL_OUTPUT_TRANSFORM_NORMAL)
    {
        return CapturedFrame{buffer_size, std::move(buffer)};
    }

    auto const buffer_width = buffer_size.width.as_int();
    auto const buffer_height = buffer_size.height.as_int();
    bool const swaps_axes = transform & WL_OUTPUT_TRANSFORM_90;
    auto const width = swaps_axes ? buffer_height : buffer_width;
    auto const height = swaps_axes ? buffer_width : buffer_height;

    CapturedFrame image{wlcs::Size{width, height}, std::vector<uint32_t>(buffer.size())};
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            auto const [bx, by] = [&]() -> std::pair<int, int>
                {
                    switch (transform)
                    {
                    case WL_OUTPUT_TRANSFORM_FLIPPED:     return {width - 1 - x, y};
                    case WL_OUTPUT_TRANSFORM_90:          return {y, width - 1 - x};
                    case WL_OUTPUT_TRANSFORM_FLIPPED_90:  return {y, x};
                    case WL_OUTPUT_TRANSFORM_180:         return {width - 1 - x, height - 1 - y};
                    case WL_OUTPUT_TRANSFORM_FLIPPED_180: return {x, height - 1 - y};
                    case WL_OUTPUT_TRANSFORM_270:         return {height - 1 - y, x};
                    case WL_OUTPUT_TRANSFORM_FLIPPED_270: return {height - 1 - y, width - 1 - x};
                    default:                              return {x, y};
                    }
                }();
            image.pixels[y * width + x] = buffer[by * buffer_width + bx];
        }
    }
    return image;
}

class Capture
{
public:
    Capture(wlcs::Client& client, wlcs::WlHandle<ext_image_capture_source_v1> source, ext_image_copy_capture_manager_v1* manager)
        : client{client},
          source{std::move(source)},
          session{ext_image_copy_capture_manager_v1_create_session(manager, this->source, 0)}
    {
        client.dispatch_until([this] { return session.buffer_size() && !session.is_dirty(); });
    }

    auto size() -> wlcs::Size
    {
        client.roundtrip();
        client.dispatch_until([this] { return session.buffer_size() && !session.is_dirty(); });
        return session.buffer_size().value();
    }

    auto next_frame(std::chrono::seconds timeout) -> std::optional<CapturedFrame>
    {
        for (int attempt = 0; attempt < 5; ++attempt)
        {
            if (!frame)
            {
                start_frame();
            }

            try
            {
                client.dispatch_until([this] { return frame->is_ready() || frame->failure_reason(); }, timeout);
            }
            catch (wlcs::Timeout const&)
            {
                return std::nullopt;
            }

            auto const reason = frame->failure_reason();
            if (!reason)
            {
                auto const transform = frame->transform();
                frame.reset();
                auto const data = target->data();
                std::vector<uint32_t> pixels(data.size() / sizeof(uint32_t));
                std::memcpy(pixels.data(), data.data(), data.size());
                return untransform(transform, target_size, std::move(pixels));
            }

            frame.reset();
            if (*reason != EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_BUFFER_CONSTRAINTS)
            {
                ADD_FAILURE() << "Capture failed with reason " << *reason;
                return std::nullopt;
            }

            client.dispatch_until([this] { return session.buffer_size() && !session.is_dirty(); });
        }

        ADD_FAILURE() << "Capture repeatedly failed buffer constraints";
        return std::nullopt;
    }

    auto wait_for(std::function<bool(CapturedFrame const&)> const& matches, std::string const& description) -> AssertionResult
    {
        auto const deadline = std::chrono::steady_clock::now() + wlcs::helpers::a_long_time();
        std::optional<CapturedFrame> last;
        while (std::chrono::steady_clock::now() < deadline)
        {
            auto const remaining = std::chrono::ceil<std::chrono::seconds>(deadline - std::chrono::steady_clock::now());
            if (auto image = next_frame(std::max(remaining, std::chrono::seconds{1})))
            {
                if (matches(*image))
                {
                    return AssertionSuccess();
                }

                last = std::move(image);
            }
        }

        auto failure = AssertionFailure() << "expected " << description << ", ";
        if (last)
        {
            failure << "last captured " << last->size << " with centre " << std::hex << last->centre();
        }
        else
        {
            failure << "no frame captured";
        }

        return failure;
    }

    auto wait_for_color(uint32_t color) -> AssertionResult
    {
        std::stringstream description;
        description << "color " << std::hex << color;
        return wait_for([color](CapturedFrame const& image) { return image.centre() == color; }, description.str());
    }

    auto shows_no_frame_or(uint32_t color) -> AssertionResult
    {
        auto const image = next_frame(wlcs::helpers::a_short_time());
        if (!image || image->centre() == color)
        {
            return AssertionSuccess();
        }
        return AssertionFailure() << "expected color " << std::hex << color << " or no frame, captured "
                                  << image->centre();
    }

private:
    void start_frame()
    {
        auto const size = session.buffer_size().value();
        if (!target || target_size != size)
        {
            target = std::make_unique<wlcs::ShmBuffer>(client, size.width.as_int(), size.height.as_int());
            target_size = size;
        }
        frame.emplace(ext_image_copy_capture_session_v1_create_frame(session));
        ext_image_copy_capture_frame_v1_attach_buffer(*frame, *target);
        ext_image_copy_capture_frame_v1_damage_buffer(*frame, 0, 0, size.width.as_int(), size.height.as_int());
        ext_image_copy_capture_frame_v1_capture(*frame);
    }

    wlcs::Client& client;
    wlcs::WlHandle<ext_image_capture_source_v1> const source;
    wlcs::ImageCopyCaptureSession session;
    std::unique_ptr<wlcs::ShmBuffer> target;
    wlcs::Size target_size;
    std::optional<wlcs::ImageCopyCaptureFrame> frame;
};

class ExtForeignBufferTest : public wlcs::StartedInProcessServer
{
public:
    wlcs::Client source_client{the_server()};
    wlcs::Client consumer{the_server()};
    wlcs::WlHandle<ext_foreign_buffer_manager_v1> manager{
        consumer.bind_if_supported<ext_foreign_buffer_manager_v1>(wlcs::AnyVersion)};
    wlcs::WlHandle<ext_foreign_toplevel_image_capture_source_manager_v1> toplevel_sources{
        consumer.bind_if_supported<ext_foreign_toplevel_image_capture_source_manager_v1>(wlcs::AnyVersion)};
    wlcs::WlHandle<ext_image_copy_capture_manager_v1> capture_manager{
        consumer.bind_if_supported<ext_image_copy_capture_manager_v1>(wlcs::AnyVersion)};
    wlcs::ExtForeignToplevelListV1 toplevel_list{consumer};
    std::list<wlcs::WlHandle<ext_image_capture_source_v1>> capture_sources;

    auto make_toplevel_source(std::string const& app_id, uint32_t color, int width = 100, int height = 100)
        -> std::unique_ptr<Window>
    {
        return make_toplevel_source(source_client, app_id, color, width, height);
    }

    auto make_toplevel_source(wlcs::Client& client, std::string const& app_id, uint32_t color, int width = 100, int height = 100)
        -> std::unique_ptr<Window>
    {
        auto window = std::make_unique<Window>(client, app_id);
        window->show(color, width, height);
        toplevel(app_id);
        return window;
    }

    /// The consumer's handle for the (mapped) toplevel with \p app_id
    auto toplevel(std::string const& app_id) -> wlcs::ExtForeignToplevelHandleV1 const&
    {
        consumer.dispatch_until([&]
            {
                for (auto const& handle : toplevel_list.toplevels())
                {
                    if (handle->app_id() == app_id && !handle->is_dirty() && !handle->closed())
                    {
                        return true;
                    }
                }
                return false;
            });
        for (auto const& handle : toplevel_list.toplevels())
        {
            if (handle->app_id() == app_id && !handle->closed())
            {
                return *handle;
            }
        }
        BOOST_THROW_EXCEPTION(std::runtime_error{"No toplevel with app ID " + app_id});
    }

    auto capture_source_for(std::string const& app_id) -> wlcs::WlHandle<ext_image_capture_source_v1>
    {
        return wlcs::wrap_wl_object(
            ext_foreign_toplevel_image_capture_source_manager_v1_create_source(toplevel_sources, toplevel(app_id)));
    }

    /// A foreign buffer session for \p app_id. The capture source is kept alive for the whole test.
    auto session_for(std::string const& app_id, ext_foreign_buffer_manager_v1* from = nullptr)
        -> std::unique_ptr<wlcs::ExtForeignBufferSessionV1>
    {
        auto const& source = capture_sources.emplace_back(capture_source_for(app_id));
        return std::make_unique<wlcs::ExtForeignBufferSessionV1>(
            ext_foreign_buffer_manager_v1_get_session(from ? from : static_cast<ext_foreign_buffer_manager_v1*>(manager), source));
    }

    /// Requests a buffer and waits for its size event
    auto buffer_for(wlcs::ExtForeignBufferSessionV1& session) -> wlcs::WlHandle<wl_buffer>
    {
        auto const sizes = session.sizes().size();
        auto buffer = session.get_buffer();
        session.dispatch_until_size(consumer, sizes + 1);
        return buffer;
    }

    auto capture_of(std::string const& app_id) -> std::unique_ptr<Capture>
    {
        return std::make_unique<Capture>(consumer, capture_source_for(app_id), capture_manager);
    }

    /// A window paired with a capture that automatically captures its latest contents
    auto make_window_capture_pair(std::string const& app_id = "consumer")
        -> std::pair<std::unique_ptr<Window>, std::unique_ptr<Capture>>
    {
        auto window = std::make_unique<Window>(consumer, app_id);
        window->show(blue);
        auto capture = capture_of(app_id);
        EXPECT_TRUE(capture->wait_for_color(blue));
        return {std::move(window), std::move(capture)};
    }

    /// The color shown by \p app_id, from a new capture session whose first frame is always complete
    auto current_color(std::string const& app_id) -> std::optional<uint32_t>
    {
        auto const image = capture_of(app_id)->next_frame(wlcs::helpers::a_long_time());
        return image ? std::optional{image->centre()} : std::nullopt;
    }

    /// Checks the server is still working, by mapping a new toplevel from a new client
    void expect_server_still_works()
    {
        wlcs::Client client{the_server()};
        auto const window = make_toplevel_source(client, "server-still-works", white);
        consumer.roundtrip();
    }
};

/// The standard arrangement: a red source, a session for it with a buffer, and a consumer showing that buffer
class MappedExtForeignBufferTest : public ExtForeignBufferTest
{
public:
    void SetUp() override
    {
        ExtForeignBufferTest::SetUp();
        source = make_toplevel_source("source", red);
        session = session_for("source");
        buffer.emplace(buffer_for(*session));
        std::tie(consumer_window, consumer_capture) = make_window_capture_pair();
        consumer_window->attach(*buffer);
        ASSERT_TRUE(consumer_capture->wait_for_color(red));
    }

    std::unique_ptr<Window> source;
    std::unique_ptr<wlcs::ExtForeignBufferSessionV1> session;
    std::optional<wlcs::WlHandle<wl_buffer>> buffer;
    std::unique_ptr<Window> consumer_window;
    std::unique_ptr<Capture> consumer_capture;
};
}

TEST_F(ExtForeignBufferTest, manager_can_be_bound)
{
    consumer.roundtrip();
}

TEST_F(ExtForeignBufferTest, manager_can_be_destroyed_without_sessions)
{
    {
        auto const second_manager = consumer.bind_if_supported<ext_foreign_buffer_manager_v1>(wlcs::AnyVersion);
    }
    consumer.roundtrip();
}

TEST_F(ExtForeignBufferTest, can_create_session_from_toplevel_source)
{
    auto const source = make_toplevel_source("source", red);
    auto const session = session_for("source");
    consumer.roundtrip();
    EXPECT_NE(session, nullptr);
}

TEST_F(ExtForeignBufferTest, no_events_before_get_buffer)
{
    auto const source = make_toplevel_source("source", red);
    auto const session = session_for("source");
    consumer.roundtrip();

    // Neither content nor size changes are reported while there is no buffer
    source->show(green);
    source->show(green, 160, 120);
    consumer.roundtrip();

    EXPECT_THAT(session->sizes(), IsEmpty());
    EXPECT_THAT(session->invalidated_count(), Eq(0));
    EXPECT_THAT(session->stopped_count(), Eq(0));
}

TEST_F(ExtForeignBufferTest, session_remains_usable_after_manager_destroyed)
{
    auto const source = make_toplevel_source("source", red);
    std::optional<wlcs::WlHandle<ext_foreign_buffer_manager_v1>> second_manager;
    second_manager.emplace(consumer.bind_if_supported<ext_foreign_buffer_manager_v1>(wlcs::AnyVersion));
    auto const session = session_for("source", *second_manager);
    second_manager.reset();
    consumer.roundtrip();

    auto const buffer = buffer_for(*session);
    EXPECT_THAT(session->sizes(), ElementsAre(SizeEvent{100, 100, 100, 100}));

    auto const [consumer_window, consumer_capture] = make_window_capture_pair();
    consumer_window->attach(buffer);
    EXPECT_TRUE(consumer_capture->wait_for_color(red));
}

TEST_F(ExtForeignBufferTest, session_remains_usable_after_capture_source_destroyed)
{
    auto const source = make_toplevel_source("source", red);
    std::unique_ptr<wlcs::ExtForeignBufferSessionV1> session;
    {
        auto const capture_source = capture_source_for("source");
        session = std::make_unique<wlcs::ExtForeignBufferSessionV1>(
            ext_foreign_buffer_manager_v1_get_session(manager, capture_source));
    }
    consumer.roundtrip();

    auto const buffer = buffer_for(*session);
    auto const [consumer_window, consumer_capture] = make_window_capture_pair();
    consumer_window->attach(buffer);
    EXPECT_TRUE(consumer_capture->wait_for_color(red));

    source->show(green);
    EXPECT_TRUE(consumer_capture->wait_for_color(green));
}

TEST_F(ExtForeignBufferTest, sessions_for_the_same_source_are_independent)
{
    auto const source = make_toplevel_source("source", red);
    auto const first = session_for("source");
    auto const second = session_for("source");

    // Each session can have its own active buffer
    auto const first_buffer = buffer_for(*first);
    auto const second_buffer = buffer_for(*second);
    EXPECT_THAT(first->sizes(), SizeIs(1));
    EXPECT_THAT(second->sizes(), SizeIs(1));

    auto const [first_consumer_window, first_consumer_capture] = make_window_capture_pair("first-consumer");
    auto const [second_consumer_window, second_consumer_capture] = make_window_capture_pair("second-consumer");
    first_consumer_window->attach(first_buffer);
    second_consumer_window->attach(second_buffer);
    EXPECT_TRUE(first_consumer_capture->wait_for_color(red));
    EXPECT_TRUE(second_consumer_capture->wait_for_color(red));

    source->show(green);
    EXPECT_TRUE(first_consumer_capture->wait_for_color(green));
    EXPECT_TRUE(second_consumer_capture->wait_for_color(green));
}

TEST_F(ExtForeignBufferTest, multiple_managers_can_be_used_by_one_client)
{
    auto const source = make_toplevel_source("source", red);
    auto const second_manager = consumer.bind_if_supported<ext_foreign_buffer_manager_v1>(wlcs::AnyVersion);
    auto const first = session_for("source");
    auto const second = session_for("source", second_manager);

    auto const first_buffer = buffer_for(*first);
    auto const second_buffer = buffer_for(*second);
    EXPECT_THAT(first->sizes(), ElementsAre(SizeEvent{100, 100, 100, 100}));
    EXPECT_THAT(second->sizes(), ElementsAre(SizeEvent{100, 100, 100, 100}));
}

TEST_F(ExtForeignBufferTest, get_buffer_sends_size_of_toplevel)
{
    auto const source = make_toplevel_source("source", red, 120, 80);
    auto const session = session_for("source");
    auto const buffer = buffer_for(*session);
    EXPECT_THAT(session->sizes(), ElementsAre(SizeEvent{120, 80, 120, 80}));
}

TEST_F(ExtForeignBufferTest, size_is_sent_once_per_get_buffer)
{
    auto const source = make_toplevel_source("source", red);
    auto const session = session_for("source");
    auto const buffer = buffer_for(*session);
    auto const [consumer_window, consumer_capture] = make_window_capture_pair();
    consumer_window->attach(buffer);

    source->show(green);
    ASSERT_TRUE(consumer_capture->wait_for_color(green));
    source->show(yellow);
    ASSERT_TRUE(consumer_capture->wait_for_color(yellow));
    consumer.roundtrip();

    EXPECT_THAT(session->sizes(), SizeIs(1));
}

TEST_F(ExtForeignBufferTest, size_of_scaled_toplevel_is_scaled)
{
    auto const source = std::make_unique<Window>(source_client, "source");
    wl_surface_set_buffer_scale(*source, 2);
    source->show(red, 200, 160);
    auto const session = session_for("source");

    auto const buffer = buffer_for(*session);

    ASSERT_THAT(session->sizes(), SizeIs(1));
    auto const size = session->sizes()[0];
    EXPECT_THAT(size.logical_width, Eq(100u));
    EXPECT_THAT(size.logical_height, Eq(80u));
    EXPECT_THAT(size, AnyOf(Eq(SizeEvent{100, 80, 100, 80}), Eq(SizeEvent{200, 160, 100, 80})));
}

TEST_F(ExtForeignBufferTest, get_buffer_while_buffer_active_is_an_error)
{
    auto const source = make_toplevel_source("source", red);
    auto const session = session_for("source");
    auto const buffer = buffer_for(*session);

    EXPECT_PROTOCOL_ERROR(
        {
            auto const second_buffer = session->get_buffer();
            consumer.roundtrip();
        },
        &ext_foreign_buffer_session_v1_interface,
        EXT_FOREIGN_BUFFER_SESSION_V1_ERROR_BUFFER_ACTIVE);
}

TEST_F(ExtForeignBufferTest, get_buffer_while_unacknowledged_buffer_active_is_an_error)
{
    auto const source = make_toplevel_source("source", red);
    auto const session = session_for("source");

    EXPECT_PROTOCOL_ERROR(
        {
            auto const first_buffer = session->get_buffer();
            auto const second_buffer = session->get_buffer();
            consumer.roundtrip();
        },
        &ext_foreign_buffer_session_v1_interface,
        EXT_FOREIGN_BUFFER_SESSION_V1_ERROR_BUFFER_ACTIVE);
}

TEST_F(ExtForeignBufferTest, get_buffer_after_destroying_buffer_sends_size_without_source_damage)
{
    auto const source = make_toplevel_source("source", red);
    auto const session = session_for("source");
    {
        auto const buffer = buffer_for(*session);
    }

    auto const buffer = buffer_for(*session);
    EXPECT_THAT(session->sizes(), ElementsAre(SizeEvent{100, 100, 100, 100}, SizeEvent{100, 100, 100, 100}));
}

TEST_F(ExtForeignBufferTest, get_buffer_after_invalidated_without_destroying_buffer_is_an_error)
{
    auto const source = make_toplevel_source("source", red);
    auto const session = session_for("source");
    auto const buffer = buffer_for(*session);
    source->show(red, 160, 120);
    consumer.dispatch_until([&] { return session->invalidated_count() > 0; });

    EXPECT_PROTOCOL_ERROR(
        {
            auto const second_buffer = session->get_buffer();
            consumer.roundtrip();
        },
        &ext_foreign_buffer_session_v1_interface,
        EXT_FOREIGN_BUFFER_SESSION_V1_ERROR_BUFFER_ACTIVE);
}

TEST_F(MappedExtForeignBufferTest, get_buffer_after_destroying_attached_buffer_succeeds)
{
    buffer.reset();
    consumer.roundtrip();

    auto const new_buffer = buffer_for(*session);
    consumer_window->attach(new_buffer);
    source->show(green);

    EXPECT_TRUE(consumer_capture->wait_for_color(green));
}

TEST_F(ExtForeignBufferTest, attaching_foreign_buffer_maps_toplevel)
{
    auto const source = make_toplevel_source("source", red);
    auto const session = session_for("source");
    auto const buffer = buffer_for(*session);
    Window const window{consumer, "consumer"};
    commit_buffer(window, buffer);
    consumer.roundtrip();
    toplevel("consumer");
    auto const capture = capture_of("consumer");
    EXPECT_THAT(capture->size(), Eq(wlcs::Size{100, 100}));
    EXPECT_THAT(current_color("consumer"), Optional(red));
}

TEST_F(ExtForeignBufferTest, content_matches_source)
{
    // A different color in each quadrant, to catch the content being flipped or rotated
    auto const quadrant_color = [](int x, int y)
        {
            return y < 50 ? (x < 50 ? red : green) : (x < 50 ? yellow : magenta);
        };
    auto const source = std::make_unique<Window>(source_client, "source");
    source->show_pattern(quadrant_color);
    toplevel("source");
    auto const session = session_for("source");
    auto const buffer = buffer_for(*session);
    auto const [consumer_window, consumer_capture] = make_window_capture_pair();

    consumer_window->attach(buffer);

    EXPECT_TRUE(consumer_capture->wait_for(
        [&](CapturedFrame const& image)
        {
            if (image.size != wlcs::Size{100, 100})
            {
                return false;
            }
            for (int y = 0; y < 100; ++y)
            {
                for (int x = 0; x < 100; ++x)
                {
                    if (image.at(x, y) != quadrant_color(x, y))
                    {
                        return false;
                    }
                }
            }
            return true;
        },
        "the source's quadrants"));
}

TEST_F(MappedExtForeignBufferTest, content_updates_without_consumer_commit)
{
    for (auto const color : {green, yellow, magenta})
    {
        source->show(color);
        EXPECT_TRUE(consumer_capture->wait_for_color(color));
    }
}

TEST_F(MappedExtForeignBufferTest, content_keeps_updating_after_consumer_commit_without_attach)
{
    wl_surface_damage_buffer(*consumer_window, 0, 0, INT32_MAX, INT32_MAX);
    wl_surface_commit(*consumer_window);
    consumer.roundtrip();

    source->show(green);

    EXPECT_TRUE(consumer_capture->wait_for_color(green));
}

TEST_F(MappedExtForeignBufferTest, replacing_foreign_buffer_with_shm_buffer_stops_updates)
{
    consumer_window->commit_color(yellow);
    ASSERT_TRUE(consumer_capture->wait_for_color(yellow));

    source->show(green);
    EXPECT_TRUE(consumer_capture->shows_no_frame_or(yellow));

    consumer_window->commit_color(cyan);
    EXPECT_TRUE(consumer_capture->wait_for_color(cyan));
}

TEST_F(MappedExtForeignBufferTest, replacing_foreign_buffer_with_another_foreign_buffer)
{
    auto const other_source = make_toplevel_source("other-source", magenta);
    auto const other_session = session_for("other-source");
    auto const other_buffer = buffer_for(*other_session);

    consumer_window->attach(other_buffer);
    ASSERT_TRUE(consumer_capture->wait_for_color(magenta));

    source->show(green);
    EXPECT_TRUE(consumer_capture->shows_no_frame_or(magenta));

    other_source->show(cyan);
    EXPECT_TRUE(consumer_capture->wait_for_color(cyan));
}

TEST_F(MappedExtForeignBufferTest, reattaching_same_foreign_buffer_keeps_updating)
{
    consumer_window->attach(*buffer);
    consumer_window->attach(*buffer);

    source->show(green);

    EXPECT_TRUE(consumer_capture->wait_for_color(green));
}

TEST_F(MappedExtForeignBufferTest, buffer_still_works_after_attaching_null)
{
    commit_buffer(*consumer_window, nullptr);
    consumer.roundtrip();

    source->show(green);
    consumer.roundtrip();

    // The buffer can be attached again elsewhere, and has the latest content
    Window const window{consumer, "second-consumer"};
    commit_buffer(window, *buffer);
    consumer.roundtrip();
    toplevel("second-consumer");
    EXPECT_THAT(current_color("second-consumer"), Optional(green));
}

TEST_F(MappedExtForeignBufferTest, same_buffer_on_two_surfaces)
{
    auto const [second_consumer_window, second_consumer_capture] = make_window_capture_pair("second-consumer");
    second_consumer_window->attach(*buffer);
    ASSERT_TRUE(second_consumer_capture->wait_for_color(red));

    source->show(green);

    EXPECT_TRUE(consumer_capture->wait_for_color(green));
    EXPECT_TRUE(second_consumer_capture->wait_for_color(green));
}

TEST_F(ExtForeignBufferTest, foreign_buffer_on_subsurface)
{
    auto const source = make_toplevel_source("source", red, 50, 50);
    auto const session = session_for("source");
    auto const buffer = buffer_for(*session);
    auto const [consumer_window, consumer_capture] = make_window_capture_pair();

    wlcs::Subsurface subsurface{consumer_window->surface};
    wl_subsurface_set_position(subsurface, 50, 50);
    wl_subsurface_set_desync(subsurface);
    commit_buffer(subsurface, buffer);
    consumer_window->show(blue);

    auto const matches = [](uint32_t color)
        {
            return [color](CapturedFrame const& image)
                {
                    return image.size == wlcs::Size{100, 100} && image.at(25, 25) == blue && image.at(75, 75) == color;
                };
        };
    EXPECT_TRUE(consumer_capture->wait_for(matches(red), "red subsurface over blue parent"));

    source->show(green, 50, 50);
    EXPECT_TRUE(consumer_capture->wait_for(matches(green), "green subsurface over blue parent"));
}

TEST_F(MappedExtForeignBufferTest, consumer_buffer_scale_applies)
{
    wl_surface_set_buffer_scale(*consumer_window, 2);
    consumer_window->attach(*buffer);

    EXPECT_TRUE(consumer_capture->wait_for(
        [](CapturedFrame const& image) { return image.size == wlcs::Size{50, 50} && image.centre() == red; },
        "50x50 red"));
}

TEST_F(MappedExtForeignBufferTest, consumer_viewport_applies)
{
    auto const viewporter = consumer.bind_if_supported<wp_viewporter>(wlcs::AnyVersion);
    auto const viewport = wlcs::wrap_wl_object(wp_viewporter_get_viewport(viewporter, *consumer_window));
    wp_viewport_set_destination(viewport, 200, 150);
    consumer_window->attach(*buffer);

    EXPECT_TRUE(consumer_capture->wait_for(
        [](CapturedFrame const& image) { return image.size == wlcs::Size{200, 150} && image.centre() == red; },
        "200x150 red"));

    source->show(green);
    EXPECT_TRUE(consumer_capture->wait_for_color(green));
}

TEST_F(ExtForeignBufferTest, same_client_can_be_source_and_consumer)
{
    auto const source = make_toplevel_source(consumer, "source", red);
    auto const session = session_for("source");
    auto const buffer = buffer_for(*session);
    auto const [consumer_window, consumer_capture] = make_window_capture_pair();
    consumer_window->attach(buffer);
    ASSERT_TRUE(consumer_capture->wait_for_color(red));

    source->show(green);

    EXPECT_TRUE(consumer_capture->wait_for_color(green));
}

TEST_F(MappedExtForeignBufferTest, minimized_source_keeps_updating)
{
    xdg_toplevel_set_minimized(*source);
    source_client.roundtrip();

    source->commit_color(green);

    EXPECT_TRUE(consumer_capture->wait_for_color(green));
}

TEST_F(MappedExtForeignBufferTest, source_resize_sends_invalidated)
{
    source->show(red, 160, 120);

    consumer.dispatch_until([&] { return session->invalidated_count() > 0; });
}

TEST_F(MappedExtForeignBufferTest, invalidated_is_sent_once_per_buffer)
{
    source->show(red, 160, 120);
    consumer.dispatch_until([&] { return session->invalidated_count() > 0; });

    source->show(red, 200, 150);
    source->show(red, 100, 100);
    dispatch_for(consumer, wlcs::helpers::a_short_time());

    EXPECT_THAT(session->invalidated_count(), Eq(1));
}

TEST_F(MappedExtForeignBufferTest, invalidated_buffer_keeps_its_last_content_and_size)
{
    source->show(green, 160, 120);
    consumer.dispatch_until([&] { return session->invalidated_count() > 0; });

    EXPECT_TRUE(consumer_capture->shows_no_frame_or(red));
    EXPECT_THAT(consumer_capture->size(), Eq(wlcs::Size{100, 100}));
    EXPECT_THAT(current_color("consumer"), Optional(red));
}

TEST_F(MappedExtForeignBufferTest, new_buffer_after_invalidated_has_new_size_and_content)
{
    source->show(green, 160, 120);
    consumer.dispatch_until([&] { return session->invalidated_count() > 0; });

    buffer.reset();
    buffer.emplace(buffer_for(*session));
    EXPECT_THAT(session->sizes().back(), Eq(SizeEvent{160, 120, 160, 120}));

    consumer_window->attach(*buffer);
    EXPECT_TRUE(consumer_capture->wait_for(
        [](CapturedFrame const& image) { return image.size == wlcs::Size{160, 120} && image.centre() == green; },
        "160x120 green"));

    // The new buffer is not invalidated by content updates at its size
    source->show(yellow, 160, 120);
    EXPECT_TRUE(consumer_capture->wait_for_color(yellow));
    EXPECT_THAT(session->invalidated_count(), Eq(1));
}

TEST_F(MappedExtForeignBufferTest, content_change_does_not_invalidate)
{
    source->show(green);
    ASSERT_TRUE(consumer_capture->wait_for_color(green));
    source->show(yellow);
    ASSERT_TRUE(consumer_capture->wait_for_color(yellow));
    consumer.roundtrip();

    EXPECT_THAT(session->invalidated_count(), Eq(0));
}

TEST_F(ExtForeignBufferTest, resize_without_buffer_does_not_invalidate)
{
    auto const source = make_toplevel_source("source", red);
    auto const session = session_for("source");
    {
        auto const buffer = buffer_for(*session);
    }
    consumer.roundtrip();

    source->show(green, 160, 120);
    dispatch_for(consumer, wlcs::helpers::a_short_time());
    EXPECT_THAT(session->invalidated_count(), Eq(0));

    // The next buffer describes the new size directly
    auto const buffer = buffer_for(*session);
    EXPECT_THAT(session->sizes().back(), Eq(SizeEvent{160, 120, 160, 120}));
    auto const [consumer_window, consumer_capture] = make_window_capture_pair();
    consumer_window->attach(buffer);
    EXPECT_TRUE(consumer_capture->wait_for(
        [](CapturedFrame const& image) { return image.size == wlcs::Size{160, 120} && image.centre() == green; },
        "160x120 green"));
}

TEST_F(MappedExtForeignBufferTest, source_scale_change_keeps_logical_size)
{
    wl_surface_set_buffer_scale(*source, 2);
    source->show(green, 200, 200);

    // Whether this invalidates the buffer depends on whether the compositor renders at the source's scale
    dispatch_for(consumer, wlcs::helpers::a_short_time());
    if (session->invalidated_count() > 0)
    {
        buffer.reset();
        buffer.emplace(buffer_for(*session));
        consumer_window->attach(*buffer);
    }

    auto const size = session->sizes().back();
    EXPECT_THAT(size.logical_width, Eq(100u));
    EXPECT_THAT(size.logical_height, Eq(100u));
    EXPECT_TRUE(consumer_capture->wait_for_color(green));
}

TEST_F(MappedExtForeignBufferTest, source_subsurface_is_included_without_invalidating)
{
    std::optional<wlcs::Subsurface> subsurface{std::in_place, source->surface};
    wlcs::ShmBuffer subsurface_buffer{source_client, 50, 50};
    fill(subsurface_buffer, green);
    commit_buffer(*subsurface, subsurface_buffer);
    source->show(red);

    EXPECT_TRUE(consumer_capture->wait_for(
        [](CapturedFrame const& image) { return image.at(25, 25) == green && image.at(75, 75) == red; },
        "green subsurface over red"));

    subsurface.reset();
    source->show(red);

    EXPECT_TRUE(consumer_capture->wait_for(
        [](CapturedFrame const& image) { return image.at(25, 25) == red && image.at(75, 75) == red; },
        "red without subsurface"));
    EXPECT_THAT(session->invalidated_count(), Eq(0));
}

TEST_F(MappedExtForeignBufferTest, destroying_source_toplevel_sends_stopped_once)
{
    source.reset();
    source_client.roundtrip();
    consumer.dispatch_until([&] { return session->stopped_count() > 0; });
    dispatch_for(consumer, wlcs::helpers::a_short_time());
    EXPECT_THAT(session->stopped_count(), Eq(1));

    session.reset();
    consumer.roundtrip();
    expect_server_still_works();
}

TEST_F(MappedExtForeignBufferTest, source_client_disconnecting_sends_stopped)
{
    shutdown(wl_display_get_fd(source_client), SHUT_RDWR);

    consumer.dispatch_until([&] { return session->stopped_count() > 0; });
}

TEST_F(MappedExtForeignBufferTest, buffer_keeps_last_content_after_stopped)
{
    source->show(green);
    ASSERT_TRUE(consumer_capture->wait_for_color(green));

    source.reset();
    source_client.roundtrip();
    consumer.dispatch_until([&] { return session->stopped_count() > 0; });

    EXPECT_TRUE(consumer_capture->shows_no_frame_or(green));
    EXPECT_THAT(current_color("consumer"), Optional(green));
}

TEST_F(MappedExtForeignBufferTest, get_buffer_after_stopped_always_returns_the_same_buffer)
{
    source->show(green, 120, 80);
    consumer.dispatch_until([&] { return session->invalidated_count() > 0; });
    buffer.reset();
    buffer.emplace(buffer_for(*session));
    consumer_window->attach(*buffer);
    ASSERT_TRUE(consumer_capture->wait_for_color(green));

    source.reset();
    source_client.roundtrip();
    consumer.dispatch_until([&] { return session->stopped_count() > 0; });

    for (int i = 0; i < 3; ++i)
    {
        buffer.reset();
        buffer.emplace(buffer_for(*session));
        EXPECT_THAT(session->sizes().back(), Eq(SizeEvent{120, 80, 120, 80}));

        consumer_window->attach(*buffer);
        EXPECT_THAT(current_color("consumer"), Optional(green));
    }

    EXPECT_THAT(session->sizes(), SizeIs(5));
    EXPECT_THAT(session->invalidated_count(), Eq(1));
    EXPECT_THAT(session->stopped_count(), Eq(1));
}

TEST_F(ExtForeignBufferTest, session_for_closed_toplevel_sends_stopped)
{
    auto source = make_toplevel_source("source", red);
    auto const capture_source = capture_source_for("source");
    consumer.roundtrip();

    source.reset();
    source_client.roundtrip();
    consumer.roundtrip();

    wlcs::ExtForeignBufferSessionV1 const session{ext_foreign_buffer_manager_v1_get_session(manager, capture_source)};
    consumer.dispatch_until([&] { return session.stopped_count() > 0; });
    dispatch_for(consumer, wlcs::helpers::a_short_time());
    EXPECT_THAT(session.stopped_count(), Eq(1));
}

TEST_F(ExtForeignBufferTest, buffer_can_be_destroyed_before_its_size_arrives)
{
    auto const source = make_toplevel_source("source", red);
    auto const session = session_for("source");
    {
        auto const buffer = session->get_buffer();
    }
    consumer.roundtrip();

    auto const buffer = buffer_for(*session);
    EXPECT_THAT(session->sizes().back(), Eq(SizeEvent{100, 100, 100, 100}));
}

TEST_F(MappedExtForeignBufferTest, buffer_stays_valid_after_session_destroyed)
{
    session.reset();
    consumer.roundtrip();
    source->show(green);
    consumer.roundtrip();

    Window const window{consumer, "second-consumer"};
    commit_buffer(window, *buffer);
    consumer.roundtrip();
    toplevel("second-consumer");
    EXPECT_THAT(current_color("second-consumer"), Ne(std::nullopt));
    expect_server_still_works();
}

TEST_F(MappedExtForeignBufferTest, buffer_survives_consumer_window_being_destroyed)
{
    consumer_capture.reset();
    consumer_window.reset();
    consumer.roundtrip();
    source->show(green);
    consumer.roundtrip();

    Window const window{consumer, "second-consumer"};
    commit_buffer(window, *buffer);
    consumer.roundtrip();
    toplevel("second-consumer");
    EXPECT_THAT(current_color("second-consumer"), Optional(green));
}

TEST_F(MappedExtForeignBufferTest, consumer_client_can_disconnect_with_active_session)
{
    shutdown(wl_display_get_fd(consumer), SHUT_RDWR);

    source->show(green);
    source->show(yellow);
    wlcs::Client client{the_server()};
    Window window{client, "new-client"};
    window.show(white);
}

TEST_F(ExtForeignBufferTest, toplevel_can_show_its_own_content)
{
    auto const window = make_toplevel_source(consumer, "consumer", blue);
    auto const session = session_for("consumer");
    auto const buffer = buffer_for(*session);

    window->attach(buffer);
    dispatch_for(consumer, wlcs::helpers::a_short_time());

    EXPECT_THAT(session->sizes(), SizeIs(1));
    EXPECT_THAT(session->invalidated_count(), Eq(0));
    expect_server_still_works();
}

TEST_F(ExtForeignBufferTest, toplevels_can_show_each_others_content)
{
    auto const first = make_toplevel_source(consumer, "first", red);
    auto const second = make_toplevel_source(consumer, "second", green);
    auto const first_session = session_for("first");
    auto const second_session = session_for("second");
    auto const first_buffer = buffer_for(*first_session);
    auto const second_buffer = buffer_for(*second_session);

    first->attach(second_buffer);
    second->attach(first_buffer);
    dispatch_for(consumer, wlcs::helpers::a_short_time());

    EXPECT_THAT(first_session->sizes(), SizeIs(1));
    EXPECT_THAT(second_session->sizes(), SizeIs(1));
    expect_server_still_works();
}

TEST_F(MappedExtForeignBufferTest, foreign_buffer_can_be_used_as_cursor)
{
    the_server().move_surface_to(consumer_window->surface, 0, 0);
    auto pointer = the_server().create_pointer();
    auto const serial = consumer.move_pointer_to(pointer, 50, 50);

    wlcs::Surface cursor{consumer};
    commit_buffer(cursor, *buffer);
    wl_pointer_set_cursor(consumer.the_pointer(), serial, cursor, 0, 0);
    consumer.roundtrip();

    source->show(green);
    pointer.move_to(60, 60);
    consumer.roundtrip();
    expect_server_still_works();
}

namespace
{
class ExtForeignBufferOutputTest : public ExtForeignBufferTest
{
public:
    wlcs::WlHandle<ext_output_image_capture_source_manager_v1> output_sources{
        consumer.bind_if_supported<ext_output_image_capture_source_manager_v1>(wlcs::AnyVersion)};

    auto output_session() -> std::unique_ptr<wlcs::ExtForeignBufferSessionV1>
    {
        auto const& source = capture_sources.emplace_back(wlcs::wrap_wl_object(
            ext_output_image_capture_source_manager_v1_create_source(output_sources, consumer.output_state(0).output)));
        return std::make_unique<wlcs::ExtForeignBufferSessionV1>(ext_foreign_buffer_manager_v1_get_session(manager, source));
    }
};
}

TEST_F(ExtForeignBufferOutputTest, get_buffer_sends_size_of_output)
{
    auto const output = consumer.output_state(0);
    ASSERT_THAT(output.mode_size, Ne(std::nullopt));
    auto const [width, height] = output.mode_size.value();
    auto const scale = output.scale.value_or(1);
    auto const session = output_session();

    auto const buffer = buffer_for(*session);

    EXPECT_THAT(session->sizes(), ElementsAre(SizeEvent{
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height),
        static_cast<uint32_t>(width / scale),
        static_cast<uint32_t>(height / scale)}));
}

TEST_F(ExtForeignBufferOutputTest, output_content_is_shown_and_updates)
{
    auto const output = consumer.output_state(0);
    auto const scale = output.scale.value_or(1);
    auto const source = make_toplevel_source("source", red);
    the_server().move_surface_to(source->surface, 0, 0);
    source->show(red);

    auto const session = output_session();
    auto const buffer = buffer_for(*session);
    auto const [consumer_window, consumer_capture] = make_window_capture_pair();
    // Keep the consumer away from the source, so it doesn't cover it
    the_server().move_surface_to(consumer_window->surface, 200, 200);
    wl_surface_set_buffer_scale(*consumer_window, scale);
    consumer_window->attach(buffer);

    auto const source_pixel_is = [scale](uint32_t color)
        {
            return [scale, color](CapturedFrame const& image)
                {
                    return image.size.width.as_int() > 50 * scale && image.size.height.as_int() > 50 * scale &&
                           image.at(50 * scale, 50 * scale) == color;
                };
        };
    EXPECT_TRUE(consumer_capture->wait_for(source_pixel_is(red), "red source at (50, 50)"));

    source->show(green);
    EXPECT_TRUE(consumer_capture->wait_for(source_pixel_is(green), "green source at (50, 50)"));
}
