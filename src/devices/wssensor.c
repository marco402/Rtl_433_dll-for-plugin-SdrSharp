/** @file
    Hyundai WS SENZOR Remote Temperature Sensor.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/
/**
Hyundai WS SENZOR Remote Temperature Sensor.

- Transmit Interval: every ~33s
- Frequency 433.92 MHz
- Distance coding: Pulse length 224 us
- Short distance: 1032 us, long distance: 1992 us, packet distance: 4016 us

24-bit data packet format, repeated 23 times

    TTTTTTTT TTTTBSCC IIIIIIII

- T = signed temperature * 10 in Celsius
- B = battery status (0 = low, 1 = OK)
- S = startup (0 = normal operation, 1 = battery inserted or TX button pressed)
- C = channel (0-2)
- I = sensor ID
*/

#include "decoder.h"

#define WS_PACKETLEN 24
#define WS_MINREPEATS 4
#define WS_REPEATS 23

static int32_t wssensor_decode(r_device *decoder, bitbuffer_t *bitbuffer, int32_t startPulses, uint16_t package_type)
{
    uint8_t *b;
    data_t *data;

    // the signal should have 23 repeats
    // require at least 4 received repeats
	uint32_t nbRepeat = WS_MINREPEATS;
	if (decoder->_sourceIsFile)
		nbRepeat = 0;
		
    int32_t row = bitbuffer_find_repeated_row(bitbuffer, nbRepeat, WS_REPEATS);
    if (row < 0 || bitbuffer->bits_per_row[row] != WS_PACKETLEN)
        return DECODE_ABORT_LENGTH;

    b = bitbuffer->bb[row];

    // No need to decode/extract values for simple test
    if ((!b[0] && !b[1] && !b[2])
       || (b[0] == 0xff && b[1] == 0xff && b[2] == 0xff)) {
        decoder_log(decoder, 2, __func__, "DECODE_FAIL_SANITY data all 0x00 or 0xFF");
        return DECODE_FAIL_SANITY;
    }

    int32_t temperature;
    int32_t battery_status;
    int32_t startup;
    int32_t channel;
    int32_t sensor_id;
    float temperature_c;

    /* TTTTTTTT TTTTBSCC IIIIIIII  */
    temperature = (int16_t)((b[0] << 8) | (b[1] & 0xf0)); // uses sign extend
    battery_status = (b[1] & 0x08) >> 3;
    startup = (b[1] & 0x04) >> 2;
    channel = (b[1] & 0x03) + 1;
    sensor_id = b[2];

    temperature_c = (temperature >> 4) * 0.1f;

    /* clang-format off */
    data = data_make(
            "model",         "",            DATA_STRING, "Hyundai-WS",
            "id",            "House Code",  DATA_INT, sensor_id,
            "channel",       "Channel",     DATA_INT, channel,
            "battery_ok",    "Battery",     DATA_INT,    !!battery_status,
            "temperature_C", "Temperature", DATA_FORMAT, "%.2f C", DATA_DOUBLE, temperature_c,
            "button",           "Button",       DATA_INT, startup,
            NULL);
    /* clang-format on */

    decoder_output_data(decoder, data, bitbuffer, row, nbRepeat, startPulses, package_type);
    return 1;
}

static uint8_t const *const output_fields[] = {
        "model",
        "id",
        "channel",
        "battery_ok",
        "temperature_C",
        "button",
        NULL,
};

r_device const wssensor = {
        .name        = "Hyundai WS SENZOR Remote Temperature Sensor",
        .modulation  = OOK_PULSE_PPM,
        .short_width = 1000,
        .long_width  = 2000,
        .gap_limit   = 2400,
        .reset_limit = 4400,
        .decode_fn   = &wssensor_decode,
        .fields      = output_fields,
};
