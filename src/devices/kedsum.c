/** @file
    Kedsum temperature and humidity sensor (http://amzn.to/25IXeng).

    Copyright (C) 2016 John Lifsey
    Enhanced (C) 2019 Christian W. Zuckschwerdt <zany@triq.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/
/**
Largely the same as esperanza_ews, s3318p.
@sa esperanza_ews.c s3318p.c

My models transmit at a bit lower freq. of around 433.71 Mhz.
Also NC-7415 from Pearl.

Frame structure:

    Byte:      0        1        2        3        4
    Nibble:    1   2    3   4    5   6    7   8    9   10
    Type:   00 IIIIIIII BBCC++++ ttttTTTT hhhhHHHH FFFFXXXX

- I: unique id. changes on powercycle
- B: Battery state 10 = Ok, 01 = weak, 00 = bad
- C: channel, 00 = ch1, 10=ch3
- + low temp nibble
- t: med temp nibble
- T: high temp nibble
- h: humidity low nibble
- H: humidity high nibble
- F: flags
- X: CRC-4 poly 0x3 init 0x0 xor last 4 bits
*/

#include "decoder.h"

static int32_t kedsum_callback(r_device *decoder, bitbuffer_t *bitbuffer, int32_t startPulses, uint16_t package_type)
{
    uint8_t b[5];
    data_t *data;

    // the signal should start with 15 sync pulses (empty rows)
    // require at least 5 received syncs
    if (bitbuffer->num_rows < 5
            || bitbuffer->bits_per_row[0] != 0
            || bitbuffer->bits_per_row[1] != 0
            || bitbuffer->bits_per_row[2] != 0
            || bitbuffer->bits_per_row[3] != 0
            || bitbuffer->bits_per_row[4] != 0)
        return DECODE_ABORT_EARLY;

    // the signal should have 6 repeats with a sync pulse between
    // require at least 4 received repeats
	uint32_t nbRepeat = 4;
	if (decoder->_sourceIsFile)
		nbRepeat = 0;
		
    int32_t row = bitbuffer_find_repeated_row(bitbuffer, nbRepeat, 42);
    if (row < 0 || bitbuffer->bits_per_row[row] != 42)
        return DECODE_ABORT_LENGTH;

    // remove the two leading 0-bits and align the data
    bitbuffer_extract_bytes(bitbuffer, row, 2, b, 40);

    // CRC-4 poly 0x3, init 0x0 over 32 bits then XOR the next 4 bits
    int32_t crc = crc4(b, 4, 0x3, 0x0) ^ (b[4] >> 4);
    if (crc != (b[4] & 0xf))
        return DECODE_FAIL_MIC;

    int32_t id       = (b[0]);
    int32_t battery  = (b[1] >> 6); // level 0-2
    int32_t channel  = ((b[1] & 0x30) >> 4) + 1;
    int32_t temp_raw = ((b[2] & 0x0f) << 8) | (b[2] & 0xf0) | (b[1] & 0x0f);
    int32_t humidity = ((b[3] & 0x0f) << 4) | ((b[3] & 0xf0) >> 4);
    float temp_f = (temp_raw - 900) * 0.1f;

    int32_t flags = (b[1] & 0xc0) | (b[4] >> 4);

    battery = battery == 2 ? 100 : battery * 10; // level 0,1,2 -> 0,10,100

    /* clang-format off */
    data = data_make(
            "model",            "",                 DATA_STRING, "Kedsum-TH",
            "id",               "ID",               DATA_INT,    id,
            "channel",          "Channel",          DATA_INT,    channel,
            "battery_ok",       "Battery level",    DATA_DOUBLE, battery * 0.01f,
            "flags",            "Flags2",           DATA_INT,    flags,
            "temperature_F",    "Temperature",      DATA_FORMAT, "%.2f F", DATA_DOUBLE, temp_f,
            "humidity",         "Humidity",         DATA_FORMAT, "%u %%", DATA_INT, humidity,
            "mic",              "Integrity",        DATA_STRING, "CRC",
            NULL);
    /* clang-format on */
	//row = 0;    //particular case test row 0 and use another
    decoder_output_data(decoder, data, bitbuffer, row, nbRepeat, startPulses, package_type);
    return 1;
}

static uint8_t const *const output_fields[] = {
        "model",
        "id",
        "channel",
        "battery_ok",
        "flags",
        "temperature_F",
        "humidity",
        "mic",
        NULL,
};

r_device const kedsum = {
        .name        = "Kedsum Temperature & Humidity Sensor, Pearl NC-7415",
        .modulation  = OOK_PULSE_PPM,
        .short_width = 2000,
        .long_width  = 4000,
        .gap_limit   = 4400,
        .reset_limit = 9400,
        .decode_fn   = &kedsum_callback,
        .fields      = output_fields,
};
