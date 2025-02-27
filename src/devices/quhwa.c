/** @file
    Quhwa HS1527.

    Copyright (C) 2016 Ask Jakobsen

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/
/**
Quhwa HS1527.

Tested devices:
QH-C-CE-3V (which should be compatible with QH-832AC),
also sold as "1 by One" wireless doorbell
*/

#include "decoder.h"

static int32_t quhwa_callback(r_device *decoder, bitbuffer_t *bitbuffer, int32_t startPulses, uint16_t package_type)
{
	uint32_t nbRepeat = 5;
	
		
    int32_t row = bitbuffer_find_repeated_row(bitbuffer, nbRepeat, 18);
    if (row < 0)
        return DECODE_ABORT_EARLY;

    uint8_t *b = bitbuffer->bb[row];

    // No need to decode/extract values for simple test
    if (!b[0] && !b[1] && !b[2]) {
        decoder_log(decoder, 2, __func__, "DECODE_FAIL_SANITY data all 0x00");
        return DECODE_FAIL_SANITY;
    }

    b[0] = ~b[0];
    b[1] = ~b[1];
    b[2] = ~b[2];

    if (bitbuffer->bits_per_row[row] != 18
            || (b[1] & 0x03) != 0x03
            || (b[2] & 0xC0) != 0xC0)
        return DECODE_ABORT_LENGTH;

    uint32_t id = (b[0] << 8) | b[1];

    /* clang-format off */
    data_t *data = data_make(
            "model",  "",    DATA_STRING, "Quhwa-Doorbell",
            "id",     "ID",  DATA_INT, id,
            NULL);
    /* clang-format on */
    uint32_t bit_offset = 0;

    decoder_output_data(decoder, data, bitbuffer, row, nbRepeat, startPulses, package_type);
    return 1;
}

static uint8_t const *const output_fields[] = {
        "model",
        "id",
        NULL,
};

r_device const quhwa = {
        .name        = "Quhwa",
        .modulation  = OOK_PULSE_PWM,
        .short_width = 360,  // Pulse: Short 360µs, Long 1070µs
        .long_width  = 1070, // Gaps: Short 360µs, Long 1070µs
        .reset_limit = 6600, // Intermessage Gap 6500µs
        .gap_limit   = 1200, // Long Gap 1120µs
        .sync_width  = 0,    // No sync bit used
        .tolerance   = 80,   // us
        .decode_fn   = &quhwa_callback,
        .fields      = output_fields,
};
