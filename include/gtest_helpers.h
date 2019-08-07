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
 * Authored by: Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#include <chrono>
#include <ostream>
#include <iomanip>

namespace std
{
namespace chrono
{
/// GTest helper to pretty-print time-points
template<typename Clock>
void PrintTo(time_point<Clock> const& time, ostream* os)
{
    auto remainder = time.time_since_epoch();
    auto const hours = duration_cast<chrono::hours>(remainder);
    remainder -= hours;
    auto const minutes = duration_cast<chrono::minutes>(remainder);
    remainder -= minutes;
    auto const seconds = duration_cast<chrono::seconds>(remainder);
    remainder -= seconds;
    auto const nsec = duration_cast<chrono::nanoseconds>(remainder);
    (*os)
        << hours.count()
        << ":" << setw(2) << setfill('0') << minutes.count()
        << ":" << setw(2) << setfill('0') << seconds.count()
        << "." << setw(9) << setfill('0') << nsec.count();
}
}
}
