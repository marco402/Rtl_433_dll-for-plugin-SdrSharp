/** @file
    Ambient Weather F007TH Thermo-Hygrometer.

    contributed by David Ediger
    discovered by Ron C. Lewis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

/**
Decode Ambient Weather F007TH, F012TH, TF 30.3208.02, SwitchDoc F016TH.

Devices supported:

- Ambient Weather F007TH Thermo-Hygrometer.
- Ambient Weather F012TH Indoor/Display Thermo-Hygrometer.
- TFA senders 30.3208.02 from the TFA "Klima-Monitor" 30.3054,
- SwitchDoc Labs F016TH.

This decoder handles the 433mhz/868mhz thermo-hygrometers.
The 915mhz (WH*) family of devices use different modulation/encoding.


Byte 0   Byte 1   Byte 2   Byte 3   Byte 4   Byte 5
xxxxMMMM IIIIIIII BCCCTTTT TTTTTTTT HHHHHHHH MMMMMMMM

- x: Unknown 0x04 on F007TH/F012TH
- M: Model Number?, 0x05 on F007TH/F012TH/SwitchDocLabs F016TH
- I: ID byte (8 bits), volatie, changes at power up,
- B: Battery Low
- C: Channel (3 bits 1-8) - F007TH set by Dip switch, F012TH soft setting
- T: Temperature 12 bits - Fahrenheit * 10 + 400
- H: Humidity (8 bits)
- M: Message integrity check LFSR Digest-8, gen 0x98, key 0x3e, init 0x64

*/

#include "decoder.h"

static int32_t ambient_weather_decode(r_device *decoder, bitbuffer_t *bitbuffer, int32_t row, uint32_t bit_offset,int32_t startPulses, uint16_t package_type)
{
    uint8_t b[6];
    int32_t deviceID;
    int32_t isBatteryLow;
    int32_t channel;
    float temperature;
    int32_t humidity;
    data_t *data;

    bitbuffer_extract_bytes(bitbuffer, row, bit_offset, b, 6 * 8);

    uint8_t expected   = b[5];
    uint8_t calculated = lfsr_digest8(b, 5, 0x98, 0x3e) ^ 0x64;

    if (expected != calculated) {
        decoder_logf_bitrow(decoder, 1, __func__, b, 48, "Checksum error, expected: %02x calculated: %02x", expected, calculated);
        return DECODE_FAIL_MIC;
    }

    // int32_t model_number = b[0] & 0x0F; // fixed 0x05, at least for "SwitchDoc Labs F016TH"
    deviceID     = b[1];
    isBatteryLow = (b[2] & 0x80) != 0; // if not zero, battery is low
    channel      = ((b[2] & 0x70) >> 4) + 1;
    int32_t temp_f   = ((b[2] & 0x0f) << 8) | b[3];
    temperature  = (temp_f - 400) * 0.1f;
    humidity     = b[4];

    /*
    Sanity checks to reduce false positives and other bad data

    Packets with Bad data often pass the MIC check.

    - humidity > 100 (such as 255) and
    - temperatures > 140 F (such as 369.5 F and 348.8 F

    Specs in the F007TH and F012TH manuals state the range is:

    - Temperature: -40 to 140 F
    - Humidity: 10 to 99%

    @todo - sanity check b[0] "model number"

    - 0x45 - F007TH and F012TH
    - 0x?5 - SwitchDocLabs F016TH temperature sensor (based on comment b[0] & 0x0f == 5)
    - ? - TFA 30.3208.02

    */

    if (humidity < 0 || humidity > 100) {
        decoder_logf_bitrow(decoder, 1, __func__, b, 48, "Humidity failed sanity check 0x%02x", humidity);
        return DECODE_FAIL_SANITY;
    }

    if (temperature < -40.0 || temperature > 140.0) {
        decoder_logf_bitrow(decoder, 1, __func__, b, 48, "Temperature failed sanity check 0x%03x", temp_f);
        return DECODE_FAIL_SANITY;
    }


    /* clang-format off */
    data = data_make(
            "model",          "",             DATA_STRING, "Ambientweather-F007TH",
            "id",             "House Code",   DATA_INT,    deviceID,
            "channel",        "Channel",      DATA_INT,    channel,
            "battery_ok",     "Battery",      DATA_INT,    !isBatteryLow,
            "temperature_F",  "Temperature",  DATA_FORMAT, "%.1f F", DATA_DOUBLE, temperature,
            "humidity",       "Humidity",     DATA_FORMAT, "%u %%", DATA_INT, humidity,
            "mic",            "Integrity",    DATA_STRING, "CRC",
            NULL);
    /* clang-format on */


    decoder_output_data(decoder, data, bitbuffer, row, 0, startPulses, package_type); 
    return 1;
}

/**
Ambient Weather F007TH Thermo-Hygrometer.
@sa ambient_weather_decode()
*/
static int32_t ambient_weather_callback(r_device *decoder, bitbuffer_t *bitbuffer, int32_t startPulses, uint16_t package_type)
{
    // three repeats without gap
    // full preamble is 0x00145 (the last bits might not be fixed, e.g. 0x00146)
    // and on decoding also 0xffd45
    uint8_t const preamble_pattern[2]  = {0x01, 0x45}; // 12 bits
    uint8_t const preamble_inverted[2] = {0xfd, 0x45}; // 12 bits

    int32_t row=0;
    uint32_t bit_offset;
    int32_t ret = 0;

    for (row = 0; row < bitbuffer->num_rows; ++row) {
        bit_offset = 0;
        // Find a preamble with enough bits after it that it could be a complete packet
        while ((bit_offset = bitbuffer_search(bitbuffer, row, bit_offset, preamble_pattern, 12)) + 8 + 6 * 8 <=
                bitbuffer->bits_per_row[row]) {
            ret = ambient_weather_decode(decoder, bitbuffer, row, bit_offset + 8, startPulses, package_type);
            if (ret > 0)
                return ret; // for now, break after first successful message
            bit_offset += 16;
        }
        bit_offset = 0;
        while ((bit_offset = bitbuffer_search(bitbuffer, row, bit_offset, preamble_inverted, 12)) + 8 + 6 * 8 <=
                bitbuffer->bits_per_row[row]) {
            ret = ambient_weather_decode(decoder, bitbuffer, row, bit_offset + 8, startPulses, package_type);
            if (ret > 0)
                return ret; // for now, break after first successful message
            bit_offset += 15;
        }
    }

    // TODO: returns 0 when no data is found in the messages.
    // What would be a better return value? Maybe DECODE_ABORT_SANITY?
    return ret;
}

static uint8_t const *const output_fields[] = {
        "model",
        "id",
        "channel",
        "battery_ok",
        "temperature_F",
        "humidity",
        "mic",
        NULL,
};

r_device const ambient_weather = {
        .name        = "Ambient Weather F007TH, TFA 30.3208.02, SwitchDocLabs F016TH temperature sensor",
        .modulation  = OOK_PULSE_MANCHESTER_ZEROBIT,
        .short_width = 500,
        .long_width  = 0, // not used
        .reset_limit = 2400,
        .decode_fn   = &ambient_weather_callback,
        .fields      = output_fields,
};
