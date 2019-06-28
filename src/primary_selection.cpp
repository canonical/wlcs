/*
 * Copyright © 2019 Canonical Ltd.
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

#include "primary_selection.h"

#include <unistd.h>

wlcs::ActiveListeners wlcs::PrimarySelectionDeviceListener::active_listeners;
constexpr zwp_primary_selection_device_v1_listener wlcs::PrimarySelectionDeviceListener::thunks;

wlcs::ActiveListeners wlcs::PrimarySelectionOfferListener::active_listeners;
constexpr zwp_primary_selection_offer_v1_listener wlcs::PrimarySelectionOfferListener::thunks;

wlcs::ActiveListeners wlcs::PrimarySelectionSourceListener::active_listeners;
constexpr zwp_primary_selection_source_v1_listener wlcs::PrimarySelectionSourceListener::thunks;


void wlcs::PrimarySelectionDeviceListener::data_offer(
        void* data,
        zwp_primary_selection_device_v1* device,
        zwp_primary_selection_offer_v1* offer)
{
    if (active_listeners.includes(data))
        static_cast<PrimarySelectionDeviceListener*>(data)->data_offer(device, offer);
}

void wlcs::PrimarySelectionDeviceListener::data_offer(zwp_primary_selection_device_v1*, zwp_primary_selection_offer_v1*)
{
}

void wlcs::PrimarySelectionDeviceListener::selection(zwp_primary_selection_device_v1*, zwp_primary_selection_offer_v1*)
{
}

void wlcs::PrimarySelectionDeviceListener::selection(
        void* data,
        zwp_primary_selection_device_v1* device,
        zwp_primary_selection_offer_v1* offer)
{
    if (active_listeners.includes(data))
        static_cast<PrimarySelectionDeviceListener*>(data)->selection(device, offer);
}

void wlcs::PrimarySelectionOfferListener::offer(zwp_primary_selection_offer_v1*, const char*)
{
}

void wlcs::PrimarySelectionOfferListener::offer(
        void* data, zwp_primary_selection_offer_v1* offer, const char* mime_type)
{
    if (active_listeners.includes(data))
        static_cast<PrimarySelectionOfferListener*>(data)->offer(offer, mime_type);
}

void wlcs::PrimarySelectionSourceListener::send(zwp_primary_selection_source_v1*, const char*, int32_t fd)
{
    close(fd);
}

void wlcs::PrimarySelectionSourceListener::cancelled(zwp_primary_selection_source_v1*)
{
}

void wlcs::PrimarySelectionSourceListener::send(
        void* data, zwp_primary_selection_source_v1* source, const char* mime_type, int32_t fd)
{
    if (active_listeners.includes(data))
        static_cast<PrimarySelectionSourceListener*>(data)->send(source, mime_type, fd);
}

void wlcs::PrimarySelectionSourceListener::cancelled(void* data, zwp_primary_selection_source_v1* source)
{
    if (active_listeners.includes(data))
        static_cast<PrimarySelectionSourceListener*>(data)->cancelled(source);
}
