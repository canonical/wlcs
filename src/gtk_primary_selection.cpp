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

#include "gtk_primary_selection.h"

#include <unistd.h>

wlcs::ActiveListeners wlcs::GtkPrimarySelectionDeviceListener::active_listeners;
constexpr gtk_primary_selection_device_listener wlcs::GtkPrimarySelectionDeviceListener::thunks;

wlcs::ActiveListeners wlcs::GtkPrimarySelectionOfferListener::active_listeners;
constexpr gtk_primary_selection_offer_listener wlcs::GtkPrimarySelectionOfferListener::thunks;

wlcs::ActiveListeners wlcs::GtkPrimarySelectionSourceListener::active_listeners;
constexpr gtk_primary_selection_source_listener wlcs::GtkPrimarySelectionSourceListener::thunks;


void wlcs::GtkPrimarySelectionDeviceListener::data_offer(
        void* data,
        gtk_primary_selection_device* device,
        gtk_primary_selection_offer* offer)
{
    if (active_listeners.includes(data))
        static_cast<GtkPrimarySelectionDeviceListener*>(data)->data_offer(device, offer);
}

void wlcs::GtkPrimarySelectionDeviceListener::data_offer(gtk_primary_selection_device*, gtk_primary_selection_offer*)
{
}

void wlcs::GtkPrimarySelectionDeviceListener::selection(gtk_primary_selection_device*, gtk_primary_selection_offer*)
{
}

void wlcs::GtkPrimarySelectionDeviceListener::selection(
        void* data,
        gtk_primary_selection_device* device,
        gtk_primary_selection_offer* offer)
{
    if (active_listeners.includes(data))
        static_cast<GtkPrimarySelectionDeviceListener*>(data)->selection(device, offer);
}

void wlcs::GtkPrimarySelectionOfferListener::offer(gtk_primary_selection_offer*, const char*)
{
}

void wlcs::GtkPrimarySelectionOfferListener::offer(
        void* data, gtk_primary_selection_offer* offer, const char* mime_type)
{
    if (active_listeners.includes(data))
        static_cast<GtkPrimarySelectionOfferListener*>(data)->offer(offer, mime_type);
}

void wlcs::GtkPrimarySelectionSourceListener::send(gtk_primary_selection_source*, const char*, int32_t fd)
{
    close(fd);
}

void wlcs::GtkPrimarySelectionSourceListener::cancelled(gtk_primary_selection_source*)
{
}

void wlcs::GtkPrimarySelectionSourceListener::send(
        void* data, gtk_primary_selection_source* source, const char* mime_type, int32_t fd)
{
    if (active_listeners.includes(data))
        static_cast<GtkPrimarySelectionSourceListener*>(data)->send(source, mime_type, fd);
}

void wlcs::GtkPrimarySelectionSourceListener::cancelled(void* data, gtk_primary_selection_source* source)
{
    if (active_listeners.includes(data))
        static_cast<GtkPrimarySelectionSourceListener*>(data)->cancelled(source);
}
