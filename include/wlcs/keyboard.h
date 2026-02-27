/*
 * Copyright © 2026 Canonical Ltd.
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
#ifndef WLCS_KEYBOARD_H_
#define WLCS_KEYBOARD_H_

#include <wayland-client-core.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Maximum version of WlcsKeyboard this header provides a definition for
 */
#define WLCS_KEYBOARD_VERSION 1

typedef struct WlcsKeyboard WlcsKeyboard;
/**
 * An object to manipulate the server's keyboard state
 */
struct WlcsKeyboard
{
    uint32_t version; /**< Version of the struct this instance provides */

    /**
     * Generate a key-down event
     *
     * \param scancode  Key code (as per Linux input event codes in
     *                  <linux/input-event-codes.h>. eg: KEY_A)
     */
    void (*key_down)(WlcsKeyboard* keyboard, int scancode);

    /**
     * Generate a key-up event
     *
     * \param scancode  Key code (as per Linux input event codes in
     *                  <linux/input-event-codes.h>. eg: KEY_A)
     */
    void (*key_up)(WlcsKeyboard* keyboard, int scancode);

    /**
     * Destroy this keyboard, freeing any resources.
     */
    void (*destroy)(WlcsKeyboard* keyboard);
};

#ifdef __cplusplus
}
#endif

#endif // WLCS_KEYBOARD_H_
