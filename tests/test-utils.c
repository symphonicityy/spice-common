/*
   Copyright (C) 2020 Red Hat, Inc.

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
/* Test that tests some utils.h functions */
#undef NDEBUG
#include <config.h>
#include <assert.h>
#include <string.h>

#include <common/utils.h>

int main(void)
{
    unsigned int i;
    uint8_t bytes[64];

    memset(bytes, 0, sizeof(bytes));

    // some early bytes
    set_bitmap(10, bytes);
    assert(bytes[1] == 0x4);

    assert(test_bitmap(10, bytes));
    assert(!test_bitmap(12, bytes));

    set_bitmap(12, bytes);
    assert(bytes[1] == 0x14);

    assert(test_bitmap(10, bytes));
    assert(test_bitmap(12, bytes));

    // some higher bytes, to check truncation
    assert(!test_bitmap(363, bytes));
    assert(!test_bitmap(367, bytes));

    set_bitmap(367, bytes);
    assert(!test_bitmap(363, bytes));
    assert(test_bitmap(367, bytes));

    set_bitmap(363, bytes);
    assert(test_bitmap(363, bytes));
    assert(test_bitmap(367, bytes));

    // check all bytes are zeroes except one set above
    bytes[1] = 0;
    bytes[45] = 0;
    for (i = 0; i < sizeof(bytes); ++i) {
        assert(bytes[i] == 0);
    }

    return 0;
}
