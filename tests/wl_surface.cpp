/*
 * Copyright © 2026 Canonical Ltd.
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

#include "in_process_server.h"
#include "expect_protocol_error.h"
#include "version_specifier.h"

#include <gmock/gmock.h>

#include <memory>

using namespace testing;
using namespace wlcs;

// These tests exercise the wl_surface buffer-state requests (set_buffer_scale,
// set_buffer_transform, damage_buffer and offset).
//
// FUTURE IMPROVEMENT: most of the positive tests below only confirm that the
// request is accepted and that the surface is subsequently presented (a frame
// callback fires). They do *not* yet confirm that the compositor actually
// applied the requested state. To do that we would need to observe the
// resulting output, for example by:
//   - capturing the composited result (e.g. via ext_image_copy_capture_v1) and
//     checking that pixels were scaled / rotated / offset / damaged as expected,
//     and/or
//   - probing the surface's logical size via pointer hit-testing
//     (window_under_cursor()/pointer_position(), as wp_viewporter.cpp does) to
//     confirm that scale and 90/270 transforms change the surface dimensions.
// An earlier attempt at the latter found that the reference compositor did not
// report the spec-expected swapped dimensions for buffer transforms, so that
// behaviour should be investigated before such assertions are added.

namespace
{
struct SurfaceTest : StartedInProcessServer
{
    Client client{the_server()};

    // Commit `surface` and block until the compositor has presented the frame.
    void commit_and_wait(Surface& surface)
    {
        auto const presented = std::make_shared<bool>(false);
        surface.add_frame_callback([presented](auto) { *presented = true; });
        wl_surface_commit(surface);
        client.dispatch_until([presented]() { return *presented; });
    }
};
}

TEST_F(SurfaceTest, attach)
{
    // Attaching and committing a buffer to a surface should result in it being
    // presented (a frame callback fires).
    auto surface = client.create_visible_surface(100, 100);
    ShmBuffer buffer{client, 100, 100};

    wl_surface_attach(surface, buffer, 0, 0);
    commit_and_wait(surface);
}

TEST_F(SurfaceTest, buffer_scale)
{
    client.bind_if_supported<wl_compositor>(AtLeastVersion{WL_SURFACE_SET_BUFFER_SCALE_SINCE_VERSION});

    // FUTURE IMPROVEMENT: confirm the 200x200 buffer is presented at a 100x100
    // logical size, and that its contents are downscaled accordingly.
    auto surface = client.create_visible_surface(200, 200);
    ShmBuffer buffer{client, 200, 200};

    wl_surface_set_buffer_scale(surface, 2);
    wl_surface_attach(surface, buffer, 0, 0);
    commit_and_wait(surface);
}

TEST_F(SurfaceTest, buffer_transform_90)
{
    client.bind_if_supported<wl_compositor>(AtLeastVersion{WL_SURFACE_SET_BUFFER_TRANSFORM_SINCE_VERSION});

    // FUTURE IMPROVEMENT: confirm the buffer is actually rotated 90° - e.g. that
    // the surface's logical dimensions are swapped (200x100 -> 100x200) and that
    // a known pixel ends up in the rotated position.
    auto surface = client.create_visible_surface(200, 100);
    ShmBuffer buffer{client, 200, 100};

    wl_surface_set_buffer_transform(surface, WL_OUTPUT_TRANSFORM_90);
    wl_surface_attach(surface, buffer, 0, 0);
    commit_and_wait(surface);
}

TEST_F(SurfaceTest, buffer_transform_180)
{
    client.bind_if_supported<wl_compositor>(AtLeastVersion{WL_SURFACE_SET_BUFFER_TRANSFORM_SINCE_VERSION});

    // FUTURE IMPROVEMENT: confirm the buffer is actually rotated 180° - the
    // logical size is unchanged, but a known pixel should end up diagonally
    // opposite its original position.
    auto surface = client.create_visible_surface(200, 100);
    ShmBuffer buffer{client, 200, 100};

    wl_surface_set_buffer_transform(surface, WL_OUTPUT_TRANSFORM_180);
    wl_surface_attach(surface, buffer, 0, 0);
    commit_and_wait(surface);
}

TEST_F(SurfaceTest, buffer_transform_270)
{
    client.bind_if_supported<wl_compositor>(AtLeastVersion{WL_SURFACE_SET_BUFFER_TRANSFORM_SINCE_VERSION});

    // FUTURE IMPROVEMENT: confirm the buffer is actually rotated 270° - e.g. that
    // the surface's logical dimensions are swapped (200x100 -> 100x200) and that
    // a known pixel ends up in the rotated position.
    auto surface = client.create_visible_surface(200, 100);
    ShmBuffer buffer{client, 200, 100};

    wl_surface_set_buffer_transform(surface, WL_OUTPUT_TRANSFORM_270);
    wl_surface_attach(surface, buffer, 0, 0);
    commit_and_wait(surface);
}

TEST_F(SurfaceTest, damage_buffer)
{
    client.bind_if_supported<wl_compositor>(AtLeastVersion{WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION});

    // FUTURE IMPROVEMENT: attach a second buffer with different contents, damage
    // only part of it with damage_buffer, and confirm via capture that the
    // damaged region (and only that region) was updated on screen.
    auto surface = client.create_visible_surface(100, 100);
    ShmBuffer buffer{client, 100, 100};

    wl_surface_attach(surface, buffer, 0, 0);
    wl_surface_damage_buffer(surface, 0, 0, 100, 100);
    commit_and_wait(surface);
}

TEST_F(SurfaceTest, offset)
{
    client.bind_if_supported<wl_compositor>(AtLeastVersion{WL_SURFACE_OFFSET_SINCE_VERSION});

    // FUTURE IMPROVEMENT: confirm the buffer contents are actually shifted by the
    // requested offset relative to the surface origin (e.g. via capture).
    auto surface = client.create_visible_surface(100, 100);
    ShmBuffer buffer{client, 100, 100};

    wl_surface_attach(surface, buffer, 0, 0);
    wl_surface_offset(surface, 10, 10);
    commit_and_wait(surface);
}

TEST_F(SurfaceTest, set_buffer_scale_with_zero_is_an_error)
{
    client.bind_if_supported<wl_compositor>(AtLeastVersion{WL_SURFACE_SET_BUFFER_SCALE_SINCE_VERSION});

    auto surface = client.create_visible_surface(100, 100);

    EXPECT_PROTOCOL_ERROR({
        wl_surface_set_buffer_scale(surface, 0);
        client.roundtrip();
    }, &wl_surface_interface, WL_SURFACE_ERROR_INVALID_SCALE);
}

TEST_F(SurfaceTest, set_buffer_transform_with_invalid_value_is_an_error)
{
    client.bind_if_supported<wl_compositor>(AtLeastVersion{WL_SURFACE_SET_BUFFER_TRANSFORM_SINCE_VERSION});

    auto surface = client.create_visible_surface(100, 100);

    EXPECT_PROTOCOL_ERROR({
        wl_surface_set_buffer_transform(surface, 42);
        client.roundtrip();
    }, &wl_surface_interface, WL_SURFACE_ERROR_INVALID_TRANSFORM);
}

TEST_F(SurfaceTest, attach_with_non_zero_offset_is_an_error)
{
    client.bind_if_supported<wl_compositor>(AtLeastVersion{WL_SURFACE_OFFSET_SINCE_VERSION});

    auto surface = client.create_visible_surface(100, 100);
    ShmBuffer buffer{client, 100, 100};

    EXPECT_PROTOCOL_ERROR({
        wl_surface_attach(surface, buffer, 10, 10);
        client.roundtrip();
    }, &wl_surface_interface, WL_SURFACE_ERROR_INVALID_OFFSET);
}

TEST_F(SurfaceTest, attach_with_non_zero_offset_is_accepted_before_version_5)
{
    // Before version 5, passing a non-zero offset to wl_surface.attach was the
    // way to offset a buffer (the dedicated wl_surface.offset request did not
    // exist yet), so it must be accepted rather than raising a protocol error.
    auto compositor =
        client.bind_if_supported<wl_compositor>(AtMostVersion{WL_SURFACE_OFFSET_SINCE_VERSION - 1});
    auto surface = wrap_wl_object(wl_compositor_create_surface(compositor));
    ShmBuffer buffer{client, 100, 100};

    wl_surface_attach(surface, buffer, 10, 10);
    wl_surface_commit(surface);

    // If the offset were rejected, this roundtrip would raise a protocol error
    // and fail the test.
    client.roundtrip();
}
