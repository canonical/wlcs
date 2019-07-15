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
 * Authored by: William Wold <william.wold@canonical.com>
 */

#include "helpers.h"
#include "in_process_server.h"
#include "layer_shell_v1.h"

#include <gmock/gmock.h>

using namespace testing;

using LayerShellTest = wlcs::InProcessServer;

TEST_F(LayerShellTest, supports_layer_shell_protocol)
{
    wlcs::Client client{the_server()};
    ASSERT_THAT(client.layer_shell_v1(), NotNull());
    wlcs::Surface surface{client};
    wlcs::LayerSurfaceV1 layer_surface{client, surface};
}

namespace
{

class LayerSurfaceTest: public wlcs::StartedInProcessServer
{
public:
    LayerSurfaceTest()
        : client{the_server()},
          surface{client},
          layer_surface{client, surface}
    {
    }

    void commit_and_wait_for_configure()
    {
        wl_surface_commit(surface);
        layer_surface.dispatch_until_configure();
    }

    auto output_size() const -> std::pair<int, int>
    {
        EXPECT_THAT(client.output_count(), Ge(1u)) << "There are no outputs to get a size from";
        EXPECT_THAT(client.output_count(), Eq(1u)) << "Unclear which output the layer shell surface will be placed on";
        auto output_state = client.output_state(0);
        EXPECT_THAT(output_state.mode_size, IsTrue()) << "Output has no size";
        auto size = output_state.mode_size.value();
        if (output_state.scale)
        {
            size.first /= output_state.scale.value();
            size.second /= output_state.scale.value();
        }
        EXPECT_THAT(size.first, Gt(1));
        EXPECT_THAT(size.second, Gt(1));
        return size;
    }

    auto last_width() const -> int { return layer_surface.last_width(); }
    auto last_height() const -> int { return layer_surface.last_height(); }

    wlcs::Client client;
    wlcs::Surface surface;
    wlcs::LayerSurfaceV1 layer_surface;
};

uint32_t const ANCHOR_ALL =
    ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
    ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT |
    ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
    ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;

}

TEST_F(LayerSurfaceTest, can_open_layer_surface)
{
    commit_and_wait_for_configure();
}

TEST_F(LayerSurfaceTest, by_default_gets_configured_without_size)
{
    commit_and_wait_for_configure();
    ASSERT_THAT(last_width(), Eq(0));
    ASSERT_THAT(last_height(), Eq(0));
}

TEST_F(LayerSurfaceTest, when_anchored_to_all_edges_gets_configured_with_output_size)
{
    zwlr_layer_surface_v1_set_anchor(layer_surface, ANCHOR_ALL);
    commit_and_wait_for_configure();
    auto size = output_size();
    ASSERT_THAT(last_width(), Eq(size.first));
    ASSERT_THAT(last_height(), Eq(size.second));
}

TEST_F(LayerSurfaceTest, gets_configured_after_anchor_change)
{
    commit_and_wait_for_configure();
    zwlr_layer_surface_v1_set_anchor(layer_surface, ANCHOR_ALL);
    commit_and_wait_for_configure();
    ASSERT_THAT(last_width(), Gt(0));
    ASSERT_THAT(last_height(), Gt(0));
}
