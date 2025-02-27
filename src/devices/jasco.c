/** @file
    Jasco/GE Choice Alert Wireless Device Decoder.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/
/**
Jasco/GE Choice Alert Wireless Device Decoder.

- Frequency: 318.01 MHz

Manchester PCM with a de-sync preamble of 0xFC0C (11111100000011000).

Packets are 32 bit, 24 bit data and 8 bit XOR checksum.

*/

#include "decoder.h"

static int32_t jasco_decode(r_device *decoder, bitbuffer_t *bitbuffer, int32_t startPulses, uint16_t package_type)
{
    int32_t row             = 0;
    uint8_t const preamble[] = {0xfc, 0x0c}; // length 16

    if (bitbuffer->bits_per_row[row] < 80
            || bitbuffer->bits_per_row[row] > 87) {
        if (bitbuffer->bits_per_row[row] > 0) {
            decoder_logf(decoder, 2, __func__, "invalid bit count %d", bitbuffer->bits_per_row[row]);
        }
        return DECODE_ABORT_EARLY;
    }

    uint32_t bit_offset = bitbuffer_search(bitbuffer, row, 0, preamble, 16) + 16;

    if (bit_offset + 64 > bitbuffer->bits_per_row[row]) {
        return DECODE_ABORT_LENGTH;
    }

    bitbuffer_t packet_bits = {0};
    bitbuffer_manchester_decode(bitbuffer, row, bit_offset, &packet_bits, 32);

    if (packet_bits.bits_per_row[row] < 32) {
        return DECODE_ABORT_LENGTH;
    }

    uint8_t *b = packet_bits.bb[row];

    int32_t chk = b[0] ^ b[1] ^ b[2] ^ b[3];
    if (chk) {
        return DECODE_FAIL_MIC;
    }

    int32_t sensor_id = (b[0] << 8) | b[1];

    int32_t s_closed = ((b[2] & 0xef) == 0xef);
    // int32_t battery = 0;

    /* clang-format off */
    data_t *data = data_make(
            "model",            "",             DATA_STRING, "Jasco-Security",
            "id",               "Id",           DATA_INT,    sensor_id,
            "status",           "Closed",       DATA_INT,    s_closed,
            "mic",              "Integrity",    DATA_STRING, "CHECKSUM",
            NULL);
    /* clang-format on */

    decoder_output_data(decoder, data, bitbuffer, row, 0, startPulses, package_type);

    return 1;
}

static uint8_t const *const output_fields[] = {
        "model",
        "id",
        "status",
        "mic",
        NULL,
};

r_device const jasco = {
        .name        = "Jasco/GE Choice Alert Security Devices",
        .modulation  = OOK_PULSE_PCM,
        .short_width = 250,
        .long_width  = 250,
        .reset_limit = 1800, // Maximum gap size before End Of Message
        .decode_fn   = &jasco_decode,
        .fields      = output_fields,

};
