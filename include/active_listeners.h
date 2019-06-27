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

#ifndef WLCS_ACTIVE_LISTENERS_H
#define WLCS_ACTIVE_LISTENERS_H

#include <mutex>
#include <set>

namespace wlcs
{
class ActiveListeners
{
public:
    void add(void* listener)
    {
        std::lock_guard<decltype(mutex)> lock{mutex};
        listeners.insert(listener);
    }

    void del(void* listener)
    {
        std::lock_guard<decltype(mutex)> lock{mutex};
        listeners.erase(listener);
    }

    bool includes(void* listener) const
    {
        std::lock_guard<decltype(mutex)> lock{mutex};
        return listeners.find(listener) != end(listeners);
    }

private:
    std::mutex mutable mutex;
    std::set<void*> listeners;
};
}

#endif //WLCS_ACTIVE_LISTENERS_H
