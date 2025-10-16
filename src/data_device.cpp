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
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#include "data_device.h"

wlcs::ActiveListeners wlcs::DataDeviceListener::active_listeners;
constexpr wl_data_device_listener wlcs::DataDeviceListener::thunks;

wlcs::DataSource::DataSource(struct wl_data_source* ds) :
    self{ds, deleter}
{
    static wl_data_source_listener constexpr listener{
        .target =
            [](auto...)
        {
        },
        .send =
            [](auto* data, auto, auto* mime_type, auto fd)
        {
            auto* self = reinterpret_cast<DataSource*>(data);
            self->wrote_data(mime_type, fd);
        },
        .cancelled =
            [](auto...)
        {
        },
        .dnd_drop_performed =
            [](auto...)
        {
        },
        .dnd_finished =
            [](auto...)
        {
        },
        .action =
            [](auto...)
        {
        },
    };
    wl_data_source_add_listener(ds, &listener, this);
}

void wlcs::DataDeviceListener::data_offer(void* data, struct wl_data_device* wl_data_device, struct wl_data_offer* id)
{
    if (active_listeners.includes(data))
        static_cast<DataDeviceListener*>(data)->data_offer(wl_data_device, id);
}

void wlcs::DataDeviceListener::enter(
    void* data,
    struct wl_data_device* wl_data_device,
    uint32_t serial,
    struct wl_surface* surface,
    wl_fixed_t x,
    wl_fixed_t y,
    struct wl_data_offer* id)
{
    if (active_listeners.includes(data))
        static_cast<DataDeviceListener*>(data)->enter(wl_data_device, serial, surface, x, y, id);
}

void wlcs::DataDeviceListener::leave(void* data, struct wl_data_device* wl_data_device)
{
    if (active_listeners.includes(data))
        static_cast<DataDeviceListener*>(data)->leave(wl_data_device);
}

void wlcs::DataDeviceListener::motion(
    void* data,
    struct wl_data_device* wl_data_device,
    uint32_t time,
    wl_fixed_t x,
    wl_fixed_t y)
{
    if (active_listeners.includes(data))
        static_cast<DataDeviceListener*>(data)->motion(wl_data_device, time, x, y);
}

void wlcs::DataDeviceListener::drop(void* data, struct wl_data_device* wl_data_device)
{
    if (active_listeners.includes(data))
        static_cast<DataDeviceListener*>(data)->drop(wl_data_device);
}

void wlcs::DataDeviceListener::selection(
    void* data,
    struct wl_data_device* wl_data_device,
    struct wl_data_offer* id)
{
    if (active_listeners.includes(data))
        static_cast<DataDeviceListener*>(data)->selection(wl_data_device, wl_data_device, id);
}

void wlcs::DataDeviceListener::data_offer(struct wl_data_device* /*wl_data_device*/, struct wl_data_offer* /*id*/)
{
}

void wlcs::DataDeviceListener::enter(
    struct wl_data_device* /*wl_data_device*/,
    uint32_t /*serial*/,
    struct wl_surface* /*surface*/,
    wl_fixed_t /*x*/,
    wl_fixed_t /*y*/,
    struct wl_data_offer* /*id*/)
{
}

void wlcs::DataDeviceListener::leave(struct wl_data_device* /*wl_data_device*/)
{
}

void wlcs::DataDeviceListener::motion(
    struct wl_data_device* /*wl_data_device*/,
    uint32_t /*time*/,
    wl_fixed_t /*x*/,
    wl_fixed_t /*y*/)
{
}

void wlcs::DataDeviceListener::drop(struct wl_data_device* /*wl_data_device*/)
{
}

void wlcs::DataDeviceListener::selection(
    struct wl_data_device* /*wl_data_device*/,
    struct wl_data_offer* /*id*/)
{
}


wlcs::ActiveListeners wlcs::DataOfferListener::active_listeners;
constexpr wl_data_offer_listener wlcs::DataOfferListener::thunks;

void wlcs::DataOfferListener::offer(void* data, struct wl_data_offer* data_offer, char const* mime_type)
{
    if (active_listeners.includes(data))
        static_cast<DataOfferListener*>(data)->offer(data_offer, mime_type);
}

void wlcs::DataOfferListener::source_actions(void* data, struct wl_data_offer* data_offer, uint32_t dnd_actions)
{
    if (active_listeners.includes(data))
        static_cast<DataOfferListener*>(data)->source_actions(data_offer, dnd_actions);
}

void wlcs::DataOfferListener::action(void* data, struct wl_data_offer* data_offer, uint32_t dnd_action)
{
    if (active_listeners.includes(data))
        static_cast<DataOfferListener*>(data)->action(data_offer, dnd_action);
}

void wlcs::DataOfferListener::offer(struct wl_data_offer* /*data_offer*/, char const* /*mime_type*/)
{
}

void wlcs::DataOfferListener::source_actions(struct wl_data_offer* /*data_offer*/, uint32_t /*dnd_actions*/)
{
}

void wlcs::DataOfferListener::action(struct wl_data_offer* /*data_offer*/, uint32_t /*dnd_action*/)
{
}
