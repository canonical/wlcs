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

#include "geometry/rectangle.h"
#include "geometry/size.h"
#include "in_process_server.h"
#include "version_specifier.h"
#include "wl_interface_descriptor.h"
#include "generated/ext-image-capture-source-v1-client.h"
#include "generated/ext-image-copy-capture-v1-client.h"

#include <boost/throw_exception.hpp>
#include <gmock/gmock.h>

using namespace testing;

namespace wlcs {
WLCS_CREATE_INTERFACE_DESCRIPTOR(ext_output_image_capture_source_manager_v1)
WLCS_CREATE_INTERFACE_DESCRIPTOR(ext_image_capture_source_v1)
WLCS_CREATE_INTERFACE_DESCRIPTOR(ext_image_copy_capture_manager_v1)
WLCS_CREATE_INTERFACE_DESCRIPTOR(ext_image_copy_capture_session_v1)
WLCS_CREATE_INTERFACE_DESCRIPTOR(ext_image_copy_capture_cursor_session_v1)
WLCS_CREATE_INTERFACE_DESCRIPTOR(ext_image_copy_capture_frame_v1)
} // namespace wlcs

namespace {

class ImageCopyCaptureFrame
{
public:
    explicit ImageCopyCaptureFrame(ext_image_copy_capture_frame_v1* frame);
    ImageCopyCaptureFrame(ImageCopyCaptureFrame const&) = delete;
    ImageCopyCaptureFrame& operator=(ImageCopyCaptureFrame const&) = delete;

    auto is_ready() { return ready_; }
    auto failure_reason() { return failure_reason_; }
    auto damage() { return damage_; }

    operator ext_image_copy_capture_frame_v1*() const { return frame; }

private:
    wlcs::WlHandle<ext_image_copy_capture_frame_v1> const frame;

    std::optional<uint32_t> transform_;
    std::vector<wlcs::Rectangle> damage_;
    std::optional<uint32_t> presentation_time_;
    bool ready_ = false;
    std::optional<uint32_t> failure_reason_;
};

ImageCopyCaptureFrame::ImageCopyCaptureFrame(ext_image_copy_capture_frame_v1* frame)
    : frame{frame}
{
    static ext_image_copy_capture_frame_v1_listener const listener = {
        .transform = [](
            void* data,
            ext_image_copy_capture_frame_v1*,
            uint32_t transform)
            {
                auto self = static_cast<ImageCopyCaptureFrame*>(data);
                self->transform_ = transform;
            },
        .damage = [](
            void* data,
            ext_image_copy_capture_frame_v1*,
            int32_t x,
            int32_t y,
            int32_t width,
            int32_t height)
            {
                auto self = static_cast<ImageCopyCaptureFrame*>(data);
                self->damage_.emplace_back(wlcs::Rectangle{{x, y}, {width, height}});
            },
        .presentation_time = [](
            void* data,
            ext_image_copy_capture_frame_v1*,
            uint32_t tv_sec_hi,
            [[maybe_unused]] uint32_t tv_sec_lo,
            [[maybe_unused]] uint32_t tv_nsec)
            {
                auto self = static_cast<ImageCopyCaptureFrame*>(data);
                self->presentation_time_ = tv_sec_hi;
            },
        .ready = [](
            void* data,
            ext_image_copy_capture_frame_v1*)
            {
                auto self = static_cast<ImageCopyCaptureFrame*>(data);
                self->ready_ = true;
            },
        .failed = [](
            void* data,
            ext_image_copy_capture_frame_v1*,
            uint32_t reason)
            {
                auto self = static_cast<ImageCopyCaptureFrame*>(data);
                self->failure_reason_ = reason;
            },
    };
    ext_image_copy_capture_frame_v1_add_listener(frame, &listener, this);
}

class ImageCopyCaptureSession
{
public:
    explicit ImageCopyCaptureSession(ext_image_copy_capture_session_v1* session);
    ImageCopyCaptureSession(ImageCopyCaptureSession const&) = delete;
    ImageCopyCaptureSession& operator=(ImageCopyCaptureSession const&) = delete;

    auto is_dirty() const { return dirty_; }
    auto stopped() const { return stopped_; }
    auto buffer_size() const { return buffer_size_; }
    auto shm_formats() const { return shm_formats_; }

    operator ext_image_copy_capture_session_v1*() const { return session; }

private:
    wlcs::WlHandle<ext_image_copy_capture_session_v1> const session;

    bool dirty_ = false;
    bool stopped_ = false;
    std::optional<wlcs::Size> buffer_size_;
    std::vector<uint32_t> shm_formats_;

    void mark_dirty();
};

ImageCopyCaptureSession::ImageCopyCaptureSession(ext_image_copy_capture_session_v1* session)
    : session{session}
{
    static ext_image_copy_capture_session_v1_listener const listener = {
        .buffer_size = [](
            void* data,
            ext_image_copy_capture_session_v1*,
            uint32_t width,
            uint32_t height)
            {
                auto self = static_cast<ImageCopyCaptureSession*>(data);
                self->mark_dirty();
                self->buffer_size_ = wlcs::Size{width, height};
            },
        .shm_format = [](
            void* data,
            ext_image_copy_capture_session_v1*,
            uint32_t format)
            {
                auto self = static_cast<ImageCopyCaptureSession*>(data);
                self->mark_dirty();
                self->shm_formats_.push_back(format);
            },
        .dmabuf_device = [](
            [[maybe_unused]] void* data,
            ext_image_copy_capture_session_v1*,
            [[maybe_unused]] wl_array* device)
            {
            },
        .dmabuf_format = [](
            [[maybe_unused]] void* data,
            ext_image_copy_capture_session_v1*,
            [[maybe_unused]] uint32_t format,
            [[maybe_unused]] wl_array* modifiers)
            {
            },
        .done = [](
            void* data,
            ext_image_copy_capture_session_v1*)
            {
                auto self = static_cast<ImageCopyCaptureSession*>(data);
                self->dirty_ = false;
            },
        .stopped = [](
            void* data,
            ext_image_copy_capture_session_v1*)
            {
                auto self = static_cast<ImageCopyCaptureSession*>(data);
                self->stopped_ = true;
            },
    };
    ext_image_copy_capture_session_v1_add_listener(session, &listener, this);
}

void ImageCopyCaptureSession::mark_dirty()
{
    if (dirty_)
        return;
    dirty_ = true;
    // Clear previous session info
    buffer_size_.reset();
    shm_formats_.clear();
}

class ImageCopyCaptureCursorSession
{
public:
    explicit ImageCopyCaptureCursorSession(ext_image_copy_capture_cursor_session_v1* session);
    ImageCopyCaptureCursorSession(ImageCopyCaptureCursorSession const&) = delete;
    ImageCopyCaptureCursorSession& operator=(ImageCopyCaptureCursorSession const&) = delete;

    auto position() const { return position_; }
    auto hotspot() const { return hotspot_; }

    operator ext_image_copy_capture_cursor_session_v1*() const { return session; }

private:
    wlcs::WlHandle<ext_image_copy_capture_cursor_session_v1> const session;

    bool in_source_ = false;
    wlcs::Point position_;
    wlcs::Point hotspot_;
};

ImageCopyCaptureCursorSession::ImageCopyCaptureCursorSession(ext_image_copy_capture_cursor_session_v1* session)
    : session{session}
{
    static ext_image_copy_capture_cursor_session_v1_listener const listener = {
        .enter = [](
            void* data,
            ext_image_copy_capture_cursor_session_v1*)
            {
                auto self = static_cast<ImageCopyCaptureCursorSession*>(data);
                self->in_source_ = true;
            },
        .leave = [](
            void* data,
            ext_image_copy_capture_cursor_session_v1*)
            {
                auto self = static_cast<ImageCopyCaptureCursorSession*>(data);
                self->in_source_ = false;
            },
        .position = [](
            void* data,
            ext_image_copy_capture_cursor_session_v1*,
            int32_t x,
            int32_t y)
            {
                auto self = static_cast<ImageCopyCaptureCursorSession*>(data);
                self->position_ = {x, y};
            },
        .hotspot = [](
            void* data,
            ext_image_copy_capture_cursor_session_v1*,
            int32_t x,
            int32_t y)
            {
                auto self = static_cast<ImageCopyCaptureCursorSession*>(data);
                self->hotspot_ = {x, y};
            },
    };
    ext_image_copy_capture_cursor_session_v1_add_listener(session, &listener, this);
}

class ExtImageCopyCaptureTest
    : public wlcs::StartedInProcessServer
{
public:
    ExtImageCopyCaptureTest()
        : client{the_server()}
    {
    }

    wlcs::Client client;
};

TEST_F(ExtImageCopyCaptureTest, can_create_source_from_output)
{
    auto manager = client.bind_if_supported<ext_output_image_capture_source_manager_v1>(wlcs::AnyVersion);
    auto output = client.output_state(0);
    auto source = wlcs::wrap_wl_object(ext_output_image_capture_source_manager_v1_create_source(manager, output.output));
    client.roundtrip();
}

TEST_F(ExtImageCopyCaptureTest, can_create_session_from_output)
{
    auto source_manager = client.bind_if_supported<ext_output_image_capture_source_manager_v1>(wlcs::AnyVersion);
    auto capture_manager = client.bind_if_supported<ext_image_copy_capture_manager_v1>(wlcs::AnyVersion);
    auto output = client.output_state(0);
    auto source = wlcs::wrap_wl_object(ext_output_image_capture_source_manager_v1_create_source(source_manager, output.output));
    ImageCopyCaptureSession session{ext_image_copy_capture_manager_v1_create_session(capture_manager, source, 0)};
    client.roundtrip();

    ASSERT_THAT(session.is_dirty(), IsFalse());
    EXPECT_THAT(session.buffer_size(), Ne(std::nullopt));
}

TEST_F(ExtImageCopyCaptureTest, duplicate_frames_fails)
{
    auto source_manager = client.bind_if_supported<ext_output_image_capture_source_manager_v1>(wlcs::AnyVersion);
    auto capture_manager = client.bind_if_supported<ext_image_copy_capture_manager_v1>(wlcs::AnyVersion);
    auto output = client.output_state(0);
    auto source = wlcs::wrap_wl_object(ext_output_image_capture_source_manager_v1_create_source(source_manager, output.output));
    ImageCopyCaptureSession session{ext_image_copy_capture_manager_v1_create_session(capture_manager, source, 0)};

    ImageCopyCaptureFrame frame1{ext_image_copy_capture_session_v1_create_frame(session)};
    ImageCopyCaptureFrame frame2{ext_image_copy_capture_session_v1_create_frame(session)};
    try
    {
        client.roundtrip();
    }
    catch (wlcs::ProtocolError const& err)
    {
        ASSERT_THAT(err.interface(), Eq(&ext_image_copy_capture_session_v1_interface));
        ASSERT_THAT(err.error_code(), Eq(EXT_IMAGE_COPY_CAPTURE_SESSION_V1_ERROR_DUPLICATE_FRAME));
        return;
    }
    FAIL() << "Protocol error not raised";
}

TEST_F(ExtImageCopyCaptureTest, capture_with_no_buffer_fails)
{
    auto source_manager = client.bind_if_supported<ext_output_image_capture_source_manager_v1>(wlcs::AnyVersion);
    auto capture_manager = client.bind_if_supported<ext_image_copy_capture_manager_v1>(wlcs::AnyVersion);
    auto output = client.output_state(0);
    auto source = wlcs::wrap_wl_object(ext_output_image_capture_source_manager_v1_create_source(source_manager, output.output));
    ImageCopyCaptureSession session{ext_image_copy_capture_manager_v1_create_session(capture_manager, source, 0)};

    ImageCopyCaptureFrame frame{ext_image_copy_capture_session_v1_create_frame(session)};
    ext_image_copy_capture_frame_v1_capture(frame);
    try
    {
        client.roundtrip();
    }
    catch (wlcs::ProtocolError const& err)
    {
        ASSERT_THAT(err.interface(), Eq(&ext_image_copy_capture_frame_v1_interface));
        ASSERT_THAT(err.error_code(), Eq(EXT_IMAGE_COPY_CAPTURE_FRAME_V1_ERROR_NO_BUFFER));
        return;
    }
    FAIL() << "Protocol error not raised";
}

TEST_F(ExtImageCopyCaptureTest, capture_with_wrong_size_fails)
{
    auto source_manager = client.bind_if_supported<ext_output_image_capture_source_manager_v1>(wlcs::AnyVersion);
    auto capture_manager = client.bind_if_supported<ext_image_copy_capture_manager_v1>(wlcs::AnyVersion);
    auto output = client.output_state(0);
    auto source = wlcs::wrap_wl_object(ext_output_image_capture_source_manager_v1_create_source(source_manager, output.output));
    ImageCopyCaptureSession session{ext_image_copy_capture_manager_v1_create_session(capture_manager, source, 0)};
    client.roundtrip();

    ASSERT_THAT(session.is_dirty(), IsFalse());
    ASSERT_THAT(session.buffer_size(), Ne(std::nullopt));
    auto buffer_size = session.buffer_size().value();

    ImageCopyCaptureFrame frame{ext_image_copy_capture_session_v1_create_frame(session)};
    wlcs::ShmBuffer shm_buffer{
        client,
        buffer_size.width.as_int() + 10,
        buffer_size.height.as_int() + 10,
    };
    ext_image_copy_capture_frame_v1_attach_buffer(frame, shm_buffer);
    ext_image_copy_capture_frame_v1_capture(frame);
    client.roundtrip();

    EXPECT_THAT(frame.is_ready(), IsFalse());
    EXPECT_THAT(frame.failure_reason(), Optional(EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_BUFFER_CONSTRAINTS));
}


TEST_F(ExtImageCopyCaptureTest, capture_succeeds)
{
    auto source_manager = client.bind_if_supported<ext_output_image_capture_source_manager_v1>(wlcs::AnyVersion);
    auto capture_manager = client.bind_if_supported<ext_image_copy_capture_manager_v1>(wlcs::AnyVersion);
    auto output = client.output_state(0);
    auto source = wlcs::wrap_wl_object(ext_output_image_capture_source_manager_v1_create_source(source_manager, output.output));
    ImageCopyCaptureSession session{ext_image_copy_capture_manager_v1_create_session(capture_manager, source, 0)};
    client.roundtrip();

    ASSERT_THAT(session.is_dirty(), IsFalse());
    ASSERT_THAT(session.buffer_size(), Ne(std::nullopt));
    auto buffer_size = session.buffer_size().value();
    // TODO: compositor could report different formats
    ASSERT_THAT(session.shm_formats(), Contains(Eq(WL_SHM_FORMAT_ARGB8888)));

    wlcs::ShmBuffer shm_buffer{
        client,
        buffer_size.width.as_int(),
        buffer_size.height.as_int(),
    };
    auto data = shm_buffer.data();
    memset(data.data(), 0, data.size());

    ImageCopyCaptureFrame frame{ext_image_copy_capture_session_v1_create_frame(session)};
    ext_image_copy_capture_frame_v1_attach_buffer(frame, shm_buffer);
    ext_image_copy_capture_frame_v1_capture(frame);
    client.dispatch_until([&frame]() { return frame.is_ready() || frame.failure_reason() != std::nullopt; });

    ASSERT_THAT(frame.is_ready(), IsTrue());
    // At least one byte of the buffer has been changed from the
    // zeroed out initial state. As the format includes an alpha
    // channel and we expect the output to be opaque, this should be
    // true.
    EXPECT_THAT(data, Contains(Ne(std::byte{0})));

    // First frame should damage the entire buffer
    EXPECT_THAT(frame.damage(), ElementsAre(wlcs::Rectangle{{0, 0}, buffer_size}));
}

TEST_F(ExtImageCopyCaptureTest, no_second_capture_without_damage)
{
    auto source_manager = client.bind_if_supported<ext_output_image_capture_source_manager_v1>(wlcs::AnyVersion);
    auto capture_manager = client.bind_if_supported<ext_image_copy_capture_manager_v1>(wlcs::AnyVersion);
    auto output = client.output_state(0);
    auto source = wlcs::wrap_wl_object(ext_output_image_capture_source_manager_v1_create_source(source_manager, output.output));
    ImageCopyCaptureSession session{ext_image_copy_capture_manager_v1_create_session(capture_manager, source, 0)};
    client.roundtrip();

    ASSERT_THAT(session.is_dirty(), IsFalse());
    ASSERT_THAT(session.buffer_size(), Ne(std::nullopt));
    auto buffer_size = session.buffer_size().value();
    // TODO: compositor could report different formats
    ASSERT_THAT(session.shm_formats(), Contains(Eq(WL_SHM_FORMAT_ARGB8888)));

    wlcs::ShmBuffer shm_buffer{
        client,
        buffer_size.width.as_int(),
        buffer_size.height.as_int(),
    };

    // Capture a first frame, and destroy it
    {
        ImageCopyCaptureFrame frame{ext_image_copy_capture_session_v1_create_frame(session)};
        ext_image_copy_capture_frame_v1_attach_buffer(frame, shm_buffer);
        ext_image_copy_capture_frame_v1_capture(frame);
        client.dispatch_until([&frame]() { return frame.is_ready() || frame.failure_reason() != std::nullopt; });
        ASSERT_THAT(frame.is_ready(), IsTrue());
    }

    // Try to capture a second frame, which should not complete as
    // there is no damage.
    ImageCopyCaptureFrame frame{ext_image_copy_capture_session_v1_create_frame(session)};
    ext_image_copy_capture_frame_v1_attach_buffer(frame, shm_buffer);
    ext_image_copy_capture_frame_v1_capture(frame);
    try
    {
        client.dispatch_until([&frame]() { return frame.is_ready() || frame.failure_reason() != std::nullopt; }, wlcs::helpers::a_short_time());
    } catch (wlcs::Timeout const& err) {
        return;
    }
    FAIL() << "Frame captured without new damage";
}

TEST_F(ExtImageCopyCaptureTest, second_capture_after_damage)
{
    wlcs::Surface surface{client.create_visible_surface(200, 200)};

    auto source_manager = client.bind_if_supported<ext_output_image_capture_source_manager_v1>(wlcs::AnyVersion);
    auto capture_manager = client.bind_if_supported<ext_image_copy_capture_manager_v1>(wlcs::AnyVersion);
    auto output = client.output_state(0);
    auto source = wlcs::wrap_wl_object(ext_output_image_capture_source_manager_v1_create_source(source_manager, output.output));
    ImageCopyCaptureSession session{ext_image_copy_capture_manager_v1_create_session(capture_manager, source, 0)};
    client.roundtrip();

    ASSERT_THAT(session.is_dirty(), IsFalse());
    ASSERT_THAT(session.buffer_size(), Ne(std::nullopt));
    auto buffer_size = session.buffer_size().value();
    // TODO: compositor could report different formats
    ASSERT_THAT(session.shm_formats(), Contains(Eq(WL_SHM_FORMAT_ARGB8888)));

    wlcs::ShmBuffer shm_buffer{
        client,
        buffer_size.width.as_int(),
        buffer_size.height.as_int(),
    };

    {
        ImageCopyCaptureFrame frame{ext_image_copy_capture_session_v1_create_frame(session)};
        ext_image_copy_capture_frame_v1_attach_buffer(frame, shm_buffer);
        ext_image_copy_capture_frame_v1_capture(frame);
        client.dispatch_until([&frame]() { return frame.is_ready() || frame.failure_reason() != std::nullopt; });
        ASSERT_THAT(frame.is_ready(), IsTrue());
    }

    // Start a second frame capture, then create some damage
    {
        ImageCopyCaptureFrame frame{ext_image_copy_capture_session_v1_create_frame(session)};
        ext_image_copy_capture_frame_v1_attach_buffer(frame, shm_buffer);
        ext_image_copy_capture_frame_v1_capture(frame);

        surface.attach_buffer(200, 200);
        wl_surface_commit(surface);

        client.dispatch_until([&frame]() { return frame.is_ready() || frame.failure_reason() != std::nullopt; });
        ASSERT_THAT(frame.is_ready(), IsTrue());
    }
}

TEST_F(ExtImageCopyCaptureTest, cursor_session_sends_pointer)
{
    auto source_manager = client.bind_if_supported<ext_output_image_capture_source_manager_v1>(wlcs::AnyVersion);
    auto capture_manager = client.bind_if_supported<ext_image_copy_capture_manager_v1>(wlcs::AnyVersion);
    auto output = client.output_state(0);
    auto source = wlcs::wrap_wl_object(ext_output_image_capture_source_manager_v1_create_source(source_manager, output.output));
    ImageCopyCaptureCursorSession session{ext_image_copy_capture_manager_v1_create_pointer_cursor_session(capture_manager, source, client.the_pointer())};

    auto pointer = the_server().create_pointer();
    for (int i = 1; i < 10; i++)
    {
        wlcs::Point point{10 * i, 20 * i};
        pointer.move_to(point.x.as_int(), point.y.as_int());
        client.dispatch_until([&]() { return session.position() == point; });
    }
}

TEST_F(ExtImageCopyCaptureTest, cursor_session_captures_pointer_image)
{
    // Create a surface and place the cursor over it
    wlcs::Surface surface{client.create_visible_surface(200, 200)};
    the_server().move_surface_to(surface, 0, 0);
    std::optional<uint32_t> enter_serial;
    client.add_pointer_enter_notification([&](auto, auto, auto)
        {
            enter_serial = client.latest_serial();
            return false;
        });
    auto pointer = the_server().create_pointer();
    pointer.move_to(100, 100);
    client.dispatch_until([&]() { return enter_serial.has_value(); });

    // Set a cursor image
    wlcs::Surface cursor_surface{client};
    wlcs::ShmBuffer cursor_buffer1{client, 32, 32};
    auto data = cursor_buffer1.data();
    memset(data.data(), 0xff, data.size());
    wl_surface_attach(cursor_surface, cursor_buffer1, 0, 0);
    wl_surface_commit(cursor_surface);
    wl_pointer_set_cursor(client.the_pointer(), enter_serial.value(), cursor_surface, 16, 16);
    client.roundtrip();

    // Create a cursor session for the output
    auto source_manager = client.bind_if_supported<ext_output_image_capture_source_manager_v1>(wlcs::AnyVersion);
    auto capture_manager = client.bind_if_supported<ext_image_copy_capture_manager_v1>(wlcs::AnyVersion);
    auto output = client.output_state(0);
    auto source = wlcs::wrap_wl_object(ext_output_image_capture_source_manager_v1_create_source(source_manager, output.output));
    ImageCopyCaptureCursorSession cursor_session{ext_image_copy_capture_manager_v1_create_pointer_cursor_session(capture_manager, source, client.the_pointer())};
    ImageCopyCaptureSession capture_session{ext_image_copy_capture_cursor_session_v1_get_capture_session(cursor_session)};
    client.roundtrip();

    // Attach one cursor image
    ASSERT_THAT(capture_session.buffer_size(), Optional(wlcs::Size{32, 32}));
    {
        ImageCopyCaptureFrame frame{ext_image_copy_capture_session_v1_create_frame(capture_session)};
        wlcs::ShmBuffer capture_buffer{client, 32, 32};
        ext_image_copy_capture_frame_v1_attach_buffer(frame, capture_buffer);
        ext_image_copy_capture_frame_v1_capture(frame);
        client.dispatch_until([&frame]() { return frame.is_ready() || frame.failure_reason() != std::nullopt; });
        ASSERT_THAT(frame.is_ready(), IsTrue());
        EXPECT_THAT(capture_buffer.data(), ElementsAreArray(cursor_buffer1.data()));
        EXPECT_THAT(cursor_session.hotspot(), Eq(wlcs::Point{16, 16}));
    }

    // Change the cursor
    wlcs::ShmBuffer cursor_buffer2{client, 32, 32};
    data = cursor_buffer2.data();
    memset(data.data(), 0x7f, data.size());
    wl_surface_attach(cursor_surface, cursor_buffer2, 0, 0);
    wl_surface_commit(cursor_surface);

    // Capture a new frame
    {
        ImageCopyCaptureFrame frame{ext_image_copy_capture_session_v1_create_frame(capture_session)};
        wlcs::ShmBuffer capture_buffer{client, 32, 32};
        ext_image_copy_capture_frame_v1_attach_buffer(frame, capture_buffer);
        ext_image_copy_capture_frame_v1_capture(frame);
        client.dispatch_until([&frame]() { return frame.is_ready() || frame.failure_reason() != std::nullopt; });
        ASSERT_THAT(frame.is_ready(), IsTrue());
        EXPECT_THAT(capture_buffer.data(), ElementsAreArray(cursor_buffer2.data()));
    }
}

}
