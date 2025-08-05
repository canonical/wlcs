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
#include "generated/linux-drm-syncobj-v1-client.h"
#include "in_process_server.h"
#include "version_specifier.h"
#include "wl_interface_descriptor.h"
#include "gtest_matchers.h"

#include "xf86drm.h"

#include "fcntl.h"
#include <boost/throw_exception.hpp>
#include <chrono>
#include <drm.h>
#include <fcntl.h>
#include <system_error>
#include <type_traits>

namespace wlcs
{
WLCS_CREATE_INTERFACE_DESCRIPTOR(wp_linux_drm_syncobj_manager_v1)
WLCS_CREATE_INTERFACE_DESCRIPTOR(wp_linux_drm_syncobj_surface_v1)
WLCS_CREATE_INTERFACE_DESCRIPTOR(wp_linux_drm_syncobj_timeline_v1)
}

namespace
{
class Fd
{
public:
    Fd(int fd) : fd{fd}
    {
        if (fd < 0)
        {
            BOOST_THROW_EXCEPTION(std::logic_error{"Invalid fd"});
        }
    }
    ~Fd()
    {
        if (fd > 0)
        {
            close(fd);
        }
    }

    Fd(Fd&& from)
        : fd{from.fd}
    {
        from.fd = -1;
    }

    auto dup() const -> Fd
    {
        return Fd{::dup(fd)};
    }

    Fd(Fd const&) = delete;
    auto operator=(Fd const&) -> Fd& = delete;

    operator int() const
    {
        return fd;
    }

private:
    int fd;
};

using DRMDeviceHandle = std::unique_ptr<
    std::remove_pointer_t<drmDevicePtr>, decltype([](drmDevicePtr value) { drmFreeDevice(&value); })>;

auto get_drm_devices() -> std::vector<DRMDeviceHandle>
{
    std::array<drmDevicePtr, 6> devices;
    std::vector<DRMDeviceHandle> result;

    if (auto count = drmGetDevices(devices.data(), devices.size()); count > 0)
    {
        result.reserve(count);
        for (auto i = 0; i < count; ++i)
        {
            result.emplace_back(devices[i]);
        }
    }
    return result;
}

auto open_drm_node(drmDevicePtr device) -> std::optional<Fd>
{
    for (auto type : { DRM_NODE_PRIMARY, DRM_NODE_RENDER })
    {
        if (device->available_nodes & (1 << type))
        {
            if (auto fd = open(device->nodes[type], O_RDWR | O_CLOEXEC); fd >= 0)
            {
                return Fd{fd};
            }
        }
    }
    return std::nullopt;
}

class DRMSyncobjNotSupported : public std::runtime_error
{
public:
    DRMSyncobjNotSupported()
        : std::runtime_error{"DRM_CAP_SYNCOBJ_TIMELINE not supported by any available DRM devices."}
    {
        ::testing::Test::RecordProperty("wlcs-skip-test", "DRM_CAP_SYNCOBJ_TIMELINE not supported by any available DRM devices.");
    }

};

auto open_timeline_capable_drm_node() -> Fd
{
    auto devices = get_drm_devices();

    for (auto const& device : devices)
    {
        if (auto fd = open_drm_node(device.get()))
        {
            uint64_t value{0};
            if (auto err = drmGetCap(*fd, DRM_CAP_SYNCOBJ_TIMELINE, &value))
            {
                std::cerr << "Failed to query DRM cap: " << strerror(-err) << std::endl;
            }
            else if (value >= 1)
            {
                // We've found a DRM device that supports what we need.
                return std::move(*fd);
            }
        }
    }

    BOOST_THROW_EXCEPTION(DRMSyncobjNotSupported{});
}
}

class LinuxDRMSyncobjV1Test : public wlcs::StartedInProcessServer
{
public:
    wlcs::Client a_client{the_server()};
    wlcs::WlHandle<wp_linux_drm_syncobj_manager_v1> syncobj_manager{
      a_client.bind_if_supported<wp_linux_drm_syncobj_manager_v1>(wlcs::AnyVersion)};
};

namespace
{
class Syncobj
{
public:
    Syncobj(Fd drm_fd)
        : fd{std::move(drm_fd)},
          handle{syncobj_create_checked(fd)}
    {
    }

    ~Syncobj()
    {
        drmSyncobjDestroy(fd, handle);
    }

    auto get_fd_handle() const -> Fd
    {
        int timeline_fd{-1};
        if (auto err = drmSyncobjHandleToFD(fd, handle, &timeline_fd))
        {
            std::system_error failure{-err, std::system_category(), "Failed to export DRM Syncobj handle"};
            ::testing::Test::RecordProperty("wlcs-skip-test", failure.what());
            BOOST_THROW_EXCEPTION(failure);
        }
        return Fd{timeline_fd};
    }

    void signal(uint64_t point)
    {
        if (auto err = drmSyncobjTimelineSignal(fd, &handle, &point, 1))
        {
            BOOST_THROW_EXCEPTION((
                std::system_error{
                    -err,
                    std::system_category(),
                    "Failed to signal DRM syncobj"}));
        }
    }

    operator uint32_t() const
    {
        return handle;
    }
private:
    static auto syncobj_create_checked(Fd const& drm_fd) -> uint32_t
    {
        uint32_t timeline_handle;
        if (auto err = drmSyncobjCreate(drm_fd, 0, &timeline_handle))
        {
            std::system_error failure{-err, std::system_category(), "Failed to create DRM Syncobj"};
            ::testing::Test::RecordProperty("wlcs-skip-test", failure.what());
            BOOST_THROW_EXCEPTION(failure);
        }
        return timeline_handle;
    }

    Fd const fd;
    uint32_t const handle;
};
}

TEST_F(LinuxDRMSyncobjV1Test, can_import_timeline)
{
    Syncobj syncobj{open_timeline_capable_drm_node()};
    auto timeline_fd = syncobj.get_fd_handle();

    wlcs::WlHandle<wp_linux_drm_syncobj_timeline_v1> timeline{
        wp_linux_drm_syncobj_manager_v1_import_timeline(syncobj_manager, timeline_fd)};

    a_client.roundtrip();
}

TEST_F(LinuxDRMSyncobjV1Test, request_to_import_non_timeline_fails)
{
    using namespace testing;

    Fd not_a_syncobj{open("/dev/null", O_RDWR | O_CLOEXEC)};

    wlcs::WlHandle<wp_linux_drm_syncobj_timeline_v1> timeline{
        wp_linux_drm_syncobj_manager_v1_import_timeline(syncobj_manager, not_a_syncobj)};


    EXPECT_PROTOCOL_ERROR(
        {
            a_client.roundtrip();
        },
        &wp_linux_drm_syncobj_manager_v1_interface,
        WP_LINUX_DRM_SYNCOBJ_MANAGER_V1_ERROR_INVALID_TIMELINE);
}

TEST_F(LinuxDRMSyncobjV1Test, can_get_surface_timeline_object)
{
    auto surface =  a_client.create_visible_surface(200, 200);
    wlcs::WlHandle<wp_linux_drm_syncobj_surface_v1> surface_timeline{
        wp_linux_drm_syncobj_manager_v1_get_surface(syncobj_manager, surface)
    };

    a_client.roundtrip();
}

TEST_F(LinuxDRMSyncobjV1Test, can_set_timeline_acquire_point)
{
    auto surface =  a_client.create_visible_surface(200, 200);
    wlcs::WlHandle<wp_linux_drm_syncobj_surface_v1> surface_timeline{
        wp_linux_drm_syncobj_manager_v1_get_surface(syncobj_manager, surface)
    };

    Syncobj syncobj{open_timeline_capable_drm_node()};
    auto timeline_fd = syncobj.get_fd_handle();
    wlcs::WlHandle<wp_linux_drm_syncobj_timeline_v1> timeline{
        wp_linux_drm_syncobj_manager_v1_import_timeline(syncobj_manager, timeline_fd)};

    wp_linux_drm_syncobj_surface_v1_set_acquire_point(surface_timeline, timeline, 0, 0);

    a_client.roundtrip();
}

TEST_F(LinuxDRMSyncobjV1Test, get_surface_twice_is_an_error)
{
    using namespace testing;

    auto surface =  a_client.create_visible_surface(200, 200);
    wlcs::WlHandle<wp_linux_drm_syncobj_surface_v1> surface_timeline{
        wp_linux_drm_syncobj_manager_v1_get_surface(syncobj_manager, surface)
    };

    EXPECT_PROTOCOL_ERROR(
        {
            wlcs::WlHandle<wp_linux_drm_syncobj_surface_v1> second_timeline_surface{
                wp_linux_drm_syncobj_manager_v1_get_surface(syncobj_manager, surface)
            };
            a_client.roundtrip();
        },
        &wp_linux_drm_syncobj_manager_v1_interface,
        WP_LINUX_DRM_SYNCOBJ_MANAGER_V1_ERROR_SURFACE_EXISTS
    );
}

TEST_F(LinuxDRMSyncobjV1Test, committing_without_setting_acquire_point_is_an_error)
try
{
    using namespace testing;

    auto surface =  a_client.create_visible_surface(200, 200);
    wlcs::WlHandle<wp_linux_drm_syncobj_surface_v1> surface_timeline{
        wp_linux_drm_syncobj_manager_v1_get_surface(syncobj_manager, surface)
    };

    Syncobj syncobj{open_timeline_capable_drm_node()};
    auto timeline_fd = syncobj.get_fd_handle();
    wlcs::WlHandle<wp_linux_drm_syncobj_timeline_v1> timeline{
        wp_linux_drm_syncobj_manager_v1_import_timeline(syncobj_manager, timeline_fd)};

    wp_linux_drm_syncobj_surface_v1_set_release_point(surface_timeline, timeline, 0, 0);

    auto const& buffer = a_client.create_buffer(200, 200);
    wl_surface_attach(surface, buffer, 0, 0);

    EXPECT_PROTOCOL_ERROR(
        {
            wl_surface_commit(surface);
            a_client.roundtrip();
        },
        &wp_linux_drm_syncobj_surface_v1_interface,
        WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_NO_ACQUIRE_POINT);
}
catch(wlcs::ProtocolError const& err)
{
    if (err.interface() == &wp_linux_drm_syncobj_surface_v1_interface &&
        err.error_code() == WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_UNSUPPORTED_BUFFER)
    {
        ::testing::Test::RecordProperty("wlcs-skip-test", "wp_linux_drm_syncobj implementation doesn't support wl_shm buffers");
    }
    throw;
}

TEST_F(LinuxDRMSyncobjV1Test, committing_without_setting_release_point_is_an_error)
try
{
    using namespace testing;

    auto surface =  a_client.create_visible_surface(200, 200);
    wlcs::WlHandle<wp_linux_drm_syncobj_surface_v1> surface_timeline{
        wp_linux_drm_syncobj_manager_v1_get_surface(syncobj_manager, surface)
    };

    Syncobj syncobj{open_timeline_capable_drm_node()};
    auto timeline_fd = syncobj.get_fd_handle();
    wlcs::WlHandle<wp_linux_drm_syncobj_timeline_v1> timeline{
        wp_linux_drm_syncobj_manager_v1_import_timeline(syncobj_manager, timeline_fd)};

    wp_linux_drm_syncobj_surface_v1_set_acquire_point(surface_timeline, timeline, 0, 0);

    auto const& buffer = a_client.create_buffer(200, 200);
    wl_surface_attach(surface, buffer, 0, 0);

    EXPECT_PROTOCOL_ERROR(
        {
            wl_surface_commit(surface);
            a_client.roundtrip();
        },
        &wp_linux_drm_syncobj_surface_v1_interface,
        WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_NO_RELEASE_POINT);
}
catch(wlcs::ProtocolError const& err)
{
    if (err.interface() == &wp_linux_drm_syncobj_surface_v1_interface &&
        err.error_code() == WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_UNSUPPORTED_BUFFER)
    {
        ::testing::Test::RecordProperty("wlcs-skip-test", "wp_linux_drm_syncobj implementation doesn't support wl_shm buffers");
    }
    throw;
}

TEST_F(LinuxDRMSyncobjV1Test, setting_syncpoint_without_buffer_is_error)
try
{
    using namespace testing;

    auto surface =  a_client.create_visible_surface(200, 200);
    wlcs::WlHandle<wp_linux_drm_syncobj_surface_v1> surface_timeline{
        wp_linux_drm_syncobj_manager_v1_get_surface(syncobj_manager, surface)
    };

    Syncobj syncobj{open_timeline_capable_drm_node()};
    auto timeline_fd = syncobj.get_fd_handle();
    wlcs::WlHandle<wp_linux_drm_syncobj_timeline_v1> timeline{
        wp_linux_drm_syncobj_manager_v1_import_timeline(syncobj_manager, timeline_fd)};

    wp_linux_drm_syncobj_surface_v1_set_acquire_point(surface_timeline, timeline, 0, 0);
    wp_linux_drm_syncobj_surface_v1_set_release_point(surface_timeline, timeline, 0, 1);

    EXPECT_PROTOCOL_ERROR(
        {
            wl_surface_commit(surface);
            a_client.roundtrip();
        },
        &wp_linux_drm_syncobj_surface_v1_interface,
        WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_NO_BUFFER);
}
catch(wlcs::ProtocolError const& err)
{
    if (err.interface() == &wp_linux_drm_syncobj_surface_v1_interface &&
        err.error_code() == WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_UNSUPPORTED_BUFFER)
    {
        ::testing::Test::RecordProperty("wlcs-skip-test", "wp_linux_drm_syncobj implementation doesn't support wl_shm buffers");
    }
    throw;
}

TEST_F(LinuxDRMSyncobjV1Test, committing_without_both_syncpoint_and_buffer_is_valid)
try
{
    using namespace testing;

    auto surface =  a_client.create_visible_surface(200, 200);
    wlcs::WlHandle<wp_linux_drm_syncobj_surface_v1> surface_timeline{
        wp_linux_drm_syncobj_manager_v1_get_surface(syncobj_manager, surface)
    };

    Syncobj syncobj{open_timeline_capable_drm_node()};
    auto timeline_fd = syncobj.get_fd_handle();
    wlcs::WlHandle<wp_linux_drm_syncobj_timeline_v1> timeline{
        wp_linux_drm_syncobj_manager_v1_import_timeline(syncobj_manager, timeline_fd)};

    /* “Clients must set both acquire and release points if and only if a non-null buffer is
     * attached in the same commit”
     *
     * We don't have a buffer, and we haven't set any release points; this is valid
     */
    wl_surface_commit(surface);
    a_client.roundtrip();
}
catch(wlcs::ProtocolError const& err)
{
    if (err.interface() == &wp_linux_drm_syncobj_surface_v1_interface &&
        err.error_code() == WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_UNSUPPORTED_BUFFER)
    {
        ::testing::Test::RecordProperty("wlcs-skip-test", "wp_linux_drm_syncobj implementation doesn't support wl_shm buffers");
    }
    throw;
}

TEST_F(LinuxDRMSyncobjV1Test, committing_a_null_buffer_without_syncpoint_is_valid)
try
{
    using namespace testing;

    auto surface =  a_client.create_visible_surface(200, 200);
    wlcs::WlHandle<wp_linux_drm_syncobj_surface_v1> surface_timeline{
        wp_linux_drm_syncobj_manager_v1_get_surface(syncobj_manager, surface)
    };

    Syncobj syncobj{open_timeline_capable_drm_node()};
    auto timeline_fd = syncobj.get_fd_handle();
    wlcs::WlHandle<wp_linux_drm_syncobj_timeline_v1> timeline{
        wp_linux_drm_syncobj_manager_v1_import_timeline(syncobj_manager, timeline_fd)};

    /* “Clients must set both acquire and release points if and only if a non-null buffer is
     * attached in the same commit”
     *
     * We are attaching a NULL buffer, and we haven't set any release points; this is valid
     */
    wl_surface_attach(surface, nullptr, 0, 0);
    wl_surface_commit(surface);
    a_client.roundtrip();
}
catch(wlcs::ProtocolError const& err)
{
    if (err.interface() == &wp_linux_drm_syncobj_surface_v1_interface &&
        err.error_code() == WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_UNSUPPORTED_BUFFER)
    {
        ::testing::Test::RecordProperty("wlcs-skip-test", "wp_linux_drm_syncobj implementation doesn't support wl_shm buffers");
    }
    throw;
}

TEST_F(LinuxDRMSyncobjV1Test, release_point_signalled_on_buffer_release)
try
{
    using namespace testing;

    auto surface =  a_client.create_visible_surface(200, 200);
    wlcs::WlHandle<wp_linux_drm_syncobj_surface_v1> surface_timeline{
        wp_linux_drm_syncobj_manager_v1_get_surface(syncobj_manager, surface)
    };

    auto drm_fd = open_timeline_capable_drm_node();
    Syncobj syncobj{drm_fd.dup()};
    auto timeline_fd = syncobj.get_fd_handle();
    wlcs::WlHandle<wp_linux_drm_syncobj_timeline_v1> timeline{
        wp_linux_drm_syncobj_manager_v1_import_timeline(syncobj_manager, timeline_fd)};

    wp_linux_drm_syncobj_surface_v1_set_acquire_point(surface_timeline, timeline, 0, 0);
    // The buffer needs to be ready for the commit to be applied.
    syncobj.signal(0);

    uint64_t const release_point = 42;    // This only needs to be >= the acquire point
    uint32_t const release_point_lo = release_point & 0xffffffff;
    uint32_t const release_point_hi = release_point >> 32;
    wp_linux_drm_syncobj_surface_v1_set_release_point(surface_timeline, timeline, release_point_hi, release_point_lo);

    // Submit a buffer with the syncobj
    wlcs::ShmBuffer buffer_one{a_client, 200, 200};
    wl_surface_attach(surface, buffer_one, 0, 0);
    wl_surface_commit(surface);

    a_client.roundtrip();

    // Submit another buffer so that the original buffer is released
    Syncobj syncobj_two{drm_fd.dup()};
    auto timeline_fd_two = syncobj.get_fd_handle();
    wlcs::WlHandle<wp_linux_drm_syncobj_timeline_v1> timeline_two{
        wp_linux_drm_syncobj_manager_v1_import_timeline(syncobj_manager, timeline_fd_two)};

    wp_linux_drm_syncobj_surface_v1_set_acquire_point(surface_timeline, timeline_two, 0, 0);
    wp_linux_drm_syncobj_surface_v1_set_release_point(surface_timeline, timeline_two, 0, 1);

    // We need to actually mark the buffer as ready
    syncobj_two.signal(0);

    wlcs::ShmBuffer buffer_two{a_client, 200, 200};
    wl_surface_attach(surface, buffer_two, 0, 0);
    wl_surface_commit(surface);
    a_client.roundtrip();


    using namespace std::chrono;
    using namespace std::chrono_literals;
    // The release timepoint should now be signalled at some point.
    uint32_t handle = syncobj;
    if (auto err = drmSyncobjTimelineWait(drm_fd, &handle, const_cast<uint64_t*>(&release_point), 1, duration_cast<nanoseconds>((steady_clock::now() + 10s).time_since_epoch()).count(), DRM_SYNCOBJ_WAIT_FLAGS_WAIT_FOR_SUBMIT | DRM_SYNCOBJ_WAIT_FLAGS_WAIT_ALL, nullptr))
    {
        FAIL() << "Error (or timeout) waiting for release syncpoint to be signalled: " << strerror(-err);
    }
}
catch(wlcs::ProtocolError const& err)
{
    if (err.interface() == &wp_linux_drm_syncobj_surface_v1_interface &&
        err.error_code() == WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_UNSUPPORTED_BUFFER)
    {
        ::testing::Test::RecordProperty("wlcs-skip-test", "wp_linux_drm_syncobj implementation doesn't support wl_shm buffers");
    }
    throw;
}

TEST_F(LinuxDRMSyncobjV1Test, committed_buffer_not_applied_until_acquire_point_signalled)
try
{
    using namespace testing;
    using namespace std::chrono_literals;

    int const original_width = 200, original_height = 200;
    int const new_width = 400, new_height = 400;

    auto surface =  a_client.create_visible_surface(original_width, original_height);

    ASSERT_THAT(surface, IsSurfaceOfSize(original_width, original_height));

    wlcs::WlHandle<wp_linux_drm_syncobj_surface_v1> surface_timeline{
        wp_linux_drm_syncobj_manager_v1_get_surface(syncobj_manager, surface)
    };

    auto drm_fd = open_timeline_capable_drm_node();
    Syncobj syncobj{drm_fd.dup()};
    auto timeline_fd = syncobj.get_fd_handle();
    wlcs::WlHandle<wp_linux_drm_syncobj_timeline_v1> timeline{
        wp_linux_drm_syncobj_manager_v1_import_timeline(syncobj_manager, timeline_fd)};

    wp_linux_drm_syncobj_surface_v1_set_acquire_point(surface_timeline, timeline, 0, 0);
    wp_linux_drm_syncobj_surface_v1_set_release_point(surface_timeline, timeline, 0, 1);

    // Submit a buffer with a different size
    wlcs::ShmBuffer buffer_one{a_client, new_width, new_height};
    wl_surface_attach(surface, buffer_one, 0, 0);
    wl_surface_commit(surface);

    a_client.roundtrip();

    // We don't have a way to ensure that WM has settled down, so just wait a little
    std::this_thread::sleep_for(1s);

    // We haven't signalled the acquire point, so we should see the old size.
    EXPECT_THAT(surface, IsSurfaceOfSize(original_width, original_height));

    syncobj.signal(0);

    // Again, we don't have a way to ensure that WM has settled down, so just wait a little
    std::this_thread::sleep_for(1s);

    EXPECT_THAT(surface,IsSurfaceOfSize(new_width, new_height));
}
catch(wlcs::ProtocolError const& err)
{
    if (err.interface() == &wp_linux_drm_syncobj_surface_v1_interface &&
        err.error_code() == WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_UNSUPPORTED_BUFFER)
    {
        ::testing::Test::RecordProperty("wlcs-skip-test", "wp_linux_drm_syncobj implementation doesn't support wl_shm buffers");
    }
    throw;
}
