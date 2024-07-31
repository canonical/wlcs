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

#include "fractional_scale_v1.h"

#include "generated/fractional-scale-v1-client.h"
#include "in_process_server.h"
#include "version_specifier.h"

wlcs::WpFractionalScaleManagerV1::WpFractionalScaleManagerV1(wlcs::Client& client)
    : manager{client.bind_if_supported<wp_fractional_scale_manager_v1>(AnyVersion)}
{

}

wlcs::WpFractionalScaleManagerV1::~WpFractionalScaleManagerV1() = default;

wlcs::WpFractionalScaleManagerV1::operator wp_fractional_scale_manager_v1 *() const
{
    return manager;
}

wlcs::WpFractionalScaleV1::WpFractionalScaleV1(wlcs::WpFractionalScaleManagerV1 const& manager, wlcs::Surface const& surface)
    : version{wp_fractional_scale_manager_v1_get_version(manager)},
      fractional_scale(wp_fractional_scale_manager_v1_get_fractional_scale(manager, surface))
{
    wp_fractional_scale_v1_set_user_data(fractional_scale, this);
    wp_fractional_scale_v1_add_listener(fractional_scale, &listener, this);
}

wlcs::WpFractionalScaleV1::~WpFractionalScaleV1() { wp_fractional_scale_v1_destroy(fractional_scale); }

wlcs::WpFractionalScaleV1::operator wp_fractional_scale_v1 *() const
{
    return fractional_scale;
}

wp_fractional_scale_v1_listener const wlcs::WpFractionalScaleV1::listener = {
    [](void* self, auto*, auto... args)
    {
        static_cast<WpFractionalScaleV1*>(self)->preferred_scale(args...);
    }
};
