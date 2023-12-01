/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2 or 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by:
 *   Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "helpers.h"
#include "shared_library.h"

#include <boost/throw_exception.hpp>
#include <system_error>

#include <fcntl.h>
#include <linux/memfd.h>
#include <sys/syscall.h>

/* Since kernel 6.3 it generates a warning to construct a memfd without one of
 * MFD_EXEC (to mark the memfd as executable) or MFD_NOEXEC_SEAL (to permanently
 * prevent the memfd from being marked as executable).
 *
 * Since we don't need execution from our shm buffers, we can mark them as
 * MFD_NOEXEC_SEAL. Since this is only silencing a warning in dmesg we can safely
 * null it out if we're building against too-old headers.
 */
#ifndef MFD_NOEXEC_SEAL
#define MFD_NOEXEC_SEAL 0
#endif

namespace
{

bool error_indicates_tmpfile_not_supported(int error)
{
    return
        error == EISDIR ||  // Directory exists, but no support for O_TMPFILE
        error == ENOENT ||  // Directory doesn't exist, and no support for O_TMPFILE
        error == EOPNOTSUPP ||    // Filesystem that directory resides on does not support O_TMPFILE
        error == EINVAL;    // There apparently exists at least one development board that has a kernel
    // that incorrectly returns EINVAL. Yay.
}

int memfd_create(char const* name, unsigned int flags)
{
    return static_cast<int>(syscall(SYS_memfd_create, name, flags));
}
}

int wlcs::helpers::create_anonymous_file(size_t size)
{

    int fd = memfd_create("wlcs-unnamed", MFD_CLOEXEC | MFD_NOEXEC_SEAL);
    if (fd == -1 && errno == EINVAL)
    {
        // Maybe we're running on a kernel prior to MFD_NOEXEC_SEAL?
        fd = memfd_create("wlcs-unnamed", MFD_CLOEXEC);
    }
    if (fd == -1 && errno == ENOSYS)
    {
        fd = open("/dev/shm", O_TMPFILE | O_RDWR | O_EXCL | O_CLOEXEC, S_IRWXU);

        // Workaround for filesystems that don't support O_TMPFILE
        if (fd == -1 && error_indicates_tmpfile_not_supported(errno))
        {
            char template_filename[] = "/dev/shm/wlcs-buffer-XXXXXX";
            fd = mkostemp(template_filename, O_CLOEXEC);
            if (fd != -1)
            {
                if (unlink(template_filename) < 0)
                {
                    close(fd);
                    fd = -1;
                }
            }
        }
    }

    if (fd == -1)
    {
        BOOST_THROW_EXCEPTION(
            std::system_error(errno, std::system_category(), "Failed to open temporary file"));
    }

    if (ftruncate(fd, size) == -1)
    {
        close(fd);
        BOOST_THROW_EXCEPTION(
            std::system_error(errno, std::system_category(), "Failed to resize temporary file"));
    }

    return fd;
}

namespace
{
static int argc;
static char const** argv;

std::shared_ptr<WlcsServerIntegration const> entry_point;
}

void wlcs::helpers::set_command_line(int argc, char const** argv)
{
    ::argc = argc;
    ::argv = argv;
}

int wlcs::helpers::get_argc()
{
    return ::argc;
}

char const** wlcs::helpers::get_argv()
{
    return ::argv;
}

void wlcs::helpers::set_entry_point(std::shared_ptr<WlcsServerIntegration const> const& entry_point)
{
    ::entry_point = entry_point;
}

std::shared_ptr<WlcsServerIntegration const> wlcs::helpers::get_test_hooks()
{
    return ::entry_point;
}
