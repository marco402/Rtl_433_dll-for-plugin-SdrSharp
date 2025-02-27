/** @file
    Oil tank monitor using manchester encoded FSK/ASK protocol.

    Copyright (C) 2017 Christian W. Zuckschwerdt <zany@triq.net>
    based on code Copyright (C) 2015 David Woodhouse

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include "decoder.h"

/**
Oil tank monitor using manchester encoded FSK/ASK protocol.

Tested devices:
- APOLLO ULTRASONIC STANDARD (maybe also VISUAL but not SMART, FSK)
- Tekelek TEK377E (E: European, A: American version)
- Beckett Rocket TEK377A (915MHz, ASK)

Should apply to similar Watchman, Beckett, and Apollo devices too.

The sensor sends a single packet once every hour or twice a second
for 11 minutes when in pairing/test mode (pairing needs 35 sec).
depth reading is in cm, lowest reading is ~3, highest is ~305, 0 is invalid

    IIII IIII IIII IIII 0FFF L0OP DDDD DDDD

The TEK377E might send an additional 8 zero bits.

example packets are:

    010101 01010101 01010111 01101001 10011010 10101001 10100101 10011010 01101010 10011001 10011010 0000
    010101 01010101 01011000 10011010 01010110 01101010 10101010 10100101 01101010 10100110 10101001 1111

Start of frame full preamble is depending on first data bit either

    01 0101 0101 0101 0101 0111 01
    01 0101 0101 0101 0101 1000 10
*/
static int32_t oil_standard_decode(r_device *decoder, bitbuffer_t *bitbuffer, int32_t row, uint32_t bit_offset, int32_t startPulses, uint16_t package_type)
{
    bitbuffer_t databits = {0};
    bitbuffer_manchester_decode(bitbuffer, row, bit_offset, &databits, 41);

    if (databits.bits_per_row[row] < 32 || databits.bits_per_row[row] > 40 || (databits.bb[row][4] & 0xfe) != 0)
        return 0; // TODO: fix calling code to handle negative return values

    uint8_t *b = databits.bb[0];

    // The unit ID changes when you rebind by holding a magnet to the
    // sensor for long enough.
    uint16_t unit_id = (b[0] << 8) | b[1];

    // 0x01: Rebinding (magnet held to sensor)
    // 0x02: High-bit for depth
    // 0x04: (always zero?)
    // 0x08: Leak/theft alarm
    // 0x10: (unknown toggle)
    // 0x20: (unknown toggle)
    // 0x40: (unknown toggle)
    // 0x80: (always zero?)
    uint8_t flags = b[2] & ~0x0A;
    uint8_t alarm = (b[2] & 0x08) >> 3;

    uint16_t depth             = 0;
    uint16_t binding_countdown = 0;
    if (flags & 1) {
        // When binding, the countdown counts up from 0x40 to 0x4a
        // (as long as you hold the magnet to it for long enough)
        // before the device ID changes. The receiver unit needs
        // to receive this *strongly* in order to change its
        // allegiance.
        binding_countdown = b[3];
    }
    else {
        // A depth reading of zero indicates no reading.
        depth = ((b[2] & 0x02) << 7) | b[3];
    }

    /* clang-format off */
    data_t *data = data_make(
            "model",                "", DATA_STRING, "Oil-SonicStd",
            "id",                   "", DATA_FORMAT, "%04x", DATA_INT, unit_id,
            "flags",                "", DATA_FORMAT, "%02x", DATA_INT, flags,
            "alarm",                "", DATA_INT,    alarm,
            "binding_countdown",    "", DATA_INT,    binding_countdown,
            "depth_cm",             "", DATA_INT,    depth,
            NULL);
    /* clang-format on */

        
      decoder_output_data(decoder, data, bitbuffer, row, 0, startPulses, package_type); 
    return 1;
}

/**
Oil tank monitor using manchester encoded FSK/ASK protocol.
@sa oil_standard_decode()
*/
static int32_t oil_standard_callback(r_device *decoder, bitbuffer_t *bitbuffer, int32_t startPulses, uint16_t package_type)
{
    int32_t row                       = 0;
    uint8_t const preamble_pattern0[2] = {0x55, 0x5D};
    uint8_t const preamble_pattern1[2] = {0x55, 0x62};
    // End of frame is the last half-bit repeated additional 4 times

    uint32_t bit_offset = 0;
    int32_t events      = 0;

    // Find a preamble with enough bits after it that it could be a complete packet
    while ((bit_offset = bitbuffer_search(bitbuffer, row, bit_offset, preamble_pattern0, 16)) + 78 <=
            bitbuffer->bits_per_row[row]) {
        events += oil_standard_decode(decoder, bitbuffer, row, bit_offset + 14, startPulses, package_type);
        bit_offset += 2;
    }

    bit_offset = 0;
    while ((bit_offset = bitbuffer_search(bitbuffer, row, bit_offset, preamble_pattern1, 16)) + 78 <=
            bitbuffer->bits_per_row[row]) {
        events += oil_standard_decode(decoder, bitbuffer, row, bit_offset + 14, startPulses, package_type);
        bit_offset += 2;
    }
    return events;
}

static uint8_t const *const output_fields[] = {
        "model",
        "id",
        "flags",
        "alarm",
        "binding_countdown",
        "depth_cm",
        NULL,
};

r_device const oil_standard = {
        .name        = "Oil Ultrasonic STANDARD FSK",
        .modulation  = FSK_PULSE_PCM,
        .short_width = 500,
        .long_width  = 500,
        .reset_limit = 2000,
        .decode_fn   = &oil_standard_callback,
        .fields      = output_fields,
};

r_device const oil_standard_ask = {
        .name        = "Oil Ultrasonic STANDARD ASK",
        .modulation  = OOK_PULSE_PCM,
        .short_width = 500,
        .long_width  = 500,
        .reset_limit = 2000,
        .decode_fn   = &oil_standard_callback,
        .fields      = output_fields,
};
