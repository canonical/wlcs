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

#include "helpers.h"
#include "in_process_server.h"

#include <gmock/gmock.h>

#include <memory>

using namespace testing;
using namespace wlcs;

namespace
{
struct StartedInProcessServer : InProcessServer
{
    StartedInProcessServer() { InProcessServer::SetUp(); }

    void SetUp() override {}
};

auto static const any_width = 100;
auto static const any_height = 100;
auto static const any_mime_type = "AnyMimeType";

struct CopyCutPasteClient : Client
{
    using Client::Client;
    Surface surface = create_visible_surface(any_width, any_height);
};

struct CopyCutPaste : StartedInProcessServer
{
    CopyCutPasteClient source{the_server()};
    CopyCutPasteClient sink{the_server()};
};

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
}

TEST_F(CopyCutPaste, given_data_offer)
{
    DataSource source_data{wl_data_device_manager_create_data_source(source.data_device_manager())};

    wl_data_source_offer(source_data, any_mime_type);
}