/** @file
    FSK 9-byte Differential Manchester encoded TPMS data with CRC-8.

    Copyright (C) 2017 Christian W. Zuckschwerdt <zany@triq.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/
/**
FSK 9-byte Differential Manchester encoded TPMS data with CRC-8.
Pacific Industries Co.Ltd. PMV-C210
Seen on a Toyota Auris(Corolla). The manufacturers of the Toyota TPMS are
Pacific Industrial Corp and sometimes TRW Automotive and might also be used
in other car brands. Contact me with your observations!

There are 14 bits sync followed by 72 bits manchester encoded data and
3 bits trailer.
E.g. 01010101001111 00110011 [...64 manchester bits] 00101010111

The first 4 bytes are the ID. Followed by 1-bit state,
8-bit values of pressure, temperature, 7-bit state, 8-bit inverted pressure
and then the a CRC-8 with 0x07 truncated poly and init 0x80.
The temperature is offset by 40 deg C.
The pressure seems to be 1/4 PSI offset by -7 PSI (i.e. 28 raw = 0 PSI).
*/

#include "decoder.h"

static int32_t tpms_toyota_decode(r_device *decoder, bitbuffer_t *bitbuffer, int32_t row, uint32_t bit_offset,int32_t startPulses, uint16_t package_type)
{
    bitbuffer_t packet_bits = {0};
    uint8_t *b;
    uint32_t id;
    uint32_t status, pressure1, pressure2, temp;
    int32_t crc;

    // skip the first 1 bit, i.e. raw "01" to get 72 bits
    bit_offset = bitbuffer_differential_manchester_decode(bitbuffer, row, bit_offset, &packet_bits, 80);
    if (bit_offset - bit_offset < 144) {
        return 0;
    }
	row = 0; 
    b = packet_bits.bb[row];

    crc = b[8];
    if (crc8(b, 8, 0x07, 0x80) != crc) {
        return 0;
    }

    id        = (uint32_t)b[0] << 24 | b[1] << 16 | b[2] << 8 | b[3];
    status    = (b[4] & 0x80) | (b[6] & 0x7f); // status bit and 0 filler
    pressure1 = (b[4] & 0x7f) << 1 | b[5] >> 7;
    temp      = (b[5] & 0x7f) << 1 | b[6] >> 7;
    pressure2 = b[7] ^ 0xff;

    if (pressure1 != pressure2) {
        decoder_logf(decoder, 1, __func__, "Toyota TPMS pressure check error: %02x vs %02x", pressure1, pressure2);
        return 0;
    }

    uint8_t id_str[9];
    snprintf(id_str, sizeof(id_str), "%08x", id);

    /* clang-format off */
    data_t *data = data_make(
            "model",            "",             DATA_STRING,    "Toyota",
            "type",             "",             DATA_STRING,    "TPMS",
            "id",               "",             DATA_STRING,    id_str,
            "status",           "",             DATA_INT,       status,
            "pressure_PSI",     "",             DATA_DOUBLE,    pressure1*0.25-7.0,
            "temperature_C",    "",             DATA_DOUBLE,    temp-40.0,
            "mic",              "Integrity",    DATA_STRING,    "CRC",
            NULL);
    /* clang-format on */


    decoder_output_data(decoder, data, &packet_bits, row, 0, startPulses, package_type);
    return 1;
}

/** @sa tpms_toyota_decode() */
static int32_t tpms_toyota_callback(r_device *decoder, bitbuffer_t *bitbuffer, int32_t startPulses, uint16_t package_type)
{
    int32_t row = 0;
    // full preamble is 0101 0101 0011 11 = 55 3c
    // could be shorter   11 0101 0011 11
    uint8_t const preamble_pattern[2] = {0xa9, 0xe0}; // 12 bits (but pass last bit to decode)

    uint32_t bit_offset = 0;
    int32_t ret         = 0;
    int32_t events      = 0;

    // Find a preamble with enough bits after it that it could be a complete packet
    while ((bit_offset = bitbuffer_search(bitbuffer, row, bit_offset, preamble_pattern, 12)) + 156 <=
            bitbuffer->bits_per_row[row]) {
        ret = tpms_toyota_decode(decoder, bitbuffer, row, bit_offset + 11, startPulses, package_type);
        if (ret > 0)
            events += ret;
        bit_offset += 2;
    }

    return events > 0 ? events : ret;
}

static uint8_t const *const output_fields[] = {
        "model",
        "type",
        "id",
        "status",
        "pressure_PSI",
        "temperature_C",
        "mic",
        NULL,
};

r_device const tpms_toyota = {
        .name        = "Toyota TPMS",
        .modulation  = FSK_PULSE_PCM,
        .short_width = 52,  // 12-13 samples @250k
        .long_width  = 52,  // FSK
        .reset_limit = 150, // Maximum gap size before End Of Message [us].
        .decode_fn   = &tpms_toyota_callback,
        .fields      = output_fields,
};
