/*
   Copyright (C) 2018 Red Hat, Inc.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/
/* Test that the macros to replace recorder works correctly */
#undef NDEBUG
#include <config.h>
#include <assert.h>

#undef ENABLE_RECORDER
#undef ENABLE_AGENT_INTERFACE

#include <common/recorder.h>

RECORDER(rec1, 32, "Desc rec1");
RECORDER_DECLARE(rec2);

RECORDER_TWEAK_DECLARE(tweak);

int main(void)
{
    // this should compile and link
    recorder_dump_on_common_signals(0, 0);

    // dummy traces should be always disabled
    if (RECORDER_TRACE(rec1)) {
        return 1;
    }

    // check tweak value
    assert(RECORDER_TWEAK(tweak) == 123);

    RECORD(rec2, "aaa %d", 1);

    RECORD_TIMING_BEGIN(rec2);
    RECORD_TIMING_END(rec2, "op", "name", 123);

    return 0;
}

RECORDER_DEFINE(rec2, 64, "Desc rec2");
RECORDER_TWEAK_DEFINE(tweak, 123, "Desc tweak");
