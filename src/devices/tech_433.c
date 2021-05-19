/** @file
    Decoder for Digitech Tech-433 temperature sensor.

	manufacturer: Atech

	model name: Atech wireless weather station (presumed name, but not on the device, WS-308). On the outdoor sensor it says: 433 tech remote sensor

	information and photo:https://www.gitmemory.com/issue/RFD-FHEM/RFFHEM/547/474374179 

    Copyright (C) 2020 Marc Prieur https://github.com/marco402

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

The encoding is pulse position modulation
(i.e. gap width contains the modulation information)

- pulse high short gap
- pulse low long gap


	first invert all bits
	second decode as this:
		0---->0
		001-->1

A transmission package is:
	-preambule 8 "1"
	-very long gap
	-four identical packets 28 to 48 bits

-this code display 4 packets.

-after treatment
- b[1]: preamble (for synchronisation),  1100   
- b[2]:signe 2°bit
- b[3] centaines
- b[4] dizaines
- b[5] unités
- b[6] a check byte (the XOR of bytes 1-6 inclusive)
    each bit is effectively a parity bit for correspondingly positioned bit
    in the real message

	command line for me -f 433920000 -g 49.8 -vvv 2>test.txt

	example:	
for decoding

011011000000000011011000001100000011
 1  1 000000000 1  1 0000 1 00000 1
    
1100 0000 0001  1000 0100 0001
    c signe   1*100 8*10  4    parite
=184/10 degres

hexa   c    0    1    8    4    1
     b[1] b[2] b[3] b[4] b[5] b[6]

min length:0xc0000c-->000.0°
0110 1100 0000 0000 0000 0110 1100--->28
max length:0xc29997-->-99.9°
01101100 000110 01100011 01100011 01100011 0011011011->48
    c       2      9        9        9        7
for test
-r C:\marc\developpement\rtl_433\vs17_32\Debug\g001_433.92M_250k.cu8	
*/

#include "decoder.h"
#include "math.h"
static int tech_433_callback(r_device *decoder, bitbuffer_t *bitbuffer)
{
    const int LENMIN = 28;
    const int LENMAX = 48 + 2;
    data_t *data;
    uint8_t *b;
    int first_byte, byte2;
    float temp_c;
    int r = bitbuffer_find_repeated_row(bitbuffer, 1, LENMIN);
    if (r < 0) {
        if (decoder->verbose > 1) {
            fprintf(stderr, "%s: DECODE_ABORT_EARLY\n", __func__);
        }
        return DECODE_ABORT_EARLY;
    }
    b = (bitbuffer->bb[r]);
    if (decoder->verbose > 1)
        fprintf(stderr, "%s %u: LENGTH\n", __func__, bitbuffer->bits_per_row[r]);

    //b[] to allBitsIn
    unsigned int nbByte          = ceilf(bitbuffer->bits_per_row[r] / 8.0);
    unsigned int nbBit           = nbByte * 8;
    unsigned int shift           = nbBit - 8;
    unsigned long long allBitsIn = ((unsigned long long)(~b[0] & 0xff) << shift);
    shift -= 8;
    for (int bit = 1; bit < nbByte; bit++, shift -= 8) {
        allBitsIn += ((unsigned long long)(~b[bit] & 0xff) << shift);
    }
    //decode bits:  0->0    011->1 to allBitsOut

    //found synchro 01101100-->-->0x6c
    shift                          = nbBit - 8;
    unsigned long long synchro     = 0x6cULL;                             //0110 1100
    unsigned long long maskSynchro = (unsigned long long)(0xff) << shift; //  0xff00000000000000ULL;

    int firstBit = -1;

    for (int bit = nbBit; bit > 20; bit--) {                                           //20=mini:5*4  b[2] to b[6]
        if ((unsigned long long)((allBitsIn & maskSynchro) >> (bit - 8)) == synchro) { //7=len maskSynchro-1
            firstBit = bit;
            break;
        }
        maskSynchro = maskSynchro >> 1;
    }
    if (firstBit == -1)
        return DECODE_FAIL_SANITY;

    unsigned long long masque     = 7ULL;
    unsigned long long bitOne     = 3ULL;
    unsigned long long allBitsOut = 0;
    masque                        = masque << (firstBit - 3); //  4--->3
    int indexBitOut               = 24;
    for (int bit = firstBit; bit > (firstBit - bitbuffer->bits_per_row[r] +3 ); bit--) {
        if ((unsigned long long)((allBitsIn & masque) >> (bit - 2)) == bitOne) {
            allBitsOut += ((unsigned long long)(1 << indexBitOut));
            bit -= 2;             //skip 2 bit to one(011)
            masque = masque >> 2; //skip 2 bit to one(011)
        }
        masque = masque >> 1;
        indexBitOut -= 1;
    }

    //allBitsOut to b[]
    int nbNibble = 6;
    nbBit        = nbNibble * 4;
    shift        = nbBit - 4;
    int numB     = 11;  //not 1 for debug
    for (int bit = 0; bit < nbBit; bit += 4, numB += 1)
        b[numB] = (uint8_t)((allBitsOut / (1 << (shift - bit)))) & 0xf;

    //processing data
    //if (b[1] != 0x0c)        already tested with synchro
    //    return DECODE_FAIL_SANITY;

    if ((b[11] ^ b[12] ^ b[13] ^ b[14] ^ b[15] ^ b[16]) != 0) {
        if (decoder->verbose > 1) {
            fprintf(stderr, "%s: DECODE_FAIL_MIC\n", __func__);
        }
        return DECODE_FAIL_MIC;
    }
    first_byte = b[11];
    byte2      = b[12];
    if (b[12] & 2)
        temp_c = (float)-(((b[13] * 100) + (b[14] * 10) + b[15]) / 10.0);
    else
        temp_c = (float)(((b[13] * 100) + (b[14] * 10) + b[15]) / 10.0);

    data = data_make(
            "model", "", DATA_STRING, _X("tech_433", "Tech 433"),
            "id", "ID Code", DATA_INT, first_byte,
            "temperature_C", "Temperature", DATA_FORMAT, "%.01f C", DATA_DOUBLE, temp_c,
            "byte 2", "byte 2 (bit1=sign(T))", DATA_INT, byte2,
            "mic","Integrity", DATA_STRING, "CRC OK",
            NULL);
    decoder_output_data(decoder, data);
    return 1;
}

static char *output_fields[] = {
        "model",
        "id",
        "byte 2",
        "temperature_C",
        "mic",
        NULL,
};

r_device tech_433 = {
        .name        = "tech_433",
        .modulation  = OOK_PULSE_PPM,
        .short_width = 50*4,  //short gap  "0"   //*4=/0.25 for 0.25 samples /µs
        .long_width  = 500*4, //long gap   "1"  500->450
        .reset_limit = 2500*4, //2500->1000   1.9ms
        .tolerance   = 20*4,       //20->50
        .decode_fn   = &tech_433_callback,
        .fields      = output_fields,
};
