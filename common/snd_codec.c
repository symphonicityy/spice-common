/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2013 Jeremy White

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

/* snd_codec.c
     General purpose sound codec routines for use by Spice.
   These routines abstract the work of picking a codec and
   encoding and decoding the buffers.

   See below for documentation of the public routines.
*/

#include "config.h"
#include <stdio.h>
#include <string.h>

#if HAVE_OPUS
#include  <opus.h>
#endif

#include <spice/macros.h>
#include <spice/enums.h>

#include "snd_codec.h"
#include "mem.h"
#include "log.h"

typedef struct SndCodecInternal
{
    SpiceAudioDataMode mode;
    int frequency;

#if HAVE_OPUS
    OpusEncoder *opus_encoder;
    OpusDecoder *opus_decoder;
#endif
} SndCodecInternal;


/* Opus support routines */
#if HAVE_OPUS
static void snd_codec_destroy_opus(SndCodecInternal *codec)
{
    if (codec->opus_decoder) {
        opus_decoder_destroy(codec->opus_decoder);
        codec->opus_decoder = NULL;
    }

    if (codec->opus_encoder) {
        opus_encoder_destroy(codec->opus_encoder);
        codec->opus_encoder = NULL;
    }

}

static SndCodecResult snd_codec_create_opus(SndCodecInternal *codec, int purpose)
{
    int opus_error;

    if (purpose & SND_CODEC_ENCODE) {
        codec->opus_encoder = opus_encoder_create(codec->frequency,
                                SND_CODEC_PLAYBACK_CHAN,
                                OPUS_APPLICATION_AUDIO, &opus_error);
        if (! codec->opus_encoder) {
            g_warning("create opus encoder failed; error %d", opus_error);
            goto error;
        }
    }

    if (purpose & SND_CODEC_DECODE) {
        codec->opus_decoder = opus_decoder_create(codec->frequency,
                                SND_CODEC_PLAYBACK_CHAN, &opus_error);
        if (! codec->opus_decoder) {
            g_warning("create opus decoder failed; error %d", opus_error);
            goto error;
        }
    }

    codec->mode = SPICE_AUDIO_DATA_MODE_OPUS;
    return SND_CODEC_OK;

error:
    snd_codec_destroy_opus(codec);
    return SND_CODEC_UNAVAILABLE;
}

static SndCodecResult
snd_codec_encode_opus(SndCodecInternal *codec, uint8_t *in_ptr, int in_size,
                      uint8_t *out_ptr, int *out_size)
{
    int n;
    if (in_size != SND_CODEC_OPUS_FRAME_SIZE * SND_CODEC_PLAYBACK_CHAN * 2)
        return SND_CODEC_INVALID_ENCODE_SIZE;
    n = opus_encode(codec->opus_encoder, (opus_int16 *) in_ptr, SND_CODEC_OPUS_FRAME_SIZE, out_ptr, *out_size);
    if (n < 0) {
        g_warning("opus_encode failed %d", n);
        return SND_CODEC_ENCODE_FAILED;
    }
    *out_size = n;
    return SND_CODEC_OK;
}

static SndCodecResult
snd_codec_decode_opus(SndCodecInternal *codec, uint8_t *in_ptr, int in_size,
                      uint8_t *out_ptr, int *out_size)
{
    int n;
    n = opus_decode(codec->opus_decoder, in_ptr, in_size, (opus_int16 *) out_ptr,
                *out_size / SND_CODEC_PLAYBACK_CHAN / 2, 0);
    if (n < 0) {
        g_warning("opus_decode failed %d", n);
        return SND_CODEC_DECODE_FAILED;
    }
    *out_size = n * SND_CODEC_PLAYBACK_CHAN * 2 /* 16 fmt */;
    return SND_CODEC_OK;
}
#endif


/*----------------------------------------------------------------------------
**          PUBLIC INTERFACE
**--------------------------------------------------------------------------*/

/*
  snd_codec_is_capable
    Returns true if the current spice implementation can
      use the given codec, false otherwise.
   mode must be a SPICE_AUDIO_DATA_MODE_XXX enum from spice/enum.h
 */
bool snd_codec_is_capable(SpiceAudioDataMode mode, int frequency)
{
#if HAVE_OPUS
    if (mode == SPICE_AUDIO_DATA_MODE_OPUS &&
         (frequency == SND_CODEC_ANY_FREQUENCY ||
          frequency == 48000 || frequency == 24000 ||
          frequency == 16000 || frequency == 12000 ||
          frequency == 8000) )
        return true;
#endif

    return false;
}

/*
  snd_codec_create
    Create a codec control.  Required for most functions in this library.
    Parameters:
      1.  codec     Pointer to preallocated codec control
      2.  mode      SPICE_AUDIO_DATA_MODE_XXX enum from spice/enum.h
      3.  encode    TRUE if encoding is desired
      4.  decode    TRUE if decoding is desired
     Returns:
       SND_CODEC_OK  if all went well; a different code if not.

  snd_codec_destroy is the obvious partner of snd_codec_create.
 */
SndCodecResult
snd_codec_create(SndCodec *codec, SpiceAudioDataMode mode, int frequency, int purpose)
{
    SndCodecResult rc = SND_CODEC_UNAVAILABLE;
    SndCodecInternal **c = codec;

    *c = spice_new0(SndCodecInternal, 1);
    (*c)->frequency = frequency;

#if HAVE_OPUS
    if (mode == SPICE_AUDIO_DATA_MODE_OPUS)
        rc = snd_codec_create_opus(*c, purpose);
#endif

    return rc;
}

/*
  snd_codec_destroy
    The obvious companion to snd_codec_create
*/
void snd_codec_destroy(SndCodec *codec)
{
    SndCodecInternal **c = codec;
    if (! c || ! *c)
        return;

#if HAVE_OPUS
    snd_codec_destroy_opus(*c);
#endif

    free(*c);
    *c = NULL;
}

/*
  snd_codec_frame_size
    Returns the size, in frames, of the raw PCM frame buffer
      required by this codec.  To get bytes, you'll need
      to multiply by channels and sample width.
 */
int snd_codec_frame_size(SndCodec codec)
{
#if HAVE_OPUS
    SndCodecInternal *c = codec;
    if (c && c->mode == SPICE_AUDIO_DATA_MODE_OPUS)
        return SND_CODEC_OPUS_FRAME_SIZE;
#endif
    return SND_CODEC_MAX_FRAME_SIZE;
}

/*
  snd_codec_encode
     Encode a block of data to a compressed buffer.

  Parameters:
    1.  codec       Pointer to codec control previously allocated + created
    2.  in_ptr      Pointer to uncompressed PCM data
    3.  in_size     Input size
    4.  out_ptr     Pointer to area to write encoded data
    5.  out_size    On input, the maximum size of the output buffer; on
                    successful return, it will hold the number of bytes
                    returned.

     Returns:
       SND_CODEC_OK  if all went well
*/
SndCodecResult
snd_codec_encode(SndCodec codec, uint8_t *in_ptr, int in_size, uint8_t *out_ptr, int *out_size)
{
#if HAVE_OPUS
    SndCodecInternal *c = codec;
    if (c && c->mode == SPICE_AUDIO_DATA_MODE_OPUS)
        return snd_codec_encode_opus(c, in_ptr, in_size, out_ptr, out_size);
#endif

    return SND_CODEC_ENCODER_UNAVAILABLE;
}

/*
  snd_codec_decode
     Decode a block of data from a compressed buffer.

  Parameters:
    1.  codec       Pointer to codec control previously allocated + created
    2.  in_ptr      Pointer to compressed data
    3.  in_size     Input size
    4.  out_ptr     Pointer to area to write decoded data
    5.  out_size    On input, the maximum size of the output buffer; on
                    successful return, it will hold the number of bytes
                    returned.

     Returns:
       SND_CODEC_OK  if all went well
*/
SndCodecResult
snd_codec_decode(SndCodec codec, uint8_t *in_ptr, int in_size, uint8_t *out_ptr, int *out_size)
{
#if HAVE_OPUS
    SndCodecInternal *c = codec;
    if (c && c->mode == SPICE_AUDIO_DATA_MODE_OPUS)
        return snd_codec_decode_opus(c, in_ptr, in_size, out_ptr, out_size);
#endif

    return SND_CODEC_DECODER_UNAVAILABLE;
}
