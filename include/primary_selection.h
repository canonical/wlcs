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

#include "generated/primary-selection-unstable-v1-client.h"

#include "active_listeners.h"
#include "wl_interface_descriptor.h"
#include "wl_handle.h"

#include <memory>
#include <mutex>
#include <set>
#include <gmock/gmock.h>

#include <gmock/gmock.h>

namespace wlcs
{
WLCS_CREATE_INTERFACE_DESCRIPTOR(zwp_primary_selection_device_manager_v1)

    class PrimarySelectionSource
    {
    public:
        using WrappedType = zwp_primary_selection_source_v1;

        PrimarySelectionSource() = default;

        explicit PrimarySelectionSource(zwp_primary_selection_device_manager_v1* manager) :
                self{zwp_primary_selection_device_manager_v1_create_source(manager), deleter} {}

        operator WrappedType*() const { return self.get(); }

        void reset() { self.reset(); }

        void reset(WrappedType* source) { self.reset(source, deleter); }

        friend void zwp_primary_selection_source_v1_destroy(PrimarySelectionSource const&) = delete;

    private:
        static void deleter(WrappedType* source) { zwp_primary_selection_source_v1_destroy(source); }

        std::shared_ptr<WrappedType> self;
    };


    class PrimarySelectionDevice
    {
    public:
        using WrappedType = zwp_primary_selection_device_v1;

        PrimarySelectionDevice() = default;

        PrimarySelectionDevice(zwp_primary_selection_device_manager_v1* manager, wl_seat* seat) :
                self{zwp_primary_selection_device_manager_v1_get_device(manager, seat), deleter} {}

        operator WrappedType*() const { return self.get(); }

        void reset() { self.reset(); }

        void reset(WrappedType* device) { self.reset(device, deleter); }

        friend void zwp_primary_selection_device_v1_destroy(PrimarySelectionDevice const&) = delete;

    private:
        static void deleter(WrappedType* device) { zwp_primary_selection_device_v1_destroy(device); }

        std::shared_ptr<WrappedType> self;
    };

    struct PrimarySelectionDeviceListener
    {
        PrimarySelectionDeviceListener(zwp_primary_selection_device_v1* device)
        {
            active_listeners.add(this);
            zwp_primary_selection_device_v1_add_listener(device, &thunks, this);
        }

        virtual ~PrimarySelectionDeviceListener() { active_listeners.del(this); }

        PrimarySelectionDeviceListener(PrimarySelectionDeviceListener const&) = delete;
        PrimarySelectionDeviceListener& operator=(PrimarySelectionDeviceListener const&) = delete;

        virtual void data_offer(zwp_primary_selection_device_v1* device, zwp_primary_selection_offer_v1* offer);

        virtual void selection(zwp_primary_selection_device_v1* device, zwp_primary_selection_offer_v1* offer);

    private:
        static void data_offer(void* data, zwp_primary_selection_device_v1* device, zwp_primary_selection_offer_v1* offer);

        static void selection(void* data, zwp_primary_selection_device_v1* device, zwp_primary_selection_offer_v1* offer);

        static ActiveListeners active_listeners;
        constexpr static zwp_primary_selection_device_v1_listener thunks =
                {
                        &data_offer,
                        &selection
                };
    };

    struct PrimarySelectionOfferListener
    {
        PrimarySelectionOfferListener() { active_listeners.add(this); }
        virtual ~PrimarySelectionOfferListener() { active_listeners.del(this); }

        PrimarySelectionOfferListener(PrimarySelectionOfferListener const&) = delete;
        PrimarySelectionOfferListener& operator=(PrimarySelectionOfferListener const&) = delete;

        void listen_to(zwp_primary_selection_offer_v1* offer)
        {
            zwp_primary_selection_offer_v1_add_listener(offer, &thunks, this);
        }

        virtual void offer(zwp_primary_selection_offer_v1* offer, const char* mime_type);

    private:
        static void offer(void* data, zwp_primary_selection_offer_v1* offer, const char* mime_type);

        static ActiveListeners active_listeners;
        constexpr static zwp_primary_selection_offer_v1_listener thunks = { &offer, };
    };

    struct PrimarySelectionSourceListener
    {
        explicit PrimarySelectionSourceListener(PrimarySelectionSource const& source)
        {
            active_listeners.add(this);
            zwp_primary_selection_source_v1_add_listener(source, &thunks, this);
        }

        virtual ~PrimarySelectionSourceListener() { active_listeners.del(this); }

        PrimarySelectionSourceListener(PrimarySelectionSourceListener const&) = delete;
        PrimarySelectionSourceListener& operator=(PrimarySelectionSourceListener const&) = delete;

        virtual void send(zwp_primary_selection_source_v1* source, const char* mime_type, int32_t fd);
        virtual void cancelled(zwp_primary_selection_source_v1* source);

    private:
        static void send(void* data, zwp_primary_selection_source_v1* source, const char* mime_type, int32_t fd);
        static void cancelled(void* data, zwp_primary_selection_source_v1* source);

        static ActiveListeners active_listeners;
        constexpr static zwp_primary_selection_source_v1_listener thunks = { &send, &cancelled };
    };

    struct MockPrimarySelectionSourceListener : PrimarySelectionSourceListener
    {
        using PrimarySelectionSourceListener::PrimarySelectionSourceListener;

        MOCK_METHOD(
            void, send, (zwp_primary_selection_source_v1 * source, char const* mime_type, int32_t fd), (override));

        MOCK_METHOD(void, cancelled, (zwp_primary_selection_source_v1*), (override));
    };

    struct MockPrimarySelectionDeviceListener : PrimarySelectionDeviceListener
    {
        using PrimarySelectionDeviceListener::PrimarySelectionDeviceListener;

        MOCK_METHOD(
            void,
            data_offer,
            (zwp_primary_selection_device_v1 * device, zwp_primary_selection_offer_v1* offer),
            (override));
        MOCK_METHOD(
            void,
            selection,
            (zwp_primary_selection_device_v1 * device, zwp_primary_selection_offer_v1* offer),
            (override));
    };

    struct MockPrimarySelectionOfferListener : PrimarySelectionOfferListener
    {
        using PrimarySelectionOfferListener::PrimarySelectionOfferListener;

        MOCK_METHOD(void, offer, (zwp_primary_selection_offer_v1 * offer, char const* mime_type), (override));
    };
}

#endif //WLCS_PRIMARY_SELECTION_H
