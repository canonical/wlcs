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

#ifndef WLCS_PRIMARY_SELECTION_H
#define WLCS_PRIMARY_SELECTION_H

#include "generated/gtk-primary-selection-client.h"

#include "active_listeners.h"

#include <memory>
#include <mutex>
#include <set>

namespace wlcs
{
    class GtkPrimarySelectionSource
    {
    public:
        using WrappedType = gtk_primary_selection_source;

        GtkPrimarySelectionSource() = default;

        explicit GtkPrimarySelectionSource(gtk_primary_selection_device_manager* manager) :
                self{gtk_primary_selection_device_manager_create_source(manager), deleter} {}

        operator WrappedType*() const { return self.get(); }

        void reset() { self.reset(); }

        void reset(WrappedType* source) { self.reset(source, deleter); }

        friend void gtk_primary_selection_source_destroy(GtkPrimarySelectionSource const&) = delete;

    private:
        static void deleter(WrappedType* source) { gtk_primary_selection_source_destroy(source); }

        std::shared_ptr<WrappedType> self;
    };


    class GtkPrimarySelectionDevice
    {
    public:
        using WrappedType = gtk_primary_selection_device;

        GtkPrimarySelectionDevice() = default;

        GtkPrimarySelectionDevice(gtk_primary_selection_device_manager* manager, wl_seat* seat) :
                self{gtk_primary_selection_device_manager_get_device(manager, seat), deleter} {}

        operator WrappedType*() const { return self.get(); }

        void reset() { self.reset(); }

        void reset(WrappedType* device) { self.reset(device, deleter); }

        friend void gtk_primary_selection_device_destroy(GtkPrimarySelectionDevice const&) = delete;

    private:
        static void deleter(WrappedType* device) { gtk_primary_selection_device_destroy(device); }

        std::shared_ptr<WrappedType> self;
    };

    struct GtkPrimarySelectionDeviceListener
    {
        GtkPrimarySelectionDeviceListener(gtk_primary_selection_device* device)
        {
            active_listeners.add(this);
            gtk_primary_selection_device_add_listener(device, &thunks, this);
        }

        virtual ~GtkPrimarySelectionDeviceListener() { active_listeners.del(this); }

        GtkPrimarySelectionDeviceListener(GtkPrimarySelectionDeviceListener const&) = delete;
        GtkPrimarySelectionDeviceListener& operator=(GtkPrimarySelectionDeviceListener const&) = delete;

        virtual void data_offer(gtk_primary_selection_device* device, gtk_primary_selection_offer* offer);

        virtual void selection(gtk_primary_selection_device* device, gtk_primary_selection_offer* offer);

    private:
        static void data_offer(void* data, gtk_primary_selection_device* device, gtk_primary_selection_offer* offer);

        static void selection(void* data, gtk_primary_selection_device* device, gtk_primary_selection_offer* offer);

        static ActiveListeners active_listeners;
        constexpr static gtk_primary_selection_device_listener thunks =
                {
                        &data_offer,
                        &selection
                };
    };

    struct GtkPrimarySelectionOfferListener
    {
        GtkPrimarySelectionOfferListener() { active_listeners.add(this); }
        virtual ~GtkPrimarySelectionOfferListener() { active_listeners.del(this); }

        GtkPrimarySelectionOfferListener(GtkPrimarySelectionOfferListener const&) = delete;
        GtkPrimarySelectionOfferListener& operator=(GtkPrimarySelectionOfferListener const&) = delete;

        void listen_to(gtk_primary_selection_offer* offer)
        {
            gtk_primary_selection_offer_add_listener(offer, &thunks, this);
        }

        virtual void offer(gtk_primary_selection_offer* offer, const char* mime_type);

    private:
        static void offer(void* data, gtk_primary_selection_offer* offer, const char* mime_type);

        static ActiveListeners active_listeners;
        constexpr static gtk_primary_selection_offer_listener thunks = { &offer, };
    };

    struct GtkPrimarySelectionSourceListener
    {
        explicit GtkPrimarySelectionSourceListener(GtkPrimarySelectionSource const& source)
        {
            active_listeners.add(this);
            gtk_primary_selection_source_add_listener(source, &thunks, this);
        }

        virtual ~GtkPrimarySelectionSourceListener() { active_listeners.del(this); }

        GtkPrimarySelectionSourceListener(GtkPrimarySelectionSourceListener const&) = delete;
        GtkPrimarySelectionSourceListener& operator=(GtkPrimarySelectionSourceListener const&) = delete;

        virtual void send(gtk_primary_selection_source* source, const char* mime_type, int32_t fd);
        virtual void cancelled(gtk_primary_selection_source* source);

    private:
        static void send(void* data, gtk_primary_selection_source* source, const char* mime_type, int32_t fd);
        static void cancelled(void* data, gtk_primary_selection_source* source);

        static ActiveListeners active_listeners;
        constexpr static gtk_primary_selection_source_listener thunks = { &send, &cancelled };
    };
}

#endif //WLCS_PRIMARY_SELECTION_H
