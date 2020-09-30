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
/* Helper to test messages demarshalling using AFL (American Fuzzy Lop) fuzzer.
 *
 * To use you should:
 * 1- build enabling AFL. The usage of ElectricFence is to detect some
 *    additional possible buffer overflow, AFL required the program to crash
 *    in case of errors
 * $ make clean
 * $ make CC=afl-gcc CFLAGS='-O2 -fno-omit-frame-pointer' LDFLAGS='/usr/lib64/libefence.a'
 * 2- create a starting testcase, I used a simple file, you can create with
 * $ mkdir afl_testcases
 * $ perl -e 'print "\x02\x01\x00"' > afl_testcases/one
 * 3- run AFL, the export is to allow malloc(0), not an issue nowadays
 * $ export EF_ALLOW_MALLOC_0=1
 * $ mkdir afl_findings
 * $ afl-fuzz -i afl_testcases -o afl_findings -- ./tests/helper_fuzzer_demarshallers @@
 */
#undef NDEBUG
#include <config.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <common/demarshallers.h>

#ifdef _MSC_VER
typedef __int64 off_t;
#define ftello(f) _ftelli64((f))
#endif

typedef uint8_t *
spice_parse_t(uint8_t *message_start, uint8_t *message_end,
              uint32_t channel, uint16_t message_type, SPICE_GNUC_UNUSED int minor,
              size_t *size_out, message_destructor_t *free_message);

spice_parse_t spice_parse_msg, spice_parse_reply, spice_parse_msg_test;

int main(int argc, char **argv)
{
    // First argument filename of the testcase to handle
    if (argc < 2) {
        return 1;
    }

    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        return 1;
    }

    fseek(f, 0L, SEEK_END);
    off_t sz = ftello(f);
    assert(sz >= 0);
    rewind(f);

    // Testcase must have at least 3 bytes, 1 for channel, 2 for message type
    if (sz < 3) {
        fclose(f);
        return 1;
    }

#define READ(x) assert(fread(&x, 1, sizeof(x), f) == sizeof(x))

    uint8_t channel;
    READ(channel);

    // Low bits select client/server and test
    spice_parse_t *parse_func;
    switch (channel & 3) {
    case 0:
        parse_func = spice_parse_reply;
        break;
    case 1:
        parse_func = spice_parse_msg;
        break;
    case 3:
        parse_func = spice_parse_msg_test;
        break;
    default:
        fclose(f);
        return 1;
    }
    channel >>= 2;

    uint16_t type;
    READ(type);

    sz -= 3;

    // Read the rest of the file into a malloced buffer
    // Don't use GLib function, normal libc functions will be overwritten by
    // ElectricFence if you are using it
    uint8_t *p = malloc(sz);
    assert(fread(p, 1, sz, f) == (size_t) sz);
    fclose(f);
    f = NULL;

    // Parse the buffer as a SPICE message
    message_destructor_t release = NULL;
    size_t out_size;
    uint8_t *msg = parse_func(p, p + sz, channel, type, 0, &out_size, &release);
    if (msg && release) {
        release(msg);
    }

    free(p);

    return 0;
}
