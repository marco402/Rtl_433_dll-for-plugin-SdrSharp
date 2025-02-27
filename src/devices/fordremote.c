/** @file
    Ford Car Key.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

*/
/**
Ford Car Key.

Identifies event, but does not attempt to decrypt rolling code...
Note: this used to have a broken PWM decoding, but is now proper DMC.
The output changed and the fields are very likely not as intended.

    [00] {1} 80 : 1
    [01] {9} 00 80 : 00000000 1
    [02] {1} 80 : 1
    [03] {78} 03 e0 01 e4 e0 90 52 97 39 60

*/

#include "decoder.h"

static int32_t fordremote_callback(r_device *decoder, bitbuffer_t *bitbuffer, int32_t startPulses, uint16_t package_type)
{
    int32_t row = 0;
    data_t *data;
    uint8_t *bytes;
    int32_t found = 0;
    int32_t device_id, code;

    // expect {1} {9} {1} preamble
    for (row = 3; row < bitbuffer->num_rows; row++) {
        if (bitbuffer->bits_per_row[row] < 78) {
            continue; // DECODE_ABORT_LENGTH
        }

        // Validate preamble
        // pf 9_ford-unlock002_250k_131072b__STEREO.wav ok with hideki_250k_gfile001_262144b_STEREO twice are OOK_PULSE_DMC
		if (bitbuffer->bits_per_row[row - 3] != 1 || bitbuffer->bits_per_row[row - 1] != 1 
                || bitbuffer->bits_per_row[row - 2] != 9 || bitbuffer->bb[row - 2][0] != 0) {
            continue; // DECODE_ABORT_EARLY
        }

        decoder_log_bitbuffer(decoder, 1, __func__, bitbuffer, "");

        bytes     = bitbuffer->bb[row];
        device_id = (bytes[0] << 16) | (bytes[1] << 8) | bytes[2];
        code      = bytes[7];

        /* clang-format off */
        data = data_make(
                "model",    "model",        DATA_STRING, "Ford-CarRemote",
                "id",       "device-id",    DATA_INT,    device_id,
                "code",     "data",         DATA_INT,    code,
                NULL);
uint32_t bit_offset = 0;
		decoder_output_data(decoder, data, bitbuffer, row, 0, startPulses, package_type);

        /* clang-format on */

        found++;
    }
    return found;
}

static uint8_t const *const output_fields[] = {
        "model",
        "id",
        "code",
        NULL,
};

r_device const fordremote = {
        .name        = "Ford Car Key",
        .modulation  = OOK_PULSE_DMC,
        .short_width = 250,  // half-bit width is 250 us
        .long_width  = 500,  // bit width is 500 us
        .reset_limit = 4000, // sync gap is 3500 us, preamble gap is 38400 us, packet gap is 52000 us
        .tolerance   = 50,
        .decode_fn   = &fordremote_callback,
        .fields      = output_fields,
};
