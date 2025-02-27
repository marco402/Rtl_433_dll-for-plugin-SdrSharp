/**  @file
    Porsche Boxster/Cayman TPMS.

    Copyright (C) 2021 Christian W. Zuckschwerdt <zany@triq.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

/**
Porsche Boxster/Cayman TPMS.
Seen on Porsche second generation (Typ 987) Boxster/Cayman.

Full preamble is {30}ccccccca (33333332).
The data is Differential Manchester Coded (DMC).

Example data:

    {193}333333354ab32d334b2d4ab2cab54cb34cb2aab4cd552ccd8
    {193}333333354ab32d334b2d4ab2caaab34cb34d554aacd2b2cd8

    23d7 ad23 623b bb02 f05f
    23d7 ad23 603b bb02 1d37

Data layout (nibbles):

    II II II II PP TT SS SS CC

- I: 32 bit ID
- P: 8 bit Pressure (scale 2.5 offset 100, minimum seen 41 = 0 kPa)
- T: 8 bit Temperature (deg. C offset by 40)
- S: Status?
- C: 16 bit Checksum, CRC-16 poly 0x1021 init 0xffff
*/

#include "decoder.h"

static int32_t tpms_porsche_decode(r_device *decoder, bitbuffer_t *bitbuffer, int32_t row, uint32_t bit_offset, int32_t startPulses, uint16_t package_type)
{
    bitbuffer_t packet_bits = {0};
    bitbuffer_differential_manchester_decode(bitbuffer, row, bit_offset, &packet_bits, 80);

    // make sure we decoded the expected number of bits
    if (packet_bits.bits_per_row[row] < 80) {
        // decoder_logf(decoder, 0, __func__, "bit_offset=%u bit_offset=%u = %u", bit_offset, bit_offset, (bit_offset - bit_offset));
        return 0; // DECODE_FAIL_SANITY;
    }

    uint8_t *b = packet_bits.bb[row];

    // Checksum is CRC-16 poly 0x1021 init 0xffff over 8 bytes
    int32_t checksum = crc16(b, 10, 0x1021, 0xffff);
    if (checksum != 0) {
        return 0; // DECODE_FAIL_MIC;
    }

    int32_t id          = (uint32_t)b[0] << 24 | b[1] << 16 | b[2] << 8 | b[3];
    int32_t pressure    = b[4];
    int32_t temperature = b[5];
    int32_t flags       = b[6] << 8 | b[7];

    int32_t pressure_kpa  = pressure * 5 / 2 - 100;
    int32_t temperature_c = temperature - 40;

    uint8_t id_str[4 * 2 + 1];
    snprintf(id_str, sizeof(id_str), "%08x", id);

    /* clang-format off */
    data_t *data = data_make(
            "model",            "",             DATA_STRING, "Porsche",
            "type",             "",             DATA_STRING, "TPMS",
            "id",               "",             DATA_STRING, id_str,
            "pressure_kPa",     "Pressure",     DATA_FORMAT, "%.1f kPa",    DATA_DOUBLE, (float)pressure_kpa,
            "temperature_C",    "Temperature",  DATA_FORMAT, "%.0f C",      DATA_DOUBLE, (float)temperature_c,
            "flags",            "",             DATA_FORMAT, "%04x",        DATA_INT,    flags,
            "mic",              "Integrity",    DATA_STRING, "CRC",
            NULL);
    /* clang-format on */

        
    decoder_output_data(decoder, data, bitbuffer, row, 0, startPulses, package_type); 
    return 1;
}

/** @sa tpms_porsche_decode() */
static int32_t tpms_porsche_callback(r_device *decoder, bitbuffer_t *bitbuffer, int32_t startPulses, uint16_t package_type)
{
    int32_t row = 0;
    // Full preamble is {30}ccccccca (33333332).
    uint8_t const preamble_pattern[] = {0x33, 0x33, 0x20}; // 20 bit

    int32_t events = 0;

    // Find a preamble with enough bits after it that it could be a complete packet
    uint32_t bit_offset = 0;
    while ((bit_offset = bitbuffer_search(bitbuffer, row, bit_offset, preamble_pattern, 20)) + 100 <=
            bitbuffer->bits_per_row[row]) {
        events += tpms_porsche_decode(decoder, bitbuffer, row, bit_offset + 20, startPulses, package_type);
        bit_offset += 2;
    }

    return events;
}

static uint8_t const *const output_fields[] = {
        "model",
        "type",
        "id",
        "pressure",
        "temperature_C",
        "flags",
        "mic",
        NULL,
};

r_device const tpms_porsche = {
        .name        = "Porsche Boxster/Cayman TPMS",
        .modulation  = FSK_PULSE_PCM,
        .short_width = 52,  // 12-13 samples @250k
        .long_width  = 52,  // FSK
        .reset_limit = 150, // Maximum gap size before End Of Message [us].
        .decode_fn   = &tpms_porsche_callback,
        .fields      = output_fields,
};
