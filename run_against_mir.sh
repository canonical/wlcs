#!/bin/bash
set -euo pipefail

cd build
make
cd ..

FILTER="-"
FILTER="$FILTER:*times_out*"
FILTER="$FILTER:ClientSurfaceEventsTest.frame_timestamp_increases"
FILTER="$FILTER:*place_above_simple*:*place_below_simple*"
FILTER="$FILTER:*pointer_leaves_surface_during_interactive_move*"
FILTER="$FILTER:*subsurface_moves_out_from_under_input_device*"
FILTER="$FILTER:*subsurface_moves_under_input_device_twice*"
FILTER="$FILTER:*subsurface_moves_under_input_device_once*"
FILTER="$FILTER:*attaching_buffer_to_unconfigured_xdg_surface_is_an_error*"
FILTER="$FILTER:*pointer_leaves_surface_during_interactive_resize*"
FILTER="$FILTER:*creating_xdg_surface_from_wl_surface_with_committed_buffer_is_an_error*"
FILTER="$FILTER:*creating_xdg_surface_from_wl_surface_with_attached_buffer_is_an_error*"
FILTER="$FILTER:*creating_xdg_surface_from_wl_surface_with_existing_role_is_an_error*"
FILTER="$FILTER:*pointer_leaves_surface_during_interactive_resize*"
FILTER="$FILTER:*SubsurfaceTest*"
FILTER="$FILTER:*SubsurfaceMultilevelTest*"
#FILTER="$FILTER:*CopyCutPaste*"

./build/wlcs ../mir/build/lib/miral_wlcs_integration.so --gtest_filter="$FILTER" $@
