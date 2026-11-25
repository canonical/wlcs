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

#ifndef WLCS_EXT_IMAGE_COPY_CAPTURE_V1_H
#define WLCS_EXT_IMAGE_COPY_CAPTURE_V1_H

#include "generated/ext-image-capture-source-v1-client.h"
#include "generated/ext-image-copy-capture-v1-client.h"
#include "geometry/rectangle.h"
#include "geometry/size.h"
#include "wl_handle.h"
#include "wl_interface_descriptor.h"

#include <cstdint>
#include <optional>
#include <vector>

#include <wayland-client-protocol.h>

namespace wlcs
{
WLCS_CREATE_INTERFACE_DESCRIPTOR(ext_foreign_toplevel_image_capture_source_manager_v1)
WLCS_CREATE_INTERFACE_DESCRIPTOR(ext_output_image_capture_source_manager_v1)
WLCS_CREATE_INTERFACE_DESCRIPTOR(ext_image_capture_source_v1)
WLCS_CREATE_INTERFACE_DESCRIPTOR(ext_image_copy_capture_manager_v1)
WLCS_CREATE_INTERFACE_DESCRIPTOR(ext_image_copy_capture_session_v1)
WLCS_CREATE_INTERFACE_DESCRIPTOR(ext_image_copy_capture_cursor_session_v1)
WLCS_CREATE_INTERFACE_DESCRIPTOR(ext_image_copy_capture_frame_v1)

class ImageCopyCaptureFrame
{
public:
    explicit ImageCopyCaptureFrame(ext_image_copy_capture_frame_v1* frame);
    ImageCopyCaptureFrame(ImageCopyCaptureFrame const&) = delete;
    ImageCopyCaptureFrame& operator=(ImageCopyCaptureFrame const&) = delete;

    auto is_ready() const { return ready_; }
    auto failure_reason() const { return failure_reason_; }
    auto damage() const { return damage_; }
    auto transform() const -> uint32_t { return transform_.value_or(WL_OUTPUT_TRANSFORM_NORMAL); }

    operator ext_image_copy_capture_frame_v1*() const { return frame; }

private:
    WlHandle<ext_image_copy_capture_frame_v1> const frame;

    std::optional<uint32_t> transform_;
    std::vector<Rectangle> damage_;
    std::optional<uint32_t> presentation_time_;
    bool ready_ = false;
    std::optional<uint32_t> failure_reason_;
};

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
    WlHandle<ext_image_copy_capture_session_v1> const session;

    bool dirty_ = false;
    bool stopped_ = false;
    std::optional<Size> buffer_size_;
    std::vector<uint32_t> shm_formats_;

    void mark_dirty();
};
}

#endif /* WLCS_EXT_IMAGE_COPY_CAPTURE_V1_H */
