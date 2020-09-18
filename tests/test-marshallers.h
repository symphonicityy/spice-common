#ifndef H_SPICE_COMMON_TEST_MARSHALLERS
#define H_SPICE_COMMON_TEST_MARSHALLERS

#include <stdint.h>

#include "generated_test_messages.h"

typedef struct {
    uint32_t data_size;
    uint8_t dummy_byte;
    uint64_t *data;
} SpiceMsgMainShortDataSubMarshall;

typedef struct {
    int8_t name[0];
} SpiceMsgMainArrayMessage;

typedef struct {
    uint16_t n;
} SpiceMsgMainZeroes;

typedef struct SpiceMsgChannels {
    uint32_t num_of_channels;
    uint16_t channels[0];
} SpiceMsgChannels;

typedef struct {
    uint32_t dummy[2];
    uint8_t data[0];
} SpiceMsgMainLenMessage;

#endif /* H_SPICE_COMMON_TEST_MARSHALLERS */

