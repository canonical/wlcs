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

#ifndef WLCS_DATA_DEVICE_H
#define WLCS_DATA_DEVICE_H

#include "active_listeners.h"

#include <wayland-client.h>

#include <memory>

namespace wlcs
{
class DataSource
{
public:
    DataSource() = default;

    explicit DataSource(struct wl_data_source* ds) : self{ds, deleter} {}

    operator struct wl_data_source*() const { return self.get(); }

    void reset() { self.reset(); }

    void reset(struct wl_data_source* ds) { self.reset(ds, deleter); }

    friend void wl_data_source_destroy(DataSource const&) = delete;

private:
    static void deleter(struct wl_data_source* ds) { wl_data_source_destroy(ds); }

    std::shared_ptr<struct wl_data_source> self;
};

struct DataDeviceListener
{
    DataDeviceListener(struct wl_data_device* data_device)
    {
        active_listeners.add(this);
        wl_data_device_add_listener(data_device, &thunks, this);
    }

    virtual ~DataDeviceListener() { active_listeners.del(this); }

    DataDeviceListener(DataDeviceListener const&) = delete;
    DataDeviceListener& operator=(DataDeviceListener const&) = delete;

    virtual void data_offer(
        struct wl_data_device* wl_data_device,
        struct wl_data_offer* id);

    virtual void enter(
        struct wl_data_device* wl_data_device,
        uint32_t serial,
        struct wl_surface* surface,
        wl_fixed_t x,
        wl_fixed_t y,
        struct wl_data_offer* id);

    virtual void leave(struct wl_data_device* wl_data_device);

    virtual void motion(
        struct wl_data_device* wl_data_device,
        uint32_t time,
        wl_fixed_t x,
        wl_fixed_t y);

    virtual void drop(struct wl_data_device* wl_data_device);

    virtual void selection(
        struct wl_data_device* wl_data_device,
        struct wl_data_offer* id);

private:
    static void data_offer(
        void* data,
        struct wl_data_device* wl_data_device,
        struct wl_data_offer* id);

    static void enter(
        void* data,
        struct wl_data_device* wl_data_device,
        uint32_t serial,
        struct wl_surface* surface,
        wl_fixed_t x,
        wl_fixed_t y,
        struct wl_data_offer* id);

    static void leave(void* data, struct wl_data_device* wl_data_device);

    static void motion(
        void* data,
        struct wl_data_device* wl_data_device,
        uint32_t time,
        wl_fixed_t x,
        wl_fixed_t y);

    static void drop(void* data, struct wl_data_device* wl_data_device);

    static void selection(
        void* data,
        struct wl_data_device* wl_data_device,
        struct wl_data_offer* id);


    static ActiveListeners active_listeners;
    constexpr static wl_data_device_listener thunks =
        {
            &data_offer,
            &enter,
            &leave,
            &motion,
            &drop,
            &selection
        };
};

struct DataOfferListener
{
    DataOfferListener() { active_listeners.add(this); }
    virtual ~DataOfferListener() { active_listeners.del(this); }

    DataOfferListener(DataOfferListener const&) = delete;
    DataOfferListener& operator=(DataOfferListener const&) = delete;

    void listen_to(struct wl_data_offer* data_offer)
    {
        wl_data_offer_add_listener(data_offer, &thunks, this);
    }

    virtual void offer(struct wl_data_offer* data_offer, char const* mime_type);

    virtual void source_actions(struct wl_data_offer* data_offer, uint32_t dnd_actions);

    virtual void action(struct wl_data_offer* data_offer, uint32_t dnd_action);

private:
    static void offer(void* data, struct wl_data_offer* data_offer, char const* mime_type);

    static void source_actions(void* data, struct wl_data_offer* data_offer, uint32_t dnd_actions);

    static void action(void* data, struct wl_data_offer* data_offer, uint32_t dnd_action);

    static ActiveListeners active_listeners;
    constexpr static wl_data_offer_listener thunks =
        {
            &offer,
            &source_actions,
            &action
        };
};

class DataDevice
{
public:
    DataDevice() = default;

    explicit DataDevice(struct wl_data_device* dd) : self{dd, deleter} {}

    operator struct wl_data_device*() const { return self.get(); }

    void reset() { self.reset(); }

    void reset(struct wl_data_device* dd) { self.reset(dd, deleter); }

    friend void wl_data_device_destroy(DataDevice const&) = delete;

private:
    static void deleter(struct wl_data_device* dd) { wl_data_device_destroy(dd); }

    std::shared_ptr<struct wl_data_device> self;
};
}

#endif //WLCS_DATA_DEVICE_H
