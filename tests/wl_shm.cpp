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
#include "helpers.h"
#include "version_specifier.h"
#include "expect_protocol_error.h"

#include <gmock/gmock.h>

#include <boost/throw_exception.hpp>

#include <sys/mman.h>
#include <unistd.h>
#include <system_error>
#include <vector>

using namespace testing;

namespace
{
struct ShmTest : wlcs::StartedInProcessServer
{
    wlcs::Client client{the_server()};
};

// A separate fixture, identical to ShmTest, used to spin up a second server
// instance (see truncated_shm_file_is_an_error_on_a_second_server_instance).
struct SecondShmTest : wlcs::StartedInProcessServer
{
    wlcs::Client client{the_server()};
};

/// Binds a fresh wl_shm and collects the format events the server sends on bind.
std::vector<uint32_t> collect_advertised_formats(wlcs::Client& client)
{
    static std::vector<uint32_t> formats;
    formats.clear();

    static wl_shm_listener const listener{
        [](void*, wl_shm*, uint32_t format)
        {
            formats.push_back(format);
        }};

    auto shm = static_cast<wl_shm*>(
        client.bind_if_supported(wl_shm_interface, wlcs::AnyVersion));
    wl_shm_add_listener(shm, &listener, nullptr);

    // Format events are delivered in response to the bind request.
    client.roundtrip();

    wl_shm_destroy(shm);
    return formats;
}

// Creates a wl_buffer whose backing file is then truncated to a tiny size, so
// that the compositor will access it out-of-bounds (and hit SIGBUS) if it does
// not validate the buffer. Used to check the server handles this gracefully.
struct wl_buffer* create_bad_shm_buffer(wlcs::Client& client, int width, int height)
{
    struct wl_shm* shm = client.shm();
    int stride = width * 4;
    int size = stride * height;

    int fd = wlcs::helpers::create_anonymous_file(size);

    auto pool = wl_shm_create_pool(shm, fd, size);
    auto buffer = wl_shm_pool_create_buffer(
        pool,
        0,
        width,
        height,
        stride,
        WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);

    // Truncate the file to a small size, so that the compositor will access it
    // out-of-bounds, and hit SIGBUS.
    if (ftruncate(fd, 12) == -1)
    {
        close(fd);
        BOOST_THROW_EXCEPTION(
            std::system_error(errno, std::system_category(), "Failed to truncate temporary file"));
    }
    close(fd);

    return buffer;
}
}

// Creating a pool from a file descriptor that cannot be mapped must be rejected.
// A pipe is not mappable, so it serves as a conveniently-invalid fd.
TEST_F(ShmTest, create_pool_with_invalid_fd_is_an_error)
{
    int pipe_fds[2];
    ASSERT_THAT(pipe(pipe_fds), Eq(0));

    EXPECT_PROTOCOL_ERROR(
        {
            auto pool = wl_shm_create_pool(client.shm(), pipe_fds[0], 4);
            wl_shm_pool_destroy(pool);
            client.roundtrip();
        },
        &wl_shm_interface,
        WL_SHM_ERROR_INVALID_FD);

    close(pipe_fds[0]);
    close(pipe_fds[1]);
}

// Creating a pool with a non-positive size must be rejected.
TEST_F(ShmTest, create_pool_with_negative_size_is_an_error)
{
    auto fd = wlcs::helpers::create_anonymous_file(64);

    EXPECT_PROTOCOL_ERROR(
        {
            auto pool = wl_shm_create_pool(client.shm(), fd, -1);
            wl_shm_pool_destroy(pool);
            client.roundtrip();
        },
        &wl_shm_interface,
        WL_SHM_ERROR_INVALID_STRIDE);

    close(fd);
}

// The wl_shm protocol mandates that ARGB8888 and XRGB8888 are always supported.
TEST_F(ShmTest, advertises_mandatory_formats)
{
    auto const formats = collect_advertised_formats(client);

    EXPECT_THAT(formats, Contains(Eq(WL_SHM_FORMAT_ARGB8888)));
    EXPECT_THAT(formats, Contains(Eq(WL_SHM_FORMAT_XRGB8888)));
}

// Requesting a buffer with a format the server never advertised must be rejected.
TEST_F(ShmTest, create_buffer_with_unknown_format_is_an_error)
{
    int const width = 4;
    int const height = 4;
    int const stride = width * 4;
    int const size = stride * height;
    auto const bogus_format = 0xDEADBEEF;

    auto const fd = wlcs::helpers::create_anonymous_file(size);
    auto const pool = wl_shm_create_pool(client.shm(), fd, size);
    close(fd);

    EXPECT_PROTOCOL_ERROR(
        {
            wl_shm_pool_create_buffer(
                pool, 0, width, height, stride, bogus_format);
            client.roundtrip();
        },
        &wl_shm_pool_interface,
        WL_SHM_ERROR_INVALID_FORMAT);

    wl_shm_pool_destroy(pool);
}

// A buffer that extends beyond the bounds of its pool must be rejected.
TEST_F(ShmTest, create_buffer_exceeding_pool_size_is_an_error)
{
    int const width = 4, height = 4;
    int const stride = width * 4;
    int const size = stride * height;

    auto fd = wlcs::helpers::create_anonymous_file(size);
    auto pool = wl_shm_create_pool(client.shm(), fd, size);
    close(fd);

    EXPECT_PROTOCOL_ERROR(
        {
            // Offsetting by a full row pushes the buffer past the end of the pool.
            int const offset = stride;
            wl_shm_pool_create_buffer(
                pool, offset, width, height, stride, WL_SHM_FORMAT_ARGB8888);
            client.roundtrip();
        },
        &wl_shm_pool_interface,
        WL_SHM_ERROR_INVALID_STRIDE);

    wl_shm_pool_destroy(pool);
}

// Growing a pool must allow buffers to be created in the newly-available space.
TEST_F(ShmTest, pool_can_be_resized_larger)
{
    int const width = 4, height = 4;
    int const stride = width * 4;
    int const buffer_size = stride * height;

    auto fd = wlcs::helpers::create_anonymous_file(buffer_size * 2);
    auto pool = wl_shm_create_pool(client.shm(), fd, buffer_size);
    close(fd);

    auto first = wl_shm_pool_create_buffer(
        pool, 0, width, height, stride, WL_SHM_FORMAT_ARGB8888);

    wl_shm_pool_resize(pool, buffer_size * 2);

    auto second = wl_shm_pool_create_buffer(
        pool, buffer_size, width, height, stride, WL_SHM_FORMAT_ARGB8888);

    // No protocol error should be raised for a valid resize and second buffer.
    client.roundtrip();

    wl_buffer_destroy(first);
    wl_buffer_destroy(second);
    wl_shm_pool_destroy(pool);
}

// A single pool may back several independent buffers simultaneously.
TEST_F(ShmTest, multiple_buffers_can_share_a_pool)
{
    int const width = 4, height = 4;
    int const stride = width * 4;
    int const buffer_size = stride * height;
    int const buffer_count = 3;
    int const pool_size = buffer_size * buffer_count;

    auto fd = wlcs::helpers::create_anonymous_file(pool_size);
    auto pool = wl_shm_create_pool(client.shm(), fd, pool_size);
    close(fd);

    std::vector<wl_buffer*> buffers;
    for (int i = 0; i < buffer_count; ++i)
    {
        buffers.push_back(wl_shm_pool_create_buffer(
            pool,
            i * buffer_size,
            width, height,
            stride,
            WL_SHM_FORMAT_ARGB8888));
    }

    // Destroying the pool must not invalidate the buffers created from it.
    wl_shm_pool_destroy(pool);

    client.roundtrip();

    for (auto buffer : buffers)
        wl_buffer_destroy(buffer);
}

// Attaching a buffer whose backing file has been truncated must not crash the
// compositor; it should raise a protocol error instead.
TEST_F(ShmTest, truncated_shm_file_is_an_error)
{
    auto surface = client.create_visible_surface(200, 200);

    wl_buffer* bad_buffer = create_bad_shm_buffer(client, 200, 200);

    wl_surface_attach(surface, bad_buffer, 0, 0);
    wl_surface_damage(surface, 0, 0, 200, 200);
    wl_surface_commit(surface);

    // We dispatch until we receive the protocol error, or hit the timeout.
    EXPECT_PROTOCOL_ERROR(
        {
            client.dispatch_until([]() { return false; });
        },
        &wl_buffer_interface,
        WL_SHM_ERROR_INVALID_FD);

    wl_buffer_destroy(bad_buffer);
}

// This should behave identically to truncated_shm_file_is_an_error, but runs in
// a second server instance. There have been issues with the server installing a
// SIGBUS handler (via wl_shm_buffer_begin_access()) that only works for the
// first server instance.
TEST_F(SecondShmTest, truncated_shm_file_is_an_error_on_a_second_server_instance)
{
    auto surface = client.create_visible_surface(200, 200);

    wl_buffer* bad_buffer = create_bad_shm_buffer(client, 200, 200);

    wl_surface_attach(surface, bad_buffer, 0, 0);
    wl_surface_damage(surface, 0, 0, 200, 200);
    wl_surface_commit(surface);

    EXPECT_PROTOCOL_ERROR(
        {
            client.dispatch_until([]() { return false; });
        },
        &wl_buffer_interface,
        WL_SHM_ERROR_INVALID_FD);

    wl_buffer_destroy(bad_buffer);
}

// Creating a buffer with a stride too small for its declared width must be
// rejected.
TEST_F(ShmTest, client_lies_about_buffer_size)
{
    auto const width = 200, height = 200;
    auto const incorrect_stride = width;

    auto fd = wlcs::helpers::create_anonymous_file(height * incorrect_stride);
    auto shm_pool = wl_shm_create_pool(client.shm(), fd, height * incorrect_stride);
    close(fd);

    auto bad_buffer = wl_shm_pool_create_buffer(
        shm_pool,
        0,
        width, height,
        incorrect_stride, // Stride is in bytes, not pixels, so this is ¼ the correct value.
        WL_SHM_FORMAT_ARGB8888);

    EXPECT_PROTOCOL_ERROR(
        {
            // Buffer creation should fail, so all we need is for the
            // create_buffer call to be processed.
            client.roundtrip();
        },
        &wl_shm_pool_interface,
        WL_SHM_ERROR_INVALID_STRIDE);

    wl_buffer_destroy(bad_buffer);
    wl_shm_pool_destroy(shm_pool);
}
