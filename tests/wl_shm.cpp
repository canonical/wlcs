/*
 * Copyright © 2024 Canonical Ltd.
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

#include <sys/mman.h>
#include <unistd.h>
#include <vector>

using namespace testing;

namespace
{
struct ShmTest : wlcs::StartedInProcessServer
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

    auto fd = wlcs::helpers::create_anonymous_file(size);
    auto pool = wl_shm_create_pool(client.shm(), fd, size);
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
            wl_shm_pool_create_buffer(
                pool, stride, width, height, stride, WL_SHM_FORMAT_ARGB8888);
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
