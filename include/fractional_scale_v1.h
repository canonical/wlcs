/*
 * Copyright © 2020 Canonical Ltd.
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

#ifndef WLCS_FRACTIONAL_SCALE_V1_H
#define WLCS_FRACTIONAL_SCALE_V1_H

#include "generated/fractional-scale-v1-client.h"
#include "wl_handle.h"
#include "wl_interface_descriptor.h"

namespace wlcs
{
WLCS_CREATE_INTERFACE_DESCRIPTOR(wp_fractional_scale_manager_v1);

class Client;

class WpFractionalScaleManagerV1
{
public:
    WpFractionalScaleManagerV1(Client& client);
    ~WpFractionalScaleManagerV1();

    operator wp_fractional_scale_manager_v1*() const;
    friend void wp_fractional_scale_manager_v1_destroy(WpFractionalScaleManagerV1 const&) = delete;

private:
    WlHandle<wp_fractional_scale_manager_v1> manager;
};

class WpFractionalScaleV1
{
public:
    WpFractionalScaleV1(WpFractionalScaleManagerV1 const& manager);
    ~WpFractionalScaleV1();


    operator wp_fractional_scale_v1*() const;
    friend void wp_fractional_scale_v1_destroy(WpFractionalScaleManagerV1 const&) = delete;

private:
    wp_fractional_scale_v1* const fractional_scale;

    static wp_fractional_scale_v1_listener const listener;
};

}

#endif // WLCS_FRACTIONAL_SCALE_V1_H
