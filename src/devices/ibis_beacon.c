/** @file
    IBIS vehicle information beacon.

    Copyright (C) 2017 Christian W. Zuckschwerdt <zany@triq.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/
/**
IBIS vehicle information beacon.
(used in public transportation)

The packet is 28 manchester encoded bytes with a Preamble of 0xAAB and
16-bit CRC, containing a company ID, vehicle ID, (door opening) counter,
and various flags.

*/

#include "decoder.h"

static int32_t ibis_beacon_callback(r_device *decoder, bitbuffer_t *bitbuffer, int32_t startPulses, uint16_t package_type)
{
    int32_t row = 0;
    data_t *data;
    uint8_t search = 0xAB; // preamble is 0xAAB
    uint8_t msg[32];
    uint32_t len;
    uint32_t bit_offset;
    uint32_t i;
    int32_t id;
    uint32_t counter;
    int32_t crc;
    int32_t crc_calculated;
    uint8_t code_str[63];

    // 224 bits data + 12 bits preamble
    if (bitbuffer->num_rows != 1 || bitbuffer->bits_per_row[row] < 232 || bitbuffer->bits_per_row[row] > 250) {
        return DECODE_ABORT_LENGTH; // Unrecognized data
    }

    bit_offset = bitbuffer_search(bitbuffer, row, 0, &search, 8);
    if (bit_offset > 26) {
        return DECODE_ABORT_EARLY; // short buffer or preamble not found
    }
    bit_offset += 8; // skip preamble
    len = bitbuffer->bits_per_row[row] - bit_offset;
    // we want 28 bytes (224 bits)
    if (len < 224) {
        return DECODE_ABORT_LENGTH; // short buffer
    }
    len = 224; // cut the last pulse

    bitbuffer_extract_bytes(bitbuffer, row, bit_offset, (uint8_t *)&msg, len);

    crc_calculated = crc16(msg, 26, 0x8005, 0x0000);
    crc = (msg[26] << 8) | msg[27];
    if (crc != crc_calculated) {
        return DECODE_FAIL_MIC; // bad crc
    }

    id = ((msg[5]&0x0f) << 12) | (msg[6] << 4) | ((msg[7]&0xf0) >> 4);
    counter = ((uint32_t)msg[20] << 24) | (msg[21] << 16) | (msg[22] << 8) | msg[23];

    for (i=0; i<(len+7)/8 ; ++i) {
        sprintf(&code_str[i*2], "%02x", msg[i]);
    }

    /* clang-format off */
    data = data_make(
            "model",    "",             DATA_STRING,    "IBIS-Beacon",
            "id",       "Vehicle No.",  DATA_INT,       id,
            "counter",  "Counter",      DATA_INT,       counter,
            "code",     "Code data",    DATA_STRING,    code_str,
            "mic",      "Integrity",    DATA_STRING,    "CRC",
            NULL);
    /* clang-format on */

    decoder_output_data(decoder, data, bitbuffer, row, 0, startPulses, package_type);
    return 1;
}

static uint8_t const *const output_fields[] = {
        "model",
        "id",
        "counter",
        "code",
        "mic",
        NULL,
};

r_device const ibis_beacon = {
        .name        = "IBIS beacon",
        .modulation  = OOK_PULSE_MANCHESTER_ZEROBIT,
        .short_width = 30,  // Nominal width of clock half period [us]
        .long_width  = 0,   // Not used
        .reset_limit = 100, // Maximum gap size before End Of Message [us].
        .decode_fn   = &ibis_beacon_callback,
        .fields      = output_fields,
};
