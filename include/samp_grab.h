/** @file
    IQ sample grabber (ring buffer and dumper).

    Copyright (C) 2018 Christian Zuckschwerdt

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#ifndef INCLUDE_SAMP_GRAB_H_
#define INCLUDE_SAMP_GRAB_H_

#include <stdint.h>
#include "rtl_433.h"
typedef struct samp_grab {
    uint32_t *frequency;
    uint32_t *samp_rate;
    int32_t *sample_size;

    uint32_t sg_counter;
    uint8_t *sg_buf;
    uint32_t sg_size;
    uint32_t sg_index;
    uint32_t sg_len;
} samp_grab_t;

samp_grab_t *samp_grab_create(uint32_t size);

void samp_grab_free(samp_grab_t *g);

void samp_grab_push(samp_grab_t *g, uint8_t *iq_buf, uint32_t len);

void samp_grab_reset(samp_grab_t *g);

/// grab_end is counted in samples from end of buf.
void samp_grab_write(samp_grab_t *g, uint32_t grab_len, uint32_t grab_end);

#endif /* INCLUDE_SAMP_GRAB_H_ */
