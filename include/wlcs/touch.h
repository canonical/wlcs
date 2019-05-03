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

/**
 * Maximum version of WlcsTouch this header provides a definition for
 */
#define WLCS_TOUCH_VERSION 1

typedef struct WlcsTouch WlcsTouch;
struct WlcsTouch
{
    uint32_t version; /**< Version of the struct this instance provides */

    void (*touch_down)(WlcsTouch* touch, wl_fixed_t x, wl_fixed_t y);
    void (*touch_move)(WlcsTouch* touch, wl_fixed_t x, wl_fixed_t y);
    void (*touch_up)(WlcsTouch* touch);

    void (*destroy)(WlcsTouch* touch);
};

#ifdef __cplusplus
}
#endif

#endif //WLCS_TOUCH_H_
