/*
 * Copyright © 2018 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2 or 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#ifndef WLCS_COPY_CUT_PASTE_H_
#define WLCS_COPY_CUT_PASTE_H_

#include "in_process_server.h"
#include "data_device.h"
#include "version_specifier.h"

#include <gmock/gmock.h>

namespace wlcs
{
namespace
{
auto static const any_width = 100;
auto static const any_height = 100;
}
struct CCnPSource : Client
{
    using Client::Client;

    Surface const surface{create_visible_surface(any_width, any_height)};
    WlHandle<wl_data_device_manager> const manager{
        this->bind_if_supported<wl_data_device_manager>(AnyVersion)};
    DataDevice data_device{wl_data_device_manager_get_data_device(manager,seat())};
    DataSource data_source{wl_data_device_manager_create_data_source(manager)};

    void offer(char const* mime_type)
    {
        wl_data_source_offer(data_source, mime_type);
        // TODO: collect a serial from the "event that triggered" the selection
        // (This works while Mir fails to validate the serial)
        uint32_t const serial = 0;
        wl_data_device_set_selection(data_device, data_source, serial);
        roundtrip();
    }
};

struct MockDataDeviceListener : DataDeviceListener
{
    using DataDeviceListener::DataDeviceListener;

    MOCK_METHOD(void, data_offer, (struct wl_data_device * wl_data_device, struct wl_data_offer* id), (override));
    MOCK_METHOD(void, selection, (struct wl_data_device * wl_data_device, struct wl_data_offer* id), (override));
};

struct CCnPSink : Client
{
    using Client::Client;

    WlHandle<wl_data_device_manager> const manager{
        this->bind_if_supported<wl_data_device_manager>(AnyVersion)};
    DataDevice sink_data{wl_data_device_manager_get_data_device(manager, seat())};
    MockDataDeviceListener listener{sink_data};

    Surface create_surface_with_focus()
    {
        return Surface{create_visible_surface(any_width, any_height)};
    }
};
}

#endif
