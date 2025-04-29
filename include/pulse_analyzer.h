/** @file
    Pulse analyzer functions.

    Copyright (C) 2015 Tommy Vestermark

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#ifndef INCLUDE_PULSE_ANALYZER_H_
#define INCLUDE_PULSE_ANALYZER_H_

#include "pulse_detect.h"
#include "rtl_433.h"
struct r_device;
#define MAX_HIST_BINS 16

/// Histogram data for single bin
typedef struct {
	uint32_t count;
	int32_t sum;
	int32_t mean;
	int32_t min;
	int32_t max;
} hist_bin_t;

/// Histogram data for all bins
typedef struct {
	uint32_t bins_count;
	hist_bin_t bins[MAX_HIST_BINS];
} histogram_t;
/// Analyze and print result.
void pulse_analyzer(pulse_data_t *data, int32_t package_type, struct r_device *device, int32_t startPulses);
//#if _DEBUG
__declspec(dllexport) void  __stdcall  pulse_analyzerPlugin(pulse_data_t *data, histogram_t *hist_pulses, histogram_t *hist_gap, histogram_t *hist_periods, histogram_t *hist_timings);
//#endif
#endif /* INCLUDE_PULSE_ANALYZER_H_ */
