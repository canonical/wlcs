# Copyright © 2017 Canonical Ltd.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 or 3 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# Authored by: Thomas Voss <thomas.voss@canonical.com>,
#              Alan Griffiths <alan@octopull.co.uk>,
#              Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>

cmake_minimum_required(VERSION 3.5)

cmake_policy(SET CMP0015 NEW)
cmake_policy(SET CMP0022 NEW)

project(wlcs)

add_definitions(-D_GNU_SOURCE)
add_definitions(-D_FILE_OFFSET_BITS=64)


set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(GNUInstallDirs)
find_package(PkgConfig)
#include (Doxygen.cmake)


set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread -g -Werror -Wall -pedantic -Wextra -fPIC")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -g -std=c++14 -Werror -Wall -fno-strict-aliasing -pedantic -Wnon-virtual-dtor -Wextra -fPIC")
if ("${CMAKE_SHARED_LINKER_FLAGS}" MATCHES ".*-fsanitize=.*")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}")
else()
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--as-needed -Wl,--no-undefined")
endif()

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-Wgnu-zero-variadic-macro-arguments HAVE_W_GNU_VARIADIC_MACROS)
if (HAVE_W_GNU_VARIADIC_MACROS)
  # GTest's parametrised test macro tweaks this warning
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=gnu-zero-variadic-macro-arguments")
endif ()

find_package(GtestGmock)
pkg_check_modules(WAYLAND_CLIENT REQUIRED wayland-client)

include_directories(include)
include_directories(${GMOCK_INCLUDE_DIR} ${GTEST_INCLUDE_DIR})

add_subdirectory(src/protocol/)

add_library(
  wlcs SHARED

  include/display_server.h
  include/helpers.h
  include/in_process_server.h

  src/data_device.cpp
  src/helpers.cpp
  src/in_process_server.cpp
  src/xdg_shell_v6.cpp
  src/main.cpp

  src/generated/wayland.c
  src/generated/xdg-shell-unstable-v6.c
  src/generated/xdg-shell.c

  tests/test_bad_buffer.cpp
  tests/copy_cut_paste.cpp
  tests/test_surface_events.cpp
  tests/touches.cpp
  tests/surface_input_regions.cpp
  tests/frame_submission.cpp
  tests/subsurfaces.cpp
  tests/xdg_surface_v6.cpp
  tests/xdg_toplevel_v6.cpp
  tests/self_test.cpp
)

target_link_libraries(
  wlcs

  ${WAYLAND_CLIENT_LDFLAGS} ${WAYLAND_CLIENT_LIBRARIES}

  ${GMOCK_LIBRARIES}
  ${GTEST_BOTH_LIBRARIES}
)

add_dependencies(wlcs wlcs_protocols)
