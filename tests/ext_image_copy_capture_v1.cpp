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
WLCS_CREATE_INTERFACE_DESCRIPTOR(ext_image_copy_capture_frame_v1)
} // namespace wlcs

namespace {

class ImageCaptureSource
{
public:
    explicit ImageCaptureSource(ext_image_capture_source_v1* handle);
    ImageCaptureSource(ImageCaptureSource const&) = delete;
    ImageCaptureSource& operator=(ImageCaptureSource const&) = delete;

    operator ext_image_capture_source_v1*() const { return handle; }

private:
    wlcs::WlHandle<ext_image_capture_source_v1> const handle;
};

ImageCaptureSource::ImageCaptureSource(ext_image_capture_source_v1* handle)
    : handle{handle}
{
}

class OutputImageCaptureSourceManager
{
public:
    explicit OutputImageCaptureSourceManager(wlcs::Client& client);
    OutputImageCaptureSourceManager(OutputImageCaptureSourceManager const&) = delete;
    OutputImageCaptureSourceManager& operator=(OutputImageCaptureSourceManager const&) = delete;

    auto create_source(wl_output* output) -> std::unique_ptr<ImageCaptureSource>;

private:
    wlcs::WlHandle<ext_output_image_capture_source_manager_v1> const manager;
};

OutputImageCaptureSourceManager::OutputImageCaptureSourceManager(wlcs::Client& client)
    : manager{client.bind_if_supported<ext_output_image_capture_source_manager_v1>(wlcs::AnyVersion)}
{
}

auto OutputImageCaptureSourceManager::create_source(wl_output* output) -> std::unique_ptr<ImageCaptureSource>
{
    return std::make_unique<ImageCaptureSource>(ext_output_image_capture_source_manager_v1_create_source(manager, output));
}

class ImageCopyCaptureFrame
{
public:
    explicit ImageCopyCaptureFrame(ext_image_copy_capture_frame_v1* frame);
    ImageCopyCaptureFrame(ImageCopyCaptureFrame const&) = delete;
    ImageCopyCaptureFrame& operator=(ImageCopyCaptureFrame const&) = delete;

    void attach_buffer(wl_buffer* buffer);
    void capture();

    auto is_ready() { return ready_; }
    auto failure_reason() { return failure_reason_; }

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

void ImageCopyCaptureFrame::attach_buffer(wl_buffer* buffer)
{
    ext_image_copy_capture_frame_v1_attach_buffer(frame, buffer);
}

void ImageCopyCaptureFrame::capture()
{
    ext_image_copy_capture_frame_v1_capture(frame);
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

    auto create_frame() -> std::unique_ptr<ImageCopyCaptureFrame>;

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

auto ImageCopyCaptureSession::create_frame() -> std::unique_ptr<ImageCopyCaptureFrame>
{
    return std::make_unique<ImageCopyCaptureFrame>(ext_image_copy_capture_session_v1_create_frame(session));
}

class ImageCopyCaptureManager
{
public:
    explicit ImageCopyCaptureManager(wlcs::Client& client);
    ImageCopyCaptureManager(ImageCopyCaptureManager const&) = delete;
    ImageCopyCaptureManager& operator=(ImageCopyCaptureManager const&) = delete;

    auto create_session(ext_image_capture_source_v1* source, uint32_t options) -> std::unique_ptr<ImageCopyCaptureSession>;

private:
    wlcs::WlHandle<ext_image_copy_capture_manager_v1> const manager;
};

ImageCopyCaptureManager::ImageCopyCaptureManager(wlcs::Client& client)
    : manager{client.bind_if_supported<ext_image_copy_capture_manager_v1>(wlcs::AnyVersion)}
{
}

auto ImageCopyCaptureManager::create_session(ext_image_capture_source_v1* source, uint32_t options) -> std::unique_ptr<ImageCopyCaptureSession>
{
    return std::make_unique<ImageCopyCaptureSession>(ext_image_copy_capture_manager_v1_create_session(manager, source, options));
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
    OutputImageCaptureSourceManager manager{client};
    auto output = client.output_state(0);
    auto source = manager.create_source(output.output);
    client.roundtrip();
}

TEST_F(ExtImageCopyCaptureTest, can_create_session_from_output)
{
    OutputImageCaptureSourceManager source_manager{client};
    ImageCopyCaptureManager capture_manager{client};
    auto output = client.output_state(0);
    auto source = source_manager.create_source(output.output);
    auto session = capture_manager.create_session(*source, 0);
    client.roundtrip();

    ASSERT_THAT(session->is_dirty(), IsFalse());
    EXPECT_THAT(session->buffer_size(), Ne(std::nullopt));
}

TEST_F(ExtImageCopyCaptureTest, duplicate_frames_fails)
{
    OutputImageCaptureSourceManager source_manager{client};
    ImageCopyCaptureManager capture_manager{client};
    auto output = client.output_state(0);
    auto source = source_manager.create_source(output.output);
    auto session = capture_manager.create_session(*source, 0);
    auto frame1 = session->create_frame();
    auto frame2 = session->create_frame();
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
    OutputImageCaptureSourceManager source_manager{client};
    ImageCopyCaptureManager capture_manager{client};
    auto output = client.output_state(0);
    auto source = source_manager.create_source(output.output);
    auto session = capture_manager.create_session(*source, 0);
    auto frame = session->create_frame();
    frame->capture();
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
    OutputImageCaptureSourceManager source_manager{client};
    ImageCopyCaptureManager capture_manager{client};
    auto output = client.output_state(0);
    auto source = source_manager.create_source(output.output);
    auto session = capture_manager.create_session(*source, 0);
    client.roundtrip();

    ASSERT_THAT(session->is_dirty(), IsFalse());
    ASSERT_THAT(session->buffer_size(), Ne(std::nullopt));
    auto buffer_size = session->buffer_size().value();

    auto frame = session->create_frame();
    wlcs::ShmBuffer shm_buffer{
        client,
        buffer_size.width.as_int() + 10,
        buffer_size.height.as_int() + 10,
    };
    frame->attach_buffer(shm_buffer);
    frame->capture();
    client.roundtrip();

    EXPECT_THAT(frame->is_ready(), IsFalse());
    EXPECT_THAT(frame->failure_reason(), Optional(EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_BUFFER_CONSTRAINTS));
}


TEST_F(ExtImageCopyCaptureTest, capture_succeeds)
{
    OutputImageCaptureSourceManager source_manager{client};
    ImageCopyCaptureManager capture_manager{client};
    auto output = client.output_state(0);
    auto source = source_manager.create_source(output.output);
    auto session = capture_manager.create_session(*source, 0);
    client.roundtrip();

    ASSERT_THAT(session->is_dirty(), IsFalse());
    ASSERT_THAT(session->buffer_size(), Ne(std::nullopt));
    auto buffer_size = session->buffer_size().value();
    // TODO: compositor could report different formats
    ASSERT_THAT(session->shm_formats(), Contains(Eq(WL_SHM_FORMAT_ARGB8888)));

    auto frame = session->create_frame();
    wlcs::ShmBuffer shm_buffer{
        client,
        buffer_size.width.as_int(),
        buffer_size.height.as_int(),
    };
    frame->attach_buffer(shm_buffer);
    frame->capture();
    client.dispatch_until([&frame]() { return frame->is_ready() || frame->failure_reason() != std::nullopt; });

    ASSERT_THAT(frame->is_ready(), IsTrue());
}

}
