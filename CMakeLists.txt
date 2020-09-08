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

project(wlcs VERSION 1.1.0)

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

include(CheckCXXSymbolExists)
list(APPEND CMAKE_REQUIRED_HEADERS ${GTEST_INCLUDE_DIRECTORIES})
check_cxx_symbol_exists(INSTANTIATE_TEST_SUITE_P "gtest/gtest.h" HAVE_INSTANTIATE_TEST_SUITE_P)
if (NOT HAVE_INSTANTIATE_TEST_SUITE_P)
  #GTest conveniently renamed INSTANTIATE_TEST_CASE_P and then deprecated it.
  add_definitions(-DINSTANTIATE_TEST_SUITE_P=INSTANTIATE_TEST_CASE_P)
endif()

find_package(Boost)
find_package(GtestGmock)
pkg_check_modules(WAYLAND_CLIENT REQUIRED wayland-client)
pkg_check_modules(WAYLAND_SERVER REQUIRED wayland-server)
pkg_check_modules(WAYLAND_SCANNER REQUIRED wayland-scanner)

include_directories(include)
include_directories(${Boost_INCLUDE_DIRS})
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

GENERATE_PROTOCOL(gtk-primary-selection)
GENERATE_PROTOCOL(primary-selection-unstable-v1)
GENERATE_PROTOCOL(wayland)
GENERATE_PROTOCOL(xdg-shell-unstable-v6)
GENERATE_PROTOCOL(xdg-shell)
GENERATE_PROTOCOL(wlr-layer-shell-unstable-v1)
GENERATE_PROTOCOL(xdg-output-unstable-v1)
GENERATE_PROTOCOL(wlr-foreign-toplevel-management-unstable-v1)
GENERATE_PROTOCOL(pointer-constraints-unstable-v1)
GENERATE_PROTOCOL(relative-pointer-unstable-v1)

option(WLCS_BUILD_ASAN "Build a test runner with AddressSanitizer annotations" ON)
option(WLCS_BUILD_TSAN "Build a test runner with ThreadSanitizer annotations" ON)
option(WLCS_BUILD_UBSAN "Build a test runner with UndefinedBehaviourSanitizer annotations" ON)

set(
  WLCS_SOURCES

  include/wlcs/display_server.h
  include/wlcs/pointer.h
  include/wlcs/touch.h
  include/helpers.h
  include/wl_handle.h
  include/in_process_server.h
  include/pointer_constraints_unstable_v1.h
  include/primary_selection.h
  include/relative_pointer_unstable_v1.h
  include/xdg_shell_v6.h
  include/xdg_shell_stable.h
  include/xdg_output_v1.h
  include/version_specifier.h
  include/wl_interface_descriptor.h
  include/layer_shell_v1.h
  include/surface_builder.h
  include/input_method.h

  src/data_device.cpp
  src/gtk_primary_selection.cpp
  src/helpers.cpp
  src/in_process_server.cpp
  src/xdg_shell_v6.cpp
  src/xdg_shell_stable.cpp
  src/layer_shell_v1.cpp
  src/main.cpp
  src/pointer_constraints_unstable_v1.cpp
  src/primary_selection.cpp
  src/shared_library.cpp
  src/relative_pointer_unstable_v1.cpp
  src/xfail_supporting_test_listener.h
  src/xfail_supporting_test_listener.cpp
  src/termcolor.hpp
  src/thread_proxy.h
  src/xdg_output_v1.cpp
  src/version_specifier.cpp
  src/surface_builder.cpp
  src/input_method.cpp

  ${PROTOCOL_SOURCES}

  tests/test_bad_buffer.cpp
  tests/pointer_constraints.cpp
  tests/copy_cut_paste.cpp
  tests/gtk_primary_selection.cpp
  tests/test_surface_events.cpp
  tests/touches.cpp
  tests/wl_output.cpp
  tests/surface_input_regions.cpp
  tests/frame_submission.cpp
  tests/primary_selection.cpp
  tests/relative_pointer.cpp
  tests/subsurfaces.cpp
  tests/xdg_surface_v6.cpp
  tests/xdg_toplevel_v6.cpp
  tests/xdg_surface_stable.cpp
  tests/xdg_toplevel_stable.cpp
  tests/xdg_popup.cpp
  tests/wlr_layer_shell_v1.cpp
  tests/xdg_output_v1.cpp
  tests/wlr_foreign_toplevel_management_v1.cpp
  tests/self_test.cpp
)

add_library(test_c_compile OBJECT src/test_c_compile.c)

add_executable(wlcs ${WLCS_SOURCES})
set(EXECUTABLE_TARGETS wlcs)
if (WLCS_BUILD_ASAN)
  add_executable(wlcs.asan ${WLCS_SOURCES})
  target_compile_options(wlcs.asan PUBLIC -fsanitize=address -fno-omit-frame-pointer)
  set_target_properties(
    wlcs.asan
    PROPERTIES
    LINK_FLAGS -fsanitize=address)

  # Explicitly linking with the sanitiser is harmless
  target_link_libraries(wlcs.asan asan)
  list(APPEND EXECUTABLE_TARGETS wlcs.asan)
endif()

if (WLCS_BUILD_TSAN)
  add_executable(wlcs.tsan ${WLCS_SOURCES})
  target_compile_options(wlcs.tsan PUBLIC -fsanitize=thread -fno-omit-frame-pointer)
  set_target_properties(
    wlcs.tsan
    PROPERTIES
    LINK_FLAGS -fsanitize=thread)
  # Explicitly linking with the sanitiser is harmless.
  target_link_libraries(wlcs.tsan tsan)
  list(APPEND EXECUTABLE_TARGETS wlcs.tsan)
endif()

if (WLCS_BUILD_UBSAN)
  add_executable(wlcs.ubsan ${WLCS_SOURCES})
  target_compile_options(wlcs.ubsan PUBLIC -fsanitize=undefined)
  set_target_properties(
    wlcs.ubsan
    PROPERTIES
    LINK_FLAGS -fsanitize=undefined)

  # Unsure quite why explicitly linking with ubsan is required, but here we are…
  target_link_libraries(wlcs.ubsan ubsan)
  list(APPEND EXECUTABLE_TARGETS wlcs.ubsan)
endif()

foreach(TARGET IN LISTS EXECUTABLE_TARGETS)
  target_link_libraries(
    ${TARGET}

    ${WAYLAND_CLIENT_LDFLAGS} ${WAYLAND_CLIENT_LIBRARIES}
    ${WAYLAND_SERVER_LDFLAGS} ${WAYLAND_CLIENT_LIBRARIES}
    dl

    ${GMOCK_LIBRARY}
    ${GTEST_LIBRARY}
  )
endforeach(TARGET)

install(
  TARGETS ${EXECUTABLE_TARGETS}
  RUNTIME DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/wlcs
)

install(
  DIRECTORY include/wlcs
  DESTINATION ${CMAKE_INSTALL_PREFIX}/include/
)

configure_file(
  wlcs.pc.in
  ${CMAKE_CURRENT_BINARY_DIR}/wlcs.pc
  @ONLY
)

install(
  FILES
    ${CMAKE_CURRENT_BINARY_DIR}/wlcs.pc
  DESTINATION
    ${CMAKE_INSTALL_LIBDIR}/pkgconfig/
)
