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

#include "ext_image_copy_capture_v1.h"

wlcs::ImageCopyCaptureFrame::ImageCopyCaptureFrame(ext_image_copy_capture_frame_v1* frame)
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

wlcs::ImageCopyCaptureSession::ImageCopyCaptureSession(ext_image_copy_capture_session_v1* session)
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

void wlcs::ImageCopyCaptureSession::mark_dirty()
{
    if (dirty_)
        return;
    dirty_ = true;
    // Clear previous session info
    buffer_size_.reset();
    shm_formats_.clear();
}
