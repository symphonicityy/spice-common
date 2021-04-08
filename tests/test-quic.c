/*
   Copyright (C) 2017 Red Hat, Inc.

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

/* Test QUIC encoding and decoding. This test can also be used to fuzz the decoding.
 *
 * To use for the fuzzer you should:
 * 1- build enabling AFL.
 * $ make clean
 * $ make CC=afl-gcc CFLAGS='-O2 -fno-omit-frame-pointer'
 * 2- run AFL, the export is to use ElectricFence to detect some additional
 *    possible buffer overflow, AFL required the program to crash in case of errors
 * $ cd tests
 * $ mkdir afl_findings
 * $ export AFL_PRELOAD=/usr/lib64/libefence.so.0.0
 * $ afl-fuzz -i fuzzer-quic-testcases -o afl_findings -m 100 -- ./test_quic --fuzzer-decode @@
 */
#include <config.h>

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "common/quic.h"

typedef enum {
    COLOR_MODE_RGB,
    COLOR_MODE_RGB16,
    COLOR_MODE_GRAY,

    COLOR_MODE_END
} color_mode_t;

static color_mode_t color_mode = COLOR_MODE_RGB;
static bool fuzzying = false;

typedef struct {
    QuicUsrContext usr;
    GByteArray *dest;
} QuicData;

static SPICE_GNUC_NORETURN SPICE_GNUC_PRINTF(2, 3) void
quic_usr_error(QuicUsrContext *usr, const char *fmt, ...)
{
    if (fuzzying) {
        exit(1);
    }

    va_list ap;

    va_start(ap, fmt);
    g_logv(G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, fmt, ap);
    va_end(ap);

    g_assert_not_reached();
}

static SPICE_GNUC_PRINTF(2, 3) void
quic_usr_warn(QuicUsrContext *usr, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    g_logv(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, fmt, ap);
    va_end(ap);
}

static void *quic_usr_malloc(QuicUsrContext *usr, int size)
{
    return g_malloc(size);
}


static void quic_usr_free(QuicUsrContext *usr, void *ptr)
{
    g_free(ptr);
}

static int quic_usr_more_space_encode(QuicUsrContext *usr, uint32_t **io_ptr, int rows_completed)
{
    QuicData *quic_data = (QuicData *)usr;
    int initial_len = quic_data->dest->len;

    g_byte_array_set_size(quic_data->dest, quic_data->dest->len*2);

    *io_ptr = (uint32_t *)(quic_data->dest->data + initial_len);
    return (quic_data->dest->len - initial_len)/4;
}

static int quic_usr_more_space_decode(QuicUsrContext *usr, uint32_t **io_ptr, int rows_completed)
{
    // currently all data are passed at initialization, decoder are not expected to
    // read more data, beside during fuzzing, in this case quic_usr_error will
    // be called
    return 0;
}


static int quic_usr_more_lines(QuicUsrContext *usr, uint8_t **lines)
{
    g_return_val_if_reached(0);
}


static void init_quic_data(QuicData *quic_data, bool encode)
{
    quic_data->usr.error = quic_usr_error;
    quic_data->usr.warn = quic_usr_warn;
    quic_data->usr.info = quic_usr_warn;
    quic_data->usr.malloc = quic_usr_malloc;
    quic_data->usr.free = quic_usr_free;
    quic_data->usr.more_space = encode ? quic_usr_more_space_encode : quic_usr_more_space_decode;
    quic_data->usr.more_lines = quic_usr_more_lines;
    quic_data->dest = g_byte_array_new();
}

// RGB luminosity (sum 256) 54, 184, 18
static inline uint8_t pixel_to_gray(uint8_t r, uint8_t g, uint8_t b)
{
    return (54u * r + 184u * g + 18u * b) / 256u;
}

static inline void gray_to_pixel(uint8_t gray, uint8_t *p)
{
    p[0] = p[1] = p[2] = gray;
}

static inline uint16_t pixel_to_rgb16(uint8_t r, uint8_t g, uint8_t b)
{
    r = (r >> 3) & 0x1Fu;
    g = (g >> 3) & 0x1Fu;
    b = (b >> 3) & 0x1Fu;
    return r * (32u*32u) + g * 32u + b;
}

static inline void rgb16_to_pixel(uint16_t color, uint8_t *p)
{
    uint8_t comp;
    comp = (color >> 10) & 0x1Fu;
    *p++ = (comp << 3) | (comp >> 2);
    color <<= 5;
    comp = (color >> 10) & 0x1Fu;
    *p++ = (comp << 3) | (comp >> 2);
    color <<= 5;
    comp = (color >> 10) & 0x1Fu;
    *p++ = (comp << 3) | (comp >> 2);
}

#define CONVERT_PROC(TYPE, FUNC) \
static void pixbuf_convert_to_##FUNC(GdkPixbuf *pixbuf, TYPE *dest_line, int dest_stride) \
{ \
    int width = gdk_pixbuf_get_width(pixbuf); \
    int height = gdk_pixbuf_get_height(pixbuf); \
    int n_channels = gdk_pixbuf_get_n_channels (pixbuf); \
    int stride = gdk_pixbuf_get_rowstride(pixbuf); \
    uint8_t *line = gdk_pixbuf_get_pixels(pixbuf); \
 \
    for (int y = 0; y < height; y++) { \
        uint8_t *p = line; \
        TYPE *dest = dest_line; \
        for (int x = 0; x < width; x++) { \
 \
            *dest = pixel_to_##FUNC(p[0], p[1], p[2]); \
            FUNC##_to_pixel(*dest, p); \
            ++dest; \
            p += n_channels; \
        } \
        line += stride; \
        dest_line = (TYPE*)((char*) dest_line + dest_stride); \
    } \
}

CONVERT_PROC(uint8_t, gray)
CONVERT_PROC(uint16_t, rgb16)

#define UNCONVERT_PROC(TYPE, FUNC) \
static void pixbuf_unconvert_to_##FUNC(GdkPixbuf *pixbuf) \
{ \
    const int width = gdk_pixbuf_get_width(pixbuf); \
    const int height = gdk_pixbuf_get_height(pixbuf); \
    const int n_channels = gdk_pixbuf_get_n_channels(pixbuf); \
    const int stride = gdk_pixbuf_get_rowstride(pixbuf); \
    uint8_t *line = gdk_pixbuf_get_pixels(pixbuf) + stride*height; \
 \
    for (int y = 0; y < height; y++) { \
        line -= stride; \
        uint8_t *p = line + width*n_channels; \
        const TYPE *dest = (TYPE*) line + width; \
        for (int x = 0; x < width; x++) { \
            --dest; \
            p -= n_channels; \
            FUNC##_to_pixel(*dest, p); \
            if (n_channels == 4) { \
                p[3] = 255; \
            } \
        } \
    } \
    g_assert(line == gdk_pixbuf_get_pixels(pixbuf)); \
}

UNCONVERT_PROC(uint8_t, gray)
UNCONVERT_PROC(uint16_t, rgb16)

typedef struct {
    QuicImageType quic_type;
    uint8_t *pixels;
    int stride;
    GdkPixbuf *pixbuf;
} ImageBuf;

static ImageBuf *image_buf_init(ImageBuf *imgbuf, GdkPixbuf *pixbuf)
{
    imgbuf->pixbuf = pixbuf;
    switch (gdk_pixbuf_get_n_channels(pixbuf)) {
        case 3:
            imgbuf->quic_type = QUIC_IMAGE_TYPE_RGB24;
            break;
        case 4:
            imgbuf->quic_type = QUIC_IMAGE_TYPE_RGBA;
            break;
        default:
            g_assert_not_reached();
    }
    imgbuf->pixels = gdk_pixbuf_get_pixels(pixbuf);
    imgbuf->stride = gdk_pixbuf_get_rowstride(pixbuf);

    if (color_mode == COLOR_MODE_GRAY) {
        int stride = gdk_pixbuf_get_width(pixbuf);
        uint8_t *pixels = g_malloc(stride * gdk_pixbuf_get_height(pixbuf));
        pixbuf_convert_to_gray(pixbuf, pixels, stride);
        imgbuf->stride = stride;
        imgbuf->pixels = pixels;
        imgbuf->quic_type = QUIC_IMAGE_TYPE_GRAY;
    } else if (color_mode == COLOR_MODE_RGB16) {
        int stride = gdk_pixbuf_get_width(pixbuf)*2;
        uint16_t *pixels = g_malloc(stride * gdk_pixbuf_get_height(pixbuf));
        pixbuf_convert_to_rgb16(pixbuf, pixels, stride);
        imgbuf->stride = stride;
        imgbuf->pixels = (uint8_t*)pixels;
        imgbuf->quic_type = QUIC_IMAGE_TYPE_RGB16;
    }

    return imgbuf;
}

static void image_buf_free(ImageBuf *imgbuf, GdkPixbuf *pixbuf)
{
    if (imgbuf->quic_type == QUIC_IMAGE_TYPE_GRAY) {
        pixbuf_unconvert_to_gray(pixbuf);
    }

    if (imgbuf->quic_type == QUIC_IMAGE_TYPE_RGB16) {
        pixbuf_unconvert_to_rgb16(pixbuf);
    }

    if (imgbuf->pixels != gdk_pixbuf_get_pixels(imgbuf->pixbuf)) {
        g_free(imgbuf->pixels);
    }
}

static GByteArray *quic_encode_from_pixbuf(GdkPixbuf *pixbuf, const ImageBuf *imgbuf)
{
    QuicData quic_data;
    QuicContext *quic;
    int encoded_size;

    init_quic_data(&quic_data, true);
    g_byte_array_set_size(quic_data.dest, 1024);

    quic = quic_create(&quic_data.usr);
    g_assert(quic != NULL);
    encoded_size = quic_encode(quic, imgbuf->quic_type,
                               gdk_pixbuf_get_width(pixbuf),
                               gdk_pixbuf_get_height(pixbuf),
                               imgbuf->pixels,
                               gdk_pixbuf_get_height(pixbuf),
                               imgbuf->stride,
                               (uint32_t *)quic_data.dest->data,
                               quic_data.dest->len/sizeof(uint32_t));
    g_assert(encoded_size > 0);
    encoded_size *= 4;
    g_byte_array_set_size(quic_data.dest, encoded_size);
    quic_destroy(quic);

    return quic_data.dest;
}

static GdkPixbuf *quic_decode_to_pixbuf(GByteArray *compressed_data)
{
    QuicData quic_data;
    QuicContext *quic;
    GdkPixbuf *pixbuf;
    QuicImageType type;
    int width;
    int height;
    int status;

    init_quic_data(&quic_data, false);
    g_byte_array_free(quic_data.dest, TRUE);
    quic_data.dest = NULL;

    quic = quic_create(&quic_data.usr);
    g_assert(quic != NULL);

    status = quic_decode_begin(quic,
                               (uint32_t *)compressed_data->data, compressed_data->len/4,
                               &type, &width, &height);
    /* limit size for fuzzer, he restrict virtual memory */
    if (fuzzying && (status != QUIC_OK || (width * height) > 16 * 1024 * 1024 / 4)) {
        exit(1);
    }
    g_assert(status == QUIC_OK);

    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB,
                            (type == QUIC_IMAGE_TYPE_RGBA || type == QUIC_IMAGE_TYPE_RGB32), 8,
                            width, height);
    status = quic_decode(quic, type,
                         gdk_pixbuf_get_pixels(pixbuf),
                         gdk_pixbuf_get_rowstride(pixbuf));
    g_assert(status == QUIC_OK);
    quic_destroy(quic);

    return pixbuf;
}

static void pixbuf_compare(GdkPixbuf *pixbuf_a, GdkPixbuf *pixbuf_b)
{
    int width = gdk_pixbuf_get_width(pixbuf_a);
    int height = gdk_pixbuf_get_height(pixbuf_a);
    int n_channels_a = gdk_pixbuf_get_n_channels(pixbuf_a);
    int n_channels_b = gdk_pixbuf_get_n_channels(pixbuf_b);
    int x;
    int y;
    guint8 *pixels_a = gdk_pixbuf_get_pixels(pixbuf_a);
    guint8 *pixels_b = gdk_pixbuf_get_pixels(pixbuf_b);
    bool check_alpha = gdk_pixbuf_get_has_alpha(pixbuf_a);

    g_assert(width == gdk_pixbuf_get_width(pixbuf_b));
    g_assert(height == gdk_pixbuf_get_height(pixbuf_b));
    if (color_mode != COLOR_MODE_RGB) {
        check_alpha = false;
    } else {
        g_assert(n_channels_a == n_channels_b);
        g_assert(gdk_pixbuf_get_byte_length(pixbuf_a) == gdk_pixbuf_get_byte_length(pixbuf_b));
    }
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            guint8 *p_a = pixels_a + y*gdk_pixbuf_get_rowstride(pixbuf_a) + x*n_channels_a;
            guint8 *p_b = pixels_b + y*gdk_pixbuf_get_rowstride(pixbuf_b) + x*n_channels_b;

            g_assert(p_a[0] == p_b[0]);
            g_assert(p_a[1] == p_b[1]);
            g_assert(p_a[2] == p_b[2]);
            if (check_alpha) {
                g_assert(p_a[3] == p_b[3]);
            }
        }
    }
}

static GdkPixbuf *pixbuf_new_random(int alpha, bool fixed)
{
    gboolean has_alpha = alpha >= 0 ? alpha : g_random_boolean();
    gint width = g_random_int_range(100, 2000);
    gint height = g_random_int_range(100, 500);
    GdkPixbuf *random_pixbuf;
    guint i, size;
    guint8 *pixels;

    random_pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, has_alpha, 8, width, height);
    pixels = gdk_pixbuf_get_pixels(random_pixbuf);
    size = gdk_pixbuf_get_byte_length(random_pixbuf);
    for (i = 0; i < size; i++) {
        pixels[i] = fixed ? (gint)((i%(has_alpha?4:3))) << 5 : g_random_int_range(0, 256);
    }

    return random_pixbuf;
}

static void test_pixbuf(GdkPixbuf *pixbuf)
{
    GdkPixbuf *uncompressed_pixbuf;
    GByteArray *compressed_data;
    g_assert(pixbuf != NULL);
    g_assert(gdk_pixbuf_get_colorspace(pixbuf) == GDK_COLORSPACE_RGB);
    g_assert(gdk_pixbuf_get_bits_per_sample(pixbuf) == 8);

    ImageBuf imgbuf[1];
    image_buf_init(imgbuf, pixbuf);
    compressed_data = quic_encode_from_pixbuf(pixbuf, imgbuf);

    uncompressed_pixbuf = quic_decode_to_pixbuf(compressed_data);
    image_buf_free(imgbuf, uncompressed_pixbuf);

    //g_assert(memcmp(gdk_pixbuf_get_pixels(pixbuf), gdk_pixbuf_get_pixels(uncompressed_pixbuf), gdk_pixbuf_get_byte_length(uncompressed_pixbuf)));
    pixbuf_compare(pixbuf, uncompressed_pixbuf);

    g_byte_array_free(compressed_data, TRUE);
    g_object_unref(uncompressed_pixbuf);

}

static int
fuzzer_decode(const char *fn)
{
    GdkPixbuf *uncompressed_pixbuf;
    GByteArray compressed_data[1];
    gchar *contents = NULL;
    gsize length;

    fuzzying = true;
    if (!g_file_get_contents(fn, &contents, &length, NULL)) {
        exit(1);
    }
    compressed_data->data = (void*) contents;
    compressed_data->len = length;
    uncompressed_pixbuf = quic_decode_to_pixbuf(compressed_data);

    g_object_unref(uncompressed_pixbuf);
    g_free(contents);

    return 0;
}

int main(int argc, char **argv)
{
    if (argc >= 3 && strcmp(argv[1], "--fuzzer-decode") == 0) {
        return fuzzer_decode(argv[2]);
    }

    if (argc >= 2) {
        for (int i = 1; i < argc; ++i) {
            GdkPixbuf *source_pixbuf;

            source_pixbuf = gdk_pixbuf_new_from_file(argv[i], NULL);
            test_pixbuf(source_pixbuf);
            g_object_unref(source_pixbuf);
        }
    } else if (argc == 1) {
        int test;
        for (test = 0; test < 4; test++) {
            int alpha = test % 2;
            bool fixed = (test / 2) % 2;

            for (color_mode = COLOR_MODE_RGB; color_mode < COLOR_MODE_END; color_mode++) {
                /* alpha affects only COLOR_MODE_RGB more, reduce number of tests */
                if (color_mode != COLOR_MODE_RGB && alpha) {
                    continue;
                }
                GdkPixbuf *pixbuf = pixbuf_new_random(alpha, fixed);
                test_pixbuf(pixbuf);
                g_object_unref(pixbuf);
            }
        }
    } else {
        g_assert_not_reached();
    }

    return 0;
}
