/** @file
    AM signal analyzer.

    Copyright (C) 2018 Christian Zuckschwerdt

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#ifndef INCLUDE_AM_ANALYZE_H_
#define INCLUDE_AM_ANALYZE_H_

#include <stdint.h>
#include "samp_grab.h"
#include "rtl_433.h"
#define PULSE_DATA_SIZE 4000 /* maximum number of pulses */

typedef struct am_analyze {
    int32_t level_limit;
    int32_t override_short;
    int32_t override_long;
    uint32_t *frequency;
    uint32_t *samp_rate;
    int32_t *sample_size;

    /* state */
    uint32_t counter;
    uint32_t print;
    uint32_t print2;
    uint32_t pulses_found;
    uint32_t prev_pulse_start;
    uint32_t pulse_start;
    uint32_t pulse_end;
    uint32_t pulse_avg;
    uint32_t signal_start;
    uint32_t signal_pulse_counter;
    uint32_t signal_pulse_data[4000][3];
} am_analyze_t;

/// Create an AM-Analyzer. Might fail and return NULL.
am_analyze_t *am_analyze_create(void);

void am_analyze_free(am_analyze_t *a);

void am_analyze_skip(am_analyze_t *a, uint32_t n_samples);

void am_analyze(am_analyze_t *a, int16_t *am_buf, uint32_t n_samples, int32_t debug_output, samp_grab_t *g);

void am_analyze_classify(am_analyze_t *aa, int32_t debug_output);

#endif /* INCLUDE_AM_ANALYZE_H_ */
