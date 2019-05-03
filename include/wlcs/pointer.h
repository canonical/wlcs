/*
 * Copyright © 2017 Canonical Ltd.
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
 * Authored by: Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */
#ifndef WLCS_POINTER_H_
#define WLCS_POINTER_H_

#include <wayland-client-core.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Maximum version of WlcsPointer this header provides a definition for
 */
#define WLCS_POINTER_VERSION 1

typedef struct WlcsPointer WlcsPointer;
/**
 * An object to manipulate the server's pointer state
 */
struct WlcsPointer
{
    uint32_t version; /**< Version of the struct this instance provides */

    /**
     * Move the pointer to the specified location, in compositor coördinate space
     */
    void (*move_absolute)(WlcsPointer* pointer, wl_fixed_t x, wl_fixed_t y);
    /**
     * Move the pointer by the specified amount, in compositor coördinates.
     */
    void (*move_relative)(WlcsPointer* pointer, wl_fixed_t dx, wl_fixed_t dy);

    /**
     * Generate a button-up event
     *
     * \param button    Button code (as per wl_pointer. eg: BTN_LEFT)
     */
    void (*button_up)(WlcsPointer* pointer, int button);
    /**
     * Generate a button-down event
     *
     * \param button    Button code (as per wl_pointer. eg: BTN_LEFT)
     */
    void (*button_down)(WlcsPointer* pointer, int button);

    /**
     * Destroy this pointer, freeing any resources.
     */
    void (*destroy)(WlcsPointer* pointer);
};

#ifdef __cplusplus
}
#endif

#endif //WLCS_SERVER_H_
