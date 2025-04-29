/** @file
    Baldr / RainPoint Rain Gauge protocol.

    Copyright (C) 2023 Christian W. Zuckschwerdt <zany@triq.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

*/

#include "decoder.h"

/**
Baldr / RainPoint Rain Gauge protocol.

For Baldr Wireless Weather Station with Rain Gauge.
See #2394

Only reports rain. There's a separate temperature sensor captured by Nexus-TH.

The sensor sends 36 bits 13 times,
the packets are ppm modulated (distance coding) with a pulse of ~500 us
followed by a short gap of ~1000 us for a 0 bit or a long ~2000 us gap for a
1 bit, the sync gap is ~4000 us.

Sample data:

    {36}75b000000 [0 mm]
    {36}75b000027 [0.9 mm]
    {36}75b000050 [2.0 mm]
    {36}75b8000cf [5.2 mm]
    {36}75b80017a [9.6 mm]
    {36}75b800224 [13.9 mm]
    {36}75b8002a3 [17.1 mm]

The data is grouped in 9 nibbles:

    II IF RR RR R

- I : 8 or 12-bit ID, could contain a model type nibble
- F : 4 bit, some flags
- R : 20 bit rain in inch/1000

*/

static int32_t baldr_rain_decode(r_device *decoder, bitbuffer_t *bitbuffer, int32_t startPulses, uint16_t package_type)
{
	uint32_t nbRepeat = 3;
	if (decoder->_sourceIsFile)
		nbRepeat = 0;
		
    int32_t row = bitbuffer_find_repeated_row(bitbuffer, nbRepeat, 36);
    if (row < 0)
        return DECODE_ABORT_EARLY;

    uint8_t *b = bitbuffer->bb[row];

    // we expect 36 bits but there might be a trailing 0 bit
    if (bitbuffer->bits_per_row[row] > 37)
        return DECODE_ABORT_LENGTH;

    // The baldr_rain protocol will trigger on rubicson data, so calculate the rubicson crc and make sure
    // it doesn't match. By guesstimate it should generate a correct crc 1/255% of the times.
    // So less then 0.5% which should be acceptable.
    if ((b[0] == 0 && b[2] == 0 && b[3] == 0)
            || (b[0] == 0xff &&  b[2] == 0xff && b[3] == 0xff))
        return DECODE_ABORT_EARLY;

    int32_t id      = (b[0] << 4) | (b[1] >> 4);
    int32_t flags   = (b[1] & 0x0f);
    int32_t rain_in = (b[2] << 12) | (b[3] << 4) | (b[4] >> 4);

    /* clang-format off */
    data_t *data = data_make(
            "model",        "",         DATA_STRING, "Baldr-Rain",
            "id",           "",         DATA_FORMAT, "%03x", DATA_INT, id,
            "flags",        "Flags",    DATA_FORMAT, "%x", DATA_INT, flags,
            "rain_in",      "Rain",     DATA_FORMAT, "%.3f in", DATA_DOUBLE, rain_in * 0.001,
            NULL);
    /* clang-format on */
    uint32_t bit_offset = 0;

    decoder_output_data(decoder, data, bitbuffer, row, nbRepeat, startPulses, package_type);
    return 1;
}

static uint8_t const *const output_fields[] = {
        "model",
        "id",
        "flags",
        "rain_in",
        NULL,
};

r_device const baldr_rain = {
        .name        = "Baldr / RainPoint rain gauge.",
        .modulation  = OOK_PULSE_PPM,
        .short_width = 1000,
        .long_width  = 2000,
        .gap_limit   = 3000,
        .reset_limit = 5000,
        .decode_fn   = &baldr_rain_decode,
		.priority = 10, // Eliminate false positives by letting Rubicson-Temperature go earlier
        .fields      = output_fields,
        .disabled    = 1, // no validity, no checksum
};
