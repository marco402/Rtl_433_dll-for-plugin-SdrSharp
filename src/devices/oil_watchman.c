/** @file
    Oil tank monitor using Si4320 framed FSK protocol.

    Copyright (C) 2015 David Woodhouse

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include "decoder.h"

/**
Oil tank monitor using Si4320 framed FSK protocol.

Tested devices:
- Sensor Systems Watchman Sonic
- Kingspan Watchman Sonic Plus
*/
static int32_t oil_watchman_decode(r_device *decoder, bitbuffer_t *bitbuffer, int32_t startPulses, uint16_t package_type)
{
    int32_t row = 0;
    // Start of frame preamble is 111000xx
    uint8_t const preamble_pattern[] = {0xe0};

    // End of frame is 00xxxxxx or 11xxxxxx depending on final data bit
    uint8_t const postamble_pattern[2] = {0x00, 0xc0};

    uint32_t bit_offset      = 0;
    int32_t events           = 0;

    // Find a preamble with enough bits after it that it could be a complete packet
    while ((bit_offset = bitbuffer_search(bitbuffer, row, bit_offset, preamble_pattern, 6)) + 136 <=
            bitbuffer->bits_per_row[row]) {

        // Skip the matched preamble bits to point to the data
        bit_offset += 6;

        bitbuffer_t databits = {0};
        bit_offset = bitbuffer_manchester_decode(bitbuffer, row, bit_offset, &databits, 64);
        if (databits.bits_per_row[row] != 64)
            continue; // DECODE_ABORT_LENGTH

        uint8_t *b = databits.bb[row];

        // Check for postamble, depending on last data bit
        if (bitbuffer_search(bitbuffer, row, bit_offset, &postamble_pattern[b[7] & 1], 2) != bit_offset)
            continue; // DECODE_ABORT_EARLY

        if (b[7] != crc8le(b, 7, 0x31, 0))
            continue; // DECODE_FAIL_MIC

        // The unit ID changes when you rebind by holding a magnet to the
        // sensor for long enough; it seems to be time-based.
        uint32_t unit_id = ((uint32_t)b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];

        // 0x01: Rebinding (magnet held to sensor)
        // 0x08: Leak/theft alarm
        // top three bits seem also to vary with temperature (independently of maybetemp)
        uint8_t flags = b[4];

        // Not entirely sure what this is but it might be inversely
        // proportional to temperature.
        uint8_t maybetemp  = b[5] >> 2;
        double temperature = (double)(145.0 - 5.0 * maybetemp) / 3.0;

        uint16_t depth             = 0;
        uint16_t binding_countdown = 0;
        if (flags & 1) {
            // When binding, the countdown counts up from 0x51 to 0x5a
            // (as long as you hold the magnet to it for long enough)
            // before the device ID changes. The receiver unit needs
            // to receive this *strongly* in order to change its
            // allegiance.
            binding_countdown = b[6];
        }
        else {
            // A depth reading of zero indicates no reading. Even with
            // the sensor flat down on a table, it still reads about 13.
            depth = ((b[5] & 3) << 8) | b[6];
        }

        /* clang-format off */
        data_t *data = data_make(
                "model",                "", DATA_STRING, "Oil-SonicSmart",
                "id",                   "", DATA_FORMAT, "%06x", DATA_INT, unit_id,
                "flags",                "", DATA_FORMAT, "%02x", DATA_INT, flags,
                "maybetemp",            "", DATA_INT,    maybetemp,
                "temperature_C",        "", DATA_DOUBLE, temperature,
                "binding_countdown",    "", DATA_INT,    binding_countdown,
                "depth_cm",             "", DATA_INT,    depth,
                NULL);
        /* clang-format on */

        
        decoder_output_data(decoder, data, bitbuffer, row, 0, startPulses, package_type);
        events++;
    }
    return events;
}

static uint8_t const *const output_fields[] = {
        "model",
        "id",
        "flags",
        "maybetemp",
        "temperature_C",
        "binding_countdown",
        "depth_cm",
        NULL,
};

r_device const oil_watchman = {
        .name        = "Watchman Sonic / Apollo Ultrasonic / Beckett Rocket oil tank monitor",
        .modulation  = FSK_PULSE_PCM,
        .short_width = 1000,
        .long_width  = 1000, // NRZ
        .reset_limit = 4000,
        .decode_fn   = &oil_watchman_decode,
        .fields      = output_fields,
};
