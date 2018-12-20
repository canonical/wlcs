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
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(PROTOCOL_SOURCES "")

execute_process(
  COMMAND wayland-scanner --version
  OUTPUT_VARIABLE WAYLAND_SCANNER_VERSION_OUTPUT
  ERROR_VARIABLE WAYLAND_SCANNER_VERSION_OUTPUT
)

separate_arguments(WAYLAND_SCANNER_VERSION_OUTPUT)
list(LENGTH WAYLAND_SCANNER_VERSION_OUTPUT VERSION_STRING_COMPONENTS)
list(GET WAYLAND_SCANNER_VERSION_OUTPUT 1 VERSION_STRING)
string(STRIP ${VERSION_STRING} VERSION_STRING)

if (NOT(VERSION_STRING_COMPONENTS EQUAL 2))
  message(AUTHOR_WARNING "Failed to parse wayland-scanner --version output")
endif()

if (VERSION_STRING VERSION_GREATER 1.14.91)
  message(STATUS "Found wayland-scanner version ${VERSION_STRING}, using private-code mode")
  set(WAYLAND_SCANNER_CODE_GENERATION_TYPE "private-code")
else()
  message(STATUS "Found wayland-scanner version ${VERSION_STRING}, using (old) code mode")
  set(WAYLAND_SCANNER_CODE_GENERATION_TYPE "code")
endif()

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/generated)

macro(GENERATE_PROTOCOL PROTOCOL_NAME)
  set(PROTOCOL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src/protocol/${PROTOCOL_NAME}.xml")
  set(OUTPUT_PATH_C "${CMAKE_CURRENT_BINARY_DIR}/generated/${PROTOCOL_NAME}.c")
  set(OUTPUT_PATH_SERVER_H "${CMAKE_CURRENT_BINARY_DIR}/generated/${PROTOCOL_NAME}-server.h")
  set(OUTPUT_PATH_CLIENT_H "${CMAKE_CURRENT_BINARY_DIR}/generated/${PROTOCOL_NAME}-client.h")
  add_custom_command(OUTPUT "${OUTPUT_PATH_C}"
    VERBATIM
    COMMAND "wayland-scanner" "--include-core-only" ${WAYLAND_SCANNER_CODE_GENERATION_TYPE} "${PROTOCOL_PATH}" "${OUTPUT_PATH_C}"
    DEPENDS "${PROTOCOL_PATH}"
  )
  add_custom_command(OUTPUT "${OUTPUT_PATH_SERVER_H}"
    VERBATIM
    COMMAND "wayland-scanner" "--include-core-only" "server-header" "${PROTOCOL_PATH}" "${OUTPUT_PATH_SERVER_H}"
    DEPENDS "${PROTOCOL_PATH}"
  )
  add_custom_command(OUTPUT "${OUTPUT_PATH_CLIENT_H}"
    VERBATIM
    COMMAND "wayland-scanner" "--include-core-only" "client-header" "${PROTOCOL_PATH}" "${OUTPUT_PATH_CLIENT_H}"
    DEPENDS "${PROTOCOL_PATH}"
  )
  list(APPEND PROTOCOL_SOURCES ${OUTPUT_PATH_C} ${OUTPUT_PATH_CLIENT_H})
endmacro()

GENERATE_PROTOCOL(wayland)
GENERATE_PROTOCOL(xdg-shell-unstable-v6)
GENERATE_PROTOCOL(xdg-shell)

add_executable(
  wlcs

  include/display_server.h
  include/helpers.h
  include/in_process_server.h
  include/xdg_shell_v6.h
  include/xdg_shell_stable.h

  src/data_device.cpp
  src/helpers.cpp
  src/in_process_server.cpp
  src/xdg_shell_v6.cpp
  src/xdg_shell_stable.cpp
  src/main.cpp
  src/shared_library.cpp
  src/module_deleter.cpp

  ${PROTOCOL_SOURCES}

  tests/test_bad_buffer.cpp
  tests/copy_cut_paste.cpp
  tests/test_surface_events.cpp
  tests/touches.cpp
  tests/surface_input_regions.cpp
  tests/frame_submission.cpp
  tests/subsurfaces.cpp
  tests/xdg_surface_v6.cpp
  tests/xdg_toplevel_v6.cpp
  tests/xdg_popup_v6.cpp
  tests/xdg_surface_stable.cpp
  tests/xdg_toplevel_stable.cpp
  tests/xdg_popup_stable.cpp
  tests/self_test.cpp
)

target_link_libraries(
  wlcs

  ${WAYLAND_CLIENT_LDFLAGS} ${WAYLAND_CLIENT_LIBRARIES}
  dl

  ${GMOCK_LIBRARIES}
  ${GTEST_BOTH_LIBRARIES}
)
