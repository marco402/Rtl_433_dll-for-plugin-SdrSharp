/** @file
    ThermoPro TX2 sensor protocol.

    Copyright (C) 2020 Christian W. Zuckschwerdt <zany@triq.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include "decoder.h"

/**
ThermoPro TX2 sensor protocol.

Note: this is the Prologue protocol with the battery low flag inverted.
Disable Prologue and enable this to use, e.g. `-R -3 -R 162`.

Note: this is a false positive for AlectoV1.

The sensor sends 36 bits 7 times, before the first packet there is a sync pulse.
The packets are ppm modulated (distance coding) with a pulse of ~500 us
followed by a short gap of ~2000 us for a 0 bit or a long ~4000 us gap for a
1 bit, the sync gap is ~9000 us.

The data is grouped in 9 nibbles

    [type] [id0] [id1] [flags] [temp0] [temp1] [temp2] [humi0] [humi1]

- type: 4 bit fixed 1001 (9) or 0110 (5)
- id: 8 bit a random id that is generated when the sensor starts, could include battery status
  the same batteries often generate the same id
- flags(3): is 1 when the battery is low, otherwise 0 (ok)
- flags(2): is 1 when the sensor sends a reading when pressing the button on the sensor
- flags(1,0): the channel number that can be set by the sensor (1, 2, 3, X)
- temp: 12 bit signed scaled by 10
- humi: 8 bit always 11001100 (0xCC) if no humidity sensor is available

*/
static int32_t thermopro_tx2_decode(r_device *decoder, bitbuffer_t *bitbuffer, int32_t startPulses, uint16_t package_type)
{
    uint8_t *b;
    data_t *data;

    int32_t type;
    int32_t id;
    int32_t battery;
    int32_t button;
    int32_t channel;
    int32_t temp_raw;
    int32_t humidity;

    if (bitbuffer->bits_per_row[0] <= 8 && bitbuffer->bits_per_row[0] != 0)
        return DECODE_ABORT_EARLY; // Alecto/Auriol-v2 has 8 sync bits, reduce false positive
	uint32_t nbRepeat = 4;
	if (decoder->_sourceIsFile)
		nbRepeat = 0;
		
    int32_t row = bitbuffer_find_repeated_row(bitbuffer, nbRepeat, 36); // only 3 repeats will give false positives for Alecto/Auriol-v2
    if (row < 0)
        return DECODE_ABORT_EARLY;

    if (bitbuffer->bits_per_row[row] > 37) // we expect 36 bits but there might be a trailing 0 bit
        return DECODE_ABORT_LENGTH;

    b = bitbuffer->bb[row];

    if ((b[0] & 0xF0) != 0x90 && (b[0] & 0xF0) != 0x50)
        return DECODE_FAIL_SANITY;

    // Prologue/ThermoPro-TX2 sensor
    type     = b[0] >> 4;
    id       = ((b[0] & 0x0F) << 4) | ((b[1] & 0xF0) >> 4);
    battery  = b[1] & 0x08;
    button   = (b[1] & 0x04) >> 2;
    channel  = (b[1] & 0x03) + 1;
    temp_raw = (int16_t)((b[2] << 8) | (b[3] & 0xF0)); // uses sign-extend
    temp_raw = temp_raw >> 4;
    humidity = ((b[3] & 0x0F) << 4) | (b[4] >> 4);

    /* clang-format off */
    data = data_make(
            "model",         "",            DATA_STRING, "Thermopro-TX2",
            "subtype",       "",            DATA_INT, type,
            "id",            "",            DATA_INT, id,
            "channel",       "Channel",     DATA_INT, channel,
            "battery_ok",    "Battery",     DATA_INT, !battery,
            "temperature_C", "Temperature", DATA_FORMAT, "%.2f C", DATA_DOUBLE, temp_raw * 0.1,
            "humidity",      "Humidity",    DATA_COND, humidity != 0xcc, DATA_FORMAT, "%u %%", DATA_INT, humidity,
            "button",        "Button",      DATA_INT, button,
            NULL);
    /* clang-format on */
	//row = 0;    //particular case test row 0 and use another
    decoder_output_data(decoder, data, bitbuffer, row, nbRepeat, startPulses, package_type);
    return 1;
}

static uint8_t const *const output_fields[] = {
        "model",
        "subtype",
        "id",
        "channel",
        "battery_ok",
        "temperature_C",
        "humidity",
        "button",
        NULL,
};

r_device const thermopro_tx2 = {
        .name        = "ThermoPro-TX2 temperature sensor",
        .modulation  = OOK_PULSE_PPM,
        .short_width = 2000,
        .long_width  = 4000,
        .gap_limit   = 7000,
        .reset_limit = 10000,
        .decode_fn   = &thermopro_tx2_decode,
        .disabled    = 1,
        .priority    = 10, // Alecto collision, if Alecto checksum is correct it's not Prologue/ThermoPro-TX2
        .fields      = output_fields,
};
