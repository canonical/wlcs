/*
 * Copyright © 2018 Canonical Ltd.
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
 *
 * Authored by: William Wold <william.wold@canonical.com>
 */
#ifndef WLCS_TOUCH_H_
#define WLCS_TOUCH_H_

#include <wayland-client-core.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct WlcsTouch WlcsTouch;

void wlcs_touch_down(WlcsTouch* touch, wl_fixed_t x, wl_fixed_t y) __attribute__((weak));
void wlcs_touch_move(WlcsTouch* touch, wl_fixed_t x, wl_fixed_t y) __attribute__((weak));
void wlcs_touch_up(WlcsTouch* touch) __attribute__((weak));

#ifdef __cplusplus
}
#endif

#endif //WLCS_TOUCH_H_
