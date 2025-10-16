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

#include "copy_cut_paste.h"


using namespace testing;
using namespace wlcs;

namespace
{

auto static const any_mime_type = "AnyMimeType";

struct MockDataOfferListener : DataOfferListener
{
    using DataOfferListener::DataOfferListener;

    MOCK_METHOD(void, offer, (struct wl_data_offer* data_offer, char const* MimeType), (override));
};

struct CopyCutPaste : StartedInProcessServer
{
    CCnPSource source{the_server()};
    CCnPSink sink{the_server()};

    MockDataOfferListener mdol;

    void TearDown() override
    {
        source.roundtrip();
        sink.roundtrip();
        StartedInProcessServer::TearDown();
    }
};
}

TEST_F(CopyCutPaste, given_source_has_offered_when_sink_gets_focus_it_sees_offer)
{
    source.offer(any_mime_type);

    EXPECT_CALL(mdol, offer(_, StrEq(any_mime_type)));
    EXPECT_CALL(sink.listener, data_offer(_,_))
        .WillOnce(Invoke([&](struct wl_data_device*, struct wl_data_offer* id)
        { mdol.listen_to(id); }));

    sink.create_surface_with_focus();
}

TEST_F(CopyCutPaste, given_sink_has_focus_when_source_makes_offer_sink_sees_offer)
{
    auto sink_surface_with_focus = sink.create_surface_with_focus();

    EXPECT_CALL(mdol, offer(_, StrEq(any_mime_type)));
    EXPECT_CALL(sink.listener, data_offer(_,_))
        .WillOnce(Invoke([&](struct wl_data_device*, struct wl_data_offer* id)
        { mdol.listen_to(id); }));

    source.offer(any_mime_type);
}
