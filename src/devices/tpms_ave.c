/** @file
    AVE TPMS FSK 11 byte differential Manchester encoded CRC-8 TPMS data.

    Copyright (C) 2021 Pascal Charest

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

/**
AVE TPMS FSK 11 byte differential Manchester encoded CRC-8 TPMS data.


Packet nibbles:

    PRE    IIIIIIII PP TT FF  CC

- PRE = preamble is 0xff 0xfe
- I = sensor Id in hex
- P = Pressure (4 conversion tables available)
- T = Temperature (deg C offset by 50)
- F = Flags
--    mode: 2 bits, mode 0 and 1 are 2.35kPa per pressure bit, mode 2 and 3 are 5.5kPa
--    battery: 3 bits, 7 is low, 6 not full and all other is full
--    unknown: 3 bits, last bit seems to swap from time to time
- C = CRC-8 with poly 0x31 init 0xff (alternatively, 0xd3 and 0x1e)
*/

#include "decoder.h"

static int32_t tpms_ave_decode(r_device *decoder, bitbuffer_t *bitbuffer, int32_t row, uint32_t bitpos, int32_t startPulses, uint16_t package_type)
{
    bitbuffer_t packet_bits = {0};
    //uint8_t *b;
    //uint32_t id;
    //int32_t mode;
    //int32_t pressure_raw;
    //double pressure;
    //int32_t temperature;
    //int32_t battery_level;
    //int32_t flags;
    //int32_t crc;
    //double ratio;
    //double offset;

    bitbuffer_differential_manchester_decode(bitbuffer, row, bitpos, &packet_bits, 160);
	int32_t row1 = 0; // add plugin
    if (packet_bits.bits_per_row[row1] < 64) {  // to see why not row=0?
        return DECODE_ABORT_LENGTH; // too short to be a whole packet
    }
    decoder_log_bitbuffer(decoder, 1, __func__, &packet_bits, "", 0, 0);

    //b = packet_bits.bb[row1];
	uint8_t *b = packet_bits.bb[row];
    //id            = (uint32_t)b[0] << 24 | b[1] << 16 | b[2] << 8 | b[3];
    //pressure_raw  = b[4];
    //temperature   = b[5];
    //mode          = b[6] >> 6 & 0x3;
    //battery_level = b[6] >> 3 & 0x7;
    //flags         = b[6] & 0x7;
    //crc           = b[7];

	if (crc8(b, 8, 0x31, 0xff) != 0) {
        return DECODE_FAIL_MIC; // bad checksum
    }
	uint32_t id = (uint32_t)b[0] << 24 | b[1] << 16 | b[2] << 8 | b[3];
	int32_t pressure_raw = b[4];
	int32_t temperature = b[5];
	int32_t mode = b[6] >> 6 & 0x3;
	int32_t battery_raw = b[6] >> 3 & 0x7;
	int32_t flags = b[6] & 0x7;
	    // int32_t crc          = b[7];
		
		int32_t battery_pct = 100; // any other value, i.e. less than 6
	if (battery_raw == 6) {
		battery_pct = 75; // not full
		
	}
	else if (battery_raw == 7) {
		battery_pct = 25; // low
		
	}
	
	double ratio;
	double offset;
    switch (mode) {
    case 0:
        ratio  = 2.352f;
        offset = 47.0f;
        break;
    case 1:
    default:
        ratio  = 2.352f;
        offset = 0.0f;
        break;
    case 2:
        ratio  = 5.491f;
        offset = 18.2f;
        break;
    case 3:
        ratio  = 5.491f;
        offset = 0.0f;
        break;
    }
	double pressure = ((double)pressure_raw - offset) * ratio;

    uint8_t id_str[9 + 1];
    snprintf(id_str, sizeof(id_str), "%08x", id);

    /* clang-format off */
    data_t *data = data_make(
            "model",            "Model",         DATA_STRING, "AVE",
            "type",             "Type",          DATA_STRING, "TPMS",
            "id",               "Id",            DATA_STRING, id_str,
            "mode",             "Mode",          DATA_FORMAT, "M%i", DATA_INT, mode,
            "pressure_kPa",     "Pressure",      DATA_FORMAT, "%.1f kPa", DATA_DOUBLE, pressure,
            "temperature_C",    "Temperature",   DATA_FORMAT, "%.0f C", DATA_DOUBLE, (double)temperature - 50.0,
		    "battery_ok",       "Battery",       DATA_INT, battery_raw != 7, // Value 7 means "Low"
		    "battery_pct",      "Battery level", DATA_INT, battery_pct, // Note: this might change with #3103            "flags",            "Flags",         DATA_FORMAT, "0x%x", DATA_INT, flags,
		    "flags",            "Flags",         DATA_FORMAT, "0x%x", DATA_INT, flags,
            "mic",              "Integrity",     DATA_STRING, "CRC",
            NULL);
    /* clang-format on */

        
    decoder_output_data(decoder, data, &packet_bits, row1, 0, startPulses, package_type);
    return 1;
}

/**
Wrapper for the AVE tpms.
@sa tpms_ave_decode()
*/
static int32_t tpms_ave_callback(r_device *decoder, bitbuffer_t *bitbuffer, int32_t startPulses, uint16_t package_type)
{
    uint8_t const preamble_pattern[] = {0xcc, 0xcc, 0xcc, 0xcd}; // Raw pattern, before differential Manchester coding

    int32_t row;
    uint32_t bitpos;
    int32_t ret    = 0;
    int32_t events = 0;

    for (row = 0; row < bitbuffer->num_rows; ++row) {
		bitpos = 0;
        // Find a preamble with enough bits after it that it could be a complete packet
		//0 or row?
        while ((bitpos = bitbuffer_search(bitbuffer, 0, bitpos, preamble_pattern, 32)) + 132 <= bitbuffer->bits_per_row[row]) {
            ret = tpms_ave_decode(decoder, bitbuffer, row, bitpos + 32, startPulses, package_type);
            if (ret > 0) {
                events += ret;
				bitpos += 132;
            }
			bitpos += 31;
        }
    }

    return events > 0 ? events : ret;
}

static uint8_t const *const output_fields[] = {
        "model",
        "type",
        "id",
		"battery_ok",
        "battery_pct",
        "mode",
        "pressure_kPa",
        "temperature_C",
        "battery_level",
        "flags",
        "mic",
        NULL,
};

r_device const tpms_ave = {
        .name        = "AVE TPMS",
        .modulation  = FSK_PULSE_PCM,
        .short_width = 100,
        .long_width  = 100,
        .reset_limit = 400,
        .tolerance   = 15,
        .decode_fn   = &tpms_ave_callback,
        .fields      = output_fields,
};
