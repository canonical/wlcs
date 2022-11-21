#!/bin/sh

set -eox

local container=$1 sourcedir=$2 builddir=$3

lxc exec ${container} -- cmake \
  -DCMAKE_C_FLAGS="-D_FORTIFY_SOURCE=2" -DCMAKE_CXX_FLAGS="-D_FORTIFY_SOURCE=2" \
  -S ${sourcedir} -B ${builddir}

# Build WLCS
lxc exec ${container} -- make -j$(nproc) -C ${builddir}

# …and run the Mir tests, but don't fail on them, unless we fail with a signal
# TODO: Store the set of passing tests in the Travis cache, and fail if a test which
#       previously passed now fails.
lxc exec ${container} -- ${builddir}/wlcs \
  /usr/lib/$(dpkg-architecture -qDEB_HOST_MULTIARCH)/mir/miral_wlcs_integration.so || { \
    test $? -lt 128 -o $? -gt 165 \
  }

