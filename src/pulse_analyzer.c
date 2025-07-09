/** @file
    Pulse analyzer functions.

    Copyright (C) 2015 Tommy Vestermark

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/
#include "decoder_util.h"
#include "r_util.h"
#include "r_api.h"
#include "pulse_analyzer.h"
#include "pulse_slicer.h"
#include "bit_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

//#define MAX_HIST_BINS 16
//
///// Histogram data for single bin
//typedef struct {
//    uint32_t count;
//    int32_t sum;
//    int32_t mean;
//    int32_t min;
//    int32_t max;
//} hist_bin_t;
//
///// Histogram data for all bins
//typedef struct {
//    uint32_t bins_count;
//    hist_bin_t bins[MAX_HIST_BINS];
//} histogram_t;

/// Generate a histogram (unsorted)
static void histogram_sum(histogram_t *hist, int32_t const *data, uint32_t len, float tolerance)
{
    uint32_t bin;    // Iterator will be used outside for!

    for (uint32_t n = 0; n < len; ++n) {
        // Search for match in existing bins
        for (bin = 0; bin < hist->bins_count; ++bin) {
            int32_t bn = data[n];
            int32_t bm = hist->bins[bin].mean;
            if (abs(bn - bm) < (tolerance * MAX(bn, bm))) {
                hist->bins[bin].count++;
                hist->bins[bin].sum += data[n];
                hist->bins[bin].mean = hist->bins[bin].sum / hist->bins[bin].count;
                hist->bins[bin].min    = MIN(data[n], hist->bins[bin].min);
                hist->bins[bin].max    = MAX(data[n], hist->bins[bin].max);
                break;    // Match found! Data added to existing bin
            }
        }
        // No match found? Add new bin
        if (bin == hist->bins_count && bin < MAX_HIST_BINS) {
            hist->bins[bin].count    = 1;
            hist->bins[bin].sum        = data[n];
            hist->bins[bin].mean    = data[n];
            hist->bins[bin].min        = data[n];
            hist->bins[bin].max        = data[n];
            hist->bins_count++;
        } // for bin
    } // for data
}

/// Delete bin from histogram
static void histogram_delete_bin(histogram_t *hist, uint32_t index)
{
    hist_bin_t const zerobin = {0};
    if (hist->bins_count < 1) return;    // Avoid out of bounds
    // Move all bins afterwards one forward
    for (uint32_t n = index; n < hist->bins_count-1; ++n) {
        hist->bins[n] = hist->bins[n+1];
    }
    hist->bins_count--;
    hist->bins[hist->bins_count] = zerobin;    // Clear previously last bin
}


/// Swap two bins in histogram
static void histogram_swap_bins(histogram_t *hist, uint32_t index1, uint32_t index2)
{
    hist_bin_t    tempbin;
    if ((index1 < hist->bins_count) && (index2 < hist->bins_count)) {        // Avoid out of bounds
        tempbin = hist->bins[index1];
        hist->bins[index1] = hist->bins[index2];
        hist->bins[index2] = tempbin;
    }
}


/// Sort histogram with mean value (order lowest to highest)
static void histogram_sort_mean(histogram_t *hist)
{
    if (hist->bins_count < 2) return;        // Avoid underflow
    // Compare all bins (bubble sort)
    for (uint32_t n = 0; n < hist->bins_count-1; ++n) {
        for (uint32_t m = n+1; m < hist->bins_count; ++m) {
            if (hist->bins[m].mean < hist->bins[n].mean) {
                histogram_swap_bins(hist, m, n);
            }
        }
    }
}


/// Sort histogram with count value (order lowest to highest)
static void histogram_sort_count(histogram_t *hist)
{
    if (hist->bins_count < 2) return;        // Avoid underflow
    // Compare all bins (bubble sort)
    for (uint32_t n = 0; n < hist->bins_count-1; ++n) {
        for (uint32_t m = n+1; m < hist->bins_count; ++m) {
            if (hist->bins[m].count < hist->bins[n].count) {
                histogram_swap_bins(hist, m, n);
            }
        }
    }
}


/// Fuse histogram bins with means within tolerance
static void histogram_fuse_bins(histogram_t *hist, float tolerance)
{
    if (hist->bins_count < 2) return;        // Avoid underflow
    // Compare all bins
    for (uint32_t n = 0; n < hist->bins_count-1; ++n) {
        for (uint32_t m = n+1; m < hist->bins_count; ++m) {
            int32_t bn = hist->bins[n].mean;
            int32_t bm = hist->bins[m].mean;
            // if within tolerance
            if (abs(bn - bm) < (tolerance * MAX(bn, bm))) {
                // Fuse data for bin[n] and bin[m]
                hist->bins[n].count += hist->bins[m].count;
                hist->bins[n].sum    += hist->bins[m].sum;
                hist->bins[n].mean    = hist->bins[n].sum / hist->bins[n].count;
                hist->bins[n].min    = MIN(hist->bins[n].min, hist->bins[m].min);
                hist->bins[n].max    = MAX(hist->bins[n].max, hist->bins[m].max);
                // Delete bin[m]
                histogram_delete_bin(hist, m);
                m--;    // Compare new bin in same place!
            }
        }
    }
}

/// Find bin index
static int32_t histogram_find_bin_index(histogram_t const *hist, int32_t width)
{
    for (uint32_t n = 0; n < hist->bins_count; ++n) {
        if (hist->bins[n].min <= width && width <= hist->bins[n].max) {
            return n;
        }
    }
    return -1;
}

/// Print a histogram
static int32_t histogram_print(histogram_t const *hist, uint32_t samp_rate)
{
	uint32_t maxCount = 0;
	int32_t mean = 0;
    for (uint32_t n = 0; n < hist->bins_count; ++n) {
		if (hist->bins[n].count > maxCount)
		{
			maxCount = hist->bins[n].count > maxCount;
			mean = (int32_t) (hist->bins[n].mean * 1e6 / samp_rate);
		}


		 
       /* fprintf(stderr, " [%2u] count: %4u,  width: %4.0f us [%.0f;%.0f]\t(%4i S)\n", n,
                hist->bins[n].count,
                hist->bins[n].mean * 1e6 / samp_rate,
                hist->bins[n].min * 1e6 / samp_rate,
                hist->bins[n].max * 1e6 / samp_rate,
                hist->bins[n].mean);*/
    }
	return mean;
}

#define HEXSTR_BUILDER_SIZE 1024
#define HEXSTR_MAX_COUNT 32

/// Hex string builder
typedef struct hexstr {
    uint8_t p[HEXSTR_BUILDER_SIZE];
    uint32_t idx;
} hexstr_t;

static void hexstr_push_byte(hexstr_t *h, uint8_t v)
{
    if (h->idx < HEXSTR_BUILDER_SIZE)
        h->p[h->idx++] = v;
}

static void hexstr_push_word(hexstr_t *h, uint16_t v)
{
    if (h->idx + 1 < HEXSTR_BUILDER_SIZE) {
        h->p[h->idx++] = v >> 8;
        h->p[h->idx++] = v & 0xff;
    }
}

static void hexstr_print(hexstr_t *h, FILE *out)
{
    for (uint32_t i = 0; i < h->idx; ++i)
        fprintf(out, "%02X", h->p[i]);
}

#define TOLERANCE (0.2f) // 20% tolerance should still discern between the pulse widths: 0.33, 0.66, 1.0


__declspec(dllexport) void  __stdcall  pulse_analyzerPlugin(pulse_data_t *data, histogram_t *hist_pulses, histogram_t *hist_gaps, histogram_t *hist_periods, histogram_t *hist_timings)
{
#if ANALYZE   //else pb link name to .def
	if (data->num_pulses < MINPULSES) {
		return;
	}

	//if (data->num_pulses == 0) {
	//    fprintf(stderr, "No pulses detected.\n");
	//    return;
	//}
	razKeyValueDeviceToPlugin();
	double to_ms = 1e3 / data->sample_rate;
	double to_us = 1e6 / data->sample_rate;
	// Generate pulse period data
	int32_t pulse_total_period = 0;
	pulse_data_t pulse_periods = { 0 };
	pulse_periods.num_pulses = data->num_pulses;
	for (uint32_t n = 0; n < pulse_periods.num_pulses; ++n) {
		pulse_periods.pulse[n] = data->pulse[n] + data->gap[n];
		pulse_total_period += data->pulse[n] + data->gap[n];
	}
	pulse_total_period -= data->gap[pulse_periods.num_pulses - 1];

	//histogram_t hist_pulses = { 0 };
	//histogram_t hist_gaps = { 0 };
	//histogram_t hist_periods = { 0 };
	//histogram_t hist_timings = { 0 };

	// Generate statistics
	histogram_sum(hist_pulses, data->pulse, data->num_pulses, TOLERANCE);
	histogram_sum(hist_gaps, data->gap, data->num_pulses - 1, TOLERANCE);                      // Leave out last gap (end)
	histogram_sum(hist_periods, pulse_periods.pulse, pulse_periods.num_pulses - 1, TOLERANCE); // Leave out last gap (end)
	histogram_sum(hist_timings, data->pulse, data->num_pulses, TOLERANCE);
	histogram_sum(hist_timings, data->gap, data->num_pulses, TOLERANCE);

	// Fuse overlapping bins
	histogram_fuse_bins(hist_pulses, TOLERANCE);
	histogram_fuse_bins(hist_gaps, TOLERANCE);
	histogram_fuse_bins(hist_periods, TOLERANCE);
	histogram_fuse_bins(hist_timings, TOLERANCE);

	//*hist_pulses_bins_count = hist_pulses.bins_count;
	//*hist_gap_bins_count = hist_gaps.bins_count;
	//*hist_periods_bins_count = hist_periods.bins_count;
#endif

}
//#endif


/// Analyze the statistics of a pulse data structure and print result
void pulse_analyzer(pulse_data_t *data, int32_t package_type, r_device* device,int32_t startPulses)
{
#if ANALYZE
if (data->num_pulses < MINPULSES) {
        return;
    }

    //if (data->num_pulses == 0) {
    //    fprintf(stderr, "No pulses detected.\n");
    //    return;
    //}
	razKeyValueDeviceToPlugin();
    double to_ms = 1e3 / data->sample_rate;
    double to_us = 1e6 / data->sample_rate;
    // Generate pulse period data
    int32_t pulse_total_period = 0;
    pulse_data_t pulse_periods = {0};
    pulse_periods.num_pulses = data->num_pulses;
    for (uint32_t n = 0; n < pulse_periods.num_pulses; ++n) {
        pulse_periods.pulse[n] = data->pulse[n] + data->gap[n];
        pulse_total_period += data->pulse[n] + data->gap[n];
    }
    pulse_total_period -= data->gap[pulse_periods.num_pulses - 1];

    histogram_t hist_pulses  = {0};
    histogram_t hist_gaps    = {0};
    histogram_t hist_periods = {0};
    histogram_t hist_timings = {0};

    // Generate statistics
    histogram_sum(&hist_pulses, data->pulse, data->num_pulses, TOLERANCE);
    histogram_sum(&hist_gaps, data->gap, data->num_pulses - 1, TOLERANCE);                      // Leave out last gap (end)
    histogram_sum(&hist_periods, pulse_periods.pulse, pulse_periods.num_pulses - 1, TOLERANCE); // Leave out last gap (end)
    histogram_sum(&hist_timings, data->pulse, data->num_pulses, TOLERANCE);
    histogram_sum(&hist_timings, data->gap, data->num_pulses, TOLERANCE);

    // Fuse overlapping bins
    histogram_fuse_bins(&hist_pulses, TOLERANCE);
    histogram_fuse_bins(&hist_gaps, TOLERANCE);
    histogram_fuse_bins(&hist_periods, TOLERANCE);
    histogram_fuse_bins(&hist_timings, TOLERANCE);





 /*   fprintf(stderr, "Analyzing pulses...\n");
    fprintf(stderr, "Total count: %4u,  width: %4.2f ms\t\t(%5i S)\n",
            data->num_pulses, pulse_total_period * to_ms, pulse_total_period);*/
	//uint8_t str[30];
	//sprintf(str, "%d", data->num_pulses);
	//AddKeyValueDevice("Total count:", str);
	AddKeyValueDevice("Name:", "Analyzer Device");

	uint8_t str[LENLINES];
	r_cfg_t *cfg = device->output_ctx;
	uint8_t time_str[LOCAL_TIME_BUFLEN];
	time_pos_str(cfg, 0, time_str);
	AddKeyValueDevice("time", time_str);




	int32_t period = (int32_t)(pulse_total_period * to_ms);
	sprintf(str, "%d", period);
	AddKeyValueDevice("period:", str);
	sprintf(str, "%d", pulse_total_period);
	AddKeyValueDevice("duration(ms):", str);
	sprintf(str, "%d", data->num_pulses);
	AddKeyValueDevice("nb_pulses", str);
    //fprintf(stderr, "Pulse width distribution:\n");
	sprintf(str, "%d", histogram_print(&hist_pulses, data->sample_rate));
	AddKeyValueDevice("Mean pulses max:", str);
    //histogram_print(&hist_pulses, data->sample_rate);
    //fprintf(stderr, "Gap width distribution:\n");
    /*histogram_print(&hist_gaps, data->sample_rate)*/;
	sprintf(str, "%d", histogram_print(&hist_gaps, data->sample_rate));
	AddKeyValueDevice("Mean gap max:", str); 
    //fprintf(stderr, "Pulse period distribution:\n");
    histogram_print(&hist_periods, data->sample_rate);
    //fprintf(stderr, "Pulse timing distribution:\n");
    histogram_print(&hist_timings, data->sample_rate);
	sprintf(str, "%.1f db", data->rssi_db);
	AddKeyValueDevice("RSSI:", str);
	sprintf(str, "%.1f db", data->snr_db);
	AddKeyValueDevice("SNR:", str);
	sprintf(str, "%.1f db", data->noise_db);
	AddKeyValueDevice("Noise:", str);
	//sprintf(str, "%.0f hz",data->freq1_hz + (float)(  data->fsk_f1_est /32767.0 * data->sample_rate / 2.0));//INT16_MAXdata->freq1_hz/1000000
	//AddKeyValueDevice("freq:", str);

	//sprintf(str, "%3f khz", (float)data->fsk_f1_est / INT16_MAX * data->sample_rate / 2.0 / 1000.0);//data->freq1_hz/1000000
	//AddKeyValueDevice("Dfreq1:", str);
	//sprintf(str, "%3f khz", (float)data->fsk_f2_est / INT16_MAX * data->sample_rate / 2.0 / 1000.0);  //data->freq2_hz/1000000
	//AddKeyValueDevice("Dfreq2:", str);
 /*   fprintf(stderr, "Level estimates [high, low]: %6i, %6i\n",
            data->ook_high_estimate, data->ook_low_estimate);
    fprintf(stderr, "RSSI: %.1f dB SNR: %.1f dB Noise: %.1f dB\n",
            data->rssi_db, data->snr_db, data->noise_db);
    fprintf(stderr, "Frequency offsets [F1, F2]:  %6i, %6i\t(%+.1f kHz, %+.1f kHz)\n",
            data->fsk_f1_est, data->fsk_f2_est,
            (float)data->fsk_f1_est / INT16_MAX * data->sample_rate / 2.0 / 1000.0,
            (float)data->fsk_f2_est / INT16_MAX * data->sample_rate / 2.0 / 1000.0);*/

    //fprintf(stderr, "Guessing modulation: ");
	device->name = "Analyzer Device";
	device->verbose = 2;
    ////histogram_sort_mean(&hist_pulses); // Easier to work with sorted data
    ////histogram_sort_mean(&hist_gaps);
    if (hist_pulses.bins[0].mean == 0) { 
        histogram_delete_bin(&hist_pulses, 0);
    } // Remove FSK initial zero-bin

	sprintf(str, "%d ", hist_pulses.bins_count);
	AddKeyValueDevice("hist_pulses.bins_count:", str);
	sprintf(str, "%d ", hist_gaps.bins_count);
	AddKeyValueDevice("hist_gaps.bins_count:", str);
	sprintf(str, "%d ", hist_periods.bins_count);
	AddKeyValueDevice("hist_periods.bins_count:", str);
//***********************************************************************************************************

	uint32_t maxPulses = hist_pulses.bins[0].count;
	int32_t nPulseMax = 0;
	for (uint32_t i = 1; i < hist_pulses.bins_count; i++)
	{
		if (hist_pulses.bins[i].count > maxPulses)
		{
			maxPulses = hist_pulses.bins[0].count;
			nPulseMax = i;
		}
	}
	int32_t nGapMax = 0;
	uint32_t maxGap = hist_gaps.bins[0].count;
	for (uint32_t i = 1; i < hist_gaps.bins_count; i++)
	{
		if (hist_gaps.bins[i].count > maxGap)
		{
			maxGap = hist_gaps.bins[0].count;
			nGapMax = i;
		}
	}

	//  FSK_PULSE_MANCHESTER_ZEROBIT	pulse = gap						N pulses = N gap
	//	FSK_PULSE_PCM					gap = pulse / 2					N pulses = N gap
	//	FSK_PULSE_PWM					gap = pulse / 2					N pulses < N gap
	//	OOK_PULSE_DMC					pulse = gap						N pulses = N gap
	//	OOK_PULSE_MANCHESTER_ZEROBIT	gap = 2  pulses					N pulses > N gap
	//	OOK_PULSE_PCM or OOK_PULSE_RZ	pulse = 4 cinquiemes gap
	//	OOK_PULSE_PIWM_DC				pulses = gap					N pulses > N gap
	//	OOK_PULSE_PPM					gap = 3 pulses
	//	OOK_PULSE_PWM					gap = 2 pulses					N pulses = N gap
	//	OOK_PULSE_PWM_OSV1				gap = 2 tiers  pulse


	//if (data->num_pulses == 1) {
	//	fprintf(stderr, "Single pulse detected. Probably Frequency Shift Keying or just noise...\n");
	//}
	//else if (hist_pulses.bins_count == 1 && hist_gaps.bins_count == 1) {
	//	fprintf(stderr, "Un-modulated signal. Maybe a preamble...\n");
	//}
	//else if (hist_gaps.bins[nGapMax].mean == hist_pulses.bins[nPulseMax].mean)
	//{
	//	if (hist_gaps.bins[nGapMax].count == hist_pulses.bins[nPulseMax].count)
	//	{
	//		//FSK_PULSE_MANCHESTER_ZEROBIT or OOK_PULSE_DMC
	//		device->modulation = FSK_PULSE_MANCHESTER_ZEROBIT; // TODO: there is not FSK_PULSE_PPM
	//		device->short_width = (float)(to_us * hist_gaps.bins[2].mean / 2.0);  //0-->1
	//		device->long_width = (float)(to_us * hist_gaps.bins[2].mean);  //1-->2
	//		device->gap_limit = (float)(to_us * (hist_gaps.bins[2].max + 1));   // marc +10 1-->2                       // Set limit above next lower gap
	//		device->reset_limit = (float)(to_us * (hist_gaps.bins[2].max + 1)); // marc +10 [hist_
	//	}
	//	else if (hist_gaps.bins[nGapMax].count < hist_pulses.bins[nPulseMax].count)
	//	{
	//		//OOK_PULSE_PIWM_DC
	//		device->modulation = OOK_PULSE_PIWM_DC; // TODO: there is not FSK_PULSE_PPM
	//		device->short_width = (float)(to_us * hist_gaps.bins[2].mean / 2.0);  //0-->1
	//		device->long_width = (float)(to_us * hist_gaps.bins[2].mean);  //1-->2
	//		device->gap_limit = (float)(to_us * (hist_gaps.bins[2].max + 1));   // marc +10 1-->2                       // Set limit above next lower gap
	//		device->reset_limit = (float)(to_us * (hist_gaps.bins[2].max + 1)); // marc +10 [hist_
	//	}

	//}
	//else if (hist_gaps.bins[nGapMax].mean == hist_pulses.bins[nPulseMax].mean / 2)
	//{
	//	if (hist_gaps.bins[nGapMax].count == hist_pulses.bins[nPulseMax].count)
	//	{
	//		//FSK_PULSE_PCM
	//		device->modulation = FSK_PULSE_PCM; // TODO: there is not FSK_PULSE_PPM
	//		device->short_width = (float)(to_us * hist_gaps.bins[2].mean / 2.0);  //0-->1
	//		device->long_width = (float)(to_us * hist_gaps.bins[2].mean);  //1-->2
	//		device->gap_limit = (float)(to_us * (hist_gaps.bins[2].max + 1));   // marc +10 1-->2                       // Set limit above next lower gap
	//		device->reset_limit = (float)(to_us * (hist_gaps.bins[2].max + 1)); // marc +10 [hist_
	//	}
	//	else if (hist_gaps.bins[nGapMax].count > hist_pulses.bins[nPulseMax].count)
	//	{
	//		//	FSK_PULSE_PWM
	//		device->modulation = FSK_PULSE_PWM; // TODO: there is not FSK_PULSE_PPM
	//		device->short_width = (float)(to_us * hist_gaps.bins[2].mean / 2.0);  //0-->1
	//		device->long_width = (float)(to_us * hist_gaps.bins[2].mean);  //1-->2
	//		device->gap_limit = (float)(to_us * (hist_gaps.bins[2].max + 1));   // marc +10 1-->2                       // Set limit above next lower gap
	//		device->reset_limit = (float)(to_us * (hist_gaps.bins[2].max + 1)); // marc +10 [hist_
	//	}
	//}
	//else if ((hist_gaps.bins[nGapMax].mean > ((hist_pulses.bins[nPulseMax].mean * 3)- 100 )) && (hist_gaps.bins[nGapMax].mean < ((hist_pulses.bins[nPulseMax].mean * 3) + 100 )))
	//{
	//	//OOK_PULSE_PPM (inFactory-TH)
	//	device->modulation = OOK_PULSE_PPM;
	//	device->short_width = (float)(to_us * hist_gaps.bins[nGapMax].mean);
	//	device->long_width = (float)(to_us * hist_gaps.bins[nGapMax].mean * 2);
	//	device->gap_limit = 0.0;
	//	device->reset_limit = (float)(to_us * (device->long_width *0.3));
	//}
	//else if (hist_gaps.bins[nGapMax].mean == hist_pulses.bins[nPulseMax].mean * 2)
	//{
	//	if (hist_gaps.bins[nGapMax].count == hist_pulses.bins[nPulseMax].count)
	//	{
	//		//	OOK_PULSE_PWM
	//		device->modulation = OOK_PULSE_PWM; // TODO: there is not FSK_PULSE_PPM
	//		device->short_width = (float)(to_us * hist_gaps.bins[2].mean / 2.0);  //0-->1
	//		device->long_width = (float)(to_us * hist_gaps.bins[2].mean);  //1-->2
	//		device->gap_limit = (float)(to_us * (hist_gaps.bins[2].max + 1));   // marc +10 1-->2                       // Set limit above next lower gap
	//		device->reset_limit = (float)(to_us * (hist_gaps.bins[2].max + 1)); // marc +10 [hist_
	//	}
	//	else if (hist_gaps.bins[nGapMax].count > hist_pulses.bins[nPulseMax].count)
	//	{
	//		//	OOK_PULSE_MANCHESTER_ZEROBIT
	//		device->modulation = OOK_PULSE_MANCHESTER_ZEROBIT; // TODO: there is not FSK_PULSE_PPM
	//		device->short_width = (float)(to_us * hist_gaps.bins[2].mean / 2.0);  //0-->1
	//		device->long_width = (float)(to_us * hist_gaps.bins[2].mean);  //1-->2
	//		device->gap_limit = (float)(to_us * (hist_gaps.bins[2].max + 1));   // marc +10 1-->2                       // Set limit above next lower gap
	//		device->reset_limit = (float)(to_us * (hist_gaps.bins[2].max + 1)); // marc +10 [hist_
	//	}
	//}
	//else if (hist_gaps.bins[nGapMax].mean == hist_pulses.bins[nPulseMax].mean * 0 * 666)
	//{
	//	//OOK_PULSE_PWM_OSV1
	//	device->modulation = OOK_PULSE_PWM_OSV1; // TODO: there is not FSK_PULSE_PPM
	//	device->short_width = (float)(to_us * hist_gaps.bins[2].mean / 2.0);  //0-->1
	//	device->long_width = (float)(to_us * hist_gaps.bins[2].mean);  //1-->2
	//	device->gap_limit = (float)(to_us * (hist_gaps.bins[2].max + 1));   // marc +10 1-->2                       // Set limit above next lower gap
	//	device->reset_limit = (float)(to_us * (hist_gaps.bins[2].max + 1)); // marc +10 [hist_
	//}
	//else  if (hist_gaps.bins[nGapMax].mean == hist_pulses.bins[nPulseMax].mean * 0 * 8)
	//{
	//	//	OOK_PULSE_PCM or OOK_PULSE_RZ
	//	device->modulation = OOK_PULSE_RZ; // TODO: there is not FSK_PULSE_PPM
	//	device->short_width = (float)(to_us * hist_gaps.bins[2].mean / 2.0);  //0-->1
	//	device->long_width = (float)(to_us * hist_gaps.bins[2].mean);  //1-->2
	//	device->gap_limit = (float)(to_us * (hist_gaps.bins[2].max + 1));   // marc +10 1-->2                       // Set limit above next lower gap
	//	device->reset_limit = (float)(to_us * (hist_gaps.bins[2].max + 1)); // marc +10 [hist_
	//}
//***********************************************************************************************************
	//for (int32_t modul=0;modul<7;modul++)
	//{
		// Attempt to find a matching modulation
		if (data->num_pulses == 1) {
			fprintf(stderr, "Single pulse detected. Probably Frequency Shift Keying or just noise...\n");
		}
		else if (hist_pulses.bins_count == 1 && hist_gaps.bins_count == 1) {
			fprintf(stderr, "Un-modulated signal. Maybe a preamble...\n");
		}
		else if (hist_gaps.bins_count > 2){  //(hist_pulses.bins_count == 1 && hist_gaps.bins_count > 2) {  //1->2
			//fprintf(stderr, "Pulse Position Modulation with fixed pulse width\n");
			device->modulation = OOK_PULSE_PPM; // TODO: there is not FSK_PULSE_PPM
			device->short_width = (float)(to_us * hist_gaps.bins[2].mean / 2.0);  //0-->1
			device->long_width = (float)(to_us * hist_gaps.bins[2].mean);  //1-->2
			device->gap_limit = (float)(to_us * (hist_gaps.bins[2].max + 1));   // marc +10 1-->2                       // Set limit above next lower gap
			device->reset_limit = (float)(to_us * (hist_gaps.bins[2].max + 1)); // marc +10 [hist_gaps.bins_count - 1].max + 1)->2 Set limit above biggest gap
		}
		else if (hist_pulses.bins_count > 1 && hist_gaps.bins_count > 0) {  //(hist_pulses.bins_count > 2 && hist_gaps.bins_count > 1) {
			//fprintf(stderr, "Pulse Width Modulation with fixed gap\n");
			device->modulation = (package_type == PULSE_DATA_FSK) ? FSK_PULSE_PWM : OOK_PULSE_PWM;
			device->short_width = (float)(to_us * hist_pulses.bins[0].mean);
			device->long_width = (float)(to_us * hist_pulses.bins[1].mean);
			device->tolerance = (float)((device->long_width - device->short_width) * 0.4);
			device->reset_limit = (float)(to_us * (hist_gaps.bins[hist_gaps.bins_count - 1].max + 1)); // Set limit above biggest gap
		}
		else if (hist_pulses.bins_count > 1 && hist_gaps.bins_count > 1) {  //(hist_pulses.bins_count == 2 && hist_gaps.bins_count == 2 && hist_periods.bins_count == 1) {
			//fprintf(stderr, "Pulse Width Modulation with fixed period\n");
			device->modulation = (package_type == PULSE_DATA_FSK) ? FSK_PULSE_PWM : OOK_PULSE_PWM;
			device->short_width = (float)(to_us * hist_pulses.bins[0].mean);
			device->long_width = (float)(to_us * hist_pulses.bins[1].mean);
			device->tolerance = (float)((device->long_width - device->short_width) * 0.4);
			device->reset_limit = (float)(to_us * (hist_gaps.bins[hist_gaps.bins_count - 1].max + 1)); // Set limit above biggest gap
		}
		else if (hist_pulses.bins_count >1 && hist_gaps.bins_count > 2 && hist_periods.bins_count == 3) {  //(hist_pulses.bins_count == 2 && hist_gaps.bins_count == 2 && hist_periods.bins_count == 3) {
			//fprintf(stderr, "Manchester coding\n");
			device->modulation = (package_type == PULSE_DATA_FSK) ? FSK_PULSE_MANCHESTER_ZEROBIT : OOK_PULSE_MANCHESTER_ZEROBIT;
			device->short_width = (float)(to_us * MIN(hist_pulses.bins[0].mean, hist_pulses.bins[1].mean)); // Assume shortest pulse is half period
			device->long_width = 0;                                                               // Not used
			device->reset_limit = (float)(to_us * (hist_gaps.bins[hist_gaps.bins_count - 1].max + 1));      // Set limit above biggest gap
		}
		else if (hist_pulses.bins_count >1 && hist_gaps.bins_count >1) {  //(hist_pulses.bins_count == 2 && hist_gaps.bins_count >= 3) {
			//fprintf(stderr, "Pulse Width Modulation with multiple packets\n");
			device->modulation = (package_type == PULSE_DATA_FSK) ? FSK_PULSE_PWM : OOK_PULSE_PWM;
			device->short_width = (float)(to_us * hist_pulses.bins[0].mean);
			device->long_width = (float)(to_us * hist_pulses.bins[1].mean);
			device->gap_limit = (float)(to_us * (hist_gaps.bins[1].max + 1)); // Set limit above second gap
			device->tolerance = (float)((device->long_width - device->short_width) * 0.4);
			device->reset_limit = (float)(to_us * (hist_gaps.bins[hist_gaps.bins_count - 1].max + 1)); // Set limit above biggest gap
		}
		else if (hist_pulses.bins_count > 1 && hist_gaps.bins_count > 2){  //((hist_pulses.bins_count >= 3 && hist_gaps.bins_count >= 3)
			//&& (abs(hist_pulses.bins[1].mean - 2 * hist_pulses.bins[0].mean) <= hist_pulses.bins[0].mean / 8)    // Pulses are multiples of shortest pulse
			//&& (abs(hist_pulses.bins[2].mean - 3 * hist_pulses.bins[0].mean) <= hist_pulses.bins[0].mean / 8)
			//&& (abs(hist_gaps.bins[0].mean - hist_pulses.bins[0].mean) <= hist_pulses.bins[0].mean / 8)    // Gaps are multiples of shortest pulse
			//&& (abs(hist_gaps.bins[1].mean - 2 * hist_pulses.bins[0].mean) <= hist_pulses.bins[0].mean / 8)
			//&& (abs(hist_gaps.bins[2].mean - 3 * hist_pulses.bins[0].mean) <= hist_pulses.bins[0].mean / 8)) {
			//fprintf(stderr, "Non Return to Zero coding (Pulse Code)\n");
			device->modulation = (package_type == PULSE_DATA_FSK) ? FSK_PULSE_PCM : OOK_PULSE_PCM;
			device->short_width = (float)(to_us * hist_pulses.bins[1].mean);        // Shortest pulse is bit width
			device->long_width = (float)(to_us * hist_gaps.bins[2].mean);        // Bit period equal to pulse length (NRZ)
			device->reset_limit = (float)(to_us * hist_pulses.bins[1].mean * 5); // No limit to run of zeros...
			device->gap_limit = (float)(to_us * (hist_gaps.bins[2].mean ));  //add marc 4* 0-->1 for tech
		}
		else if (hist_pulses.bins_count>2) {  //(hist_pulses.bins_count == 3) {
			//fprintf(stderr, "Pulse Width Modulation with sync/delimiter\n");
			// Re-sort to find lowest pulse count index (is probably delimiter)
			histogram_sort_count(&hist_pulses);
			int32_t p1 = hist_pulses.bins[1].mean;
			int32_t p2 = hist_pulses.bins[2].mean;
			device->modulation = (package_type == PULSE_DATA_FSK) ? FSK_PULSE_PWM : OOK_PULSE_PWM;
			device->short_width = (float)(to_us * (p1 < p2 ? p1 : p2));                                // Set to shorter pulse width
			device->long_width = (float)(to_us * (p1 < p2 ? p2 : p1));                                // Set to longer pulse width
			device->sync_width = (float)(to_us * hist_pulses.bins[0].mean);                           // Set to lowest count pulse width
			device->reset_limit = (float)(to_us * (hist_gaps.bins[hist_gaps.bins_count - 1].max + 1)); // Set limit above biggest gap
		}
		//else {
		//	continue;
		//    //fprintf(stderr, "No clue...\n");
	 //  }
		if (hist_pulses.bins_count == 1)
		{
			sprintf(str, "%d ", hist_pulses.bins[0].mean / 8);
			AddKeyValueDevice("hist_pulses.bins[0].mean/8:", str);
		}
		if (hist_pulses.bins_count == 2)
		{
			sprintf(str, "%d ", abs(hist_pulses.bins[1].mean - 2 * hist_pulses.bins[0].mean));
			AddKeyValueDevice("abs(hist_pulses.bins[1].mean - 2*hist_pulses.bins[0].mean):", str);
		}
		if (hist_pulses.bins_count == 3)
		{
			sprintf(str, "%d ", abs(hist_pulses.bins[2].mean - 3 * hist_pulses.bins[0].mean));
			AddKeyValueDevice("abs(hist_pulses.bins[2].mean - 3*hist_pulses.bins[0].mean):", str);
		}
		if (hist_pulses.bins_count == 1 && hist_gaps.bins_count == 1)
		{
			sprintf(str, "%d ", abs(hist_gaps.bins[0].mean - hist_pulses.bins[0].mean));
			AddKeyValueDevice("abs(hist_gaps.bins[0].mean   -   hist_pulses.bins[0].mean):", str);
		}
		if (hist_pulses.bins_count == 1 && hist_gaps.bins_count == 2)
		{
			sprintf(str, "%d ", abs(hist_gaps.bins[1].mean - 2 * hist_pulses.bins[0].mean));
			AddKeyValueDevice("abs(hist_gaps.bins[1].mean   - 2*hist_pulses.bins[0].mean):", str);
		}
		if (hist_pulses.bins_count == 1 && hist_gaps.bins_count == 3)
		{
			sprintf(str, "%d ", abs(hist_gaps.bins[2].mean - 3 * hist_pulses.bins[0].mean));
			AddKeyValueDevice("abs(hist_gaps.bins[2].mean   - 3*hist_pulses.bins[0].mean):", str);
		}


		sprintf(str, "%.0f ", device->short_width);
		AddKeyValueDevice("short_width:", str);
		sprintf(str, "%.0f ", device->long_width);
		AddKeyValueDevice("long_width:", str);
		sprintf(str, "%.0f ", device->sync_width);
		AddKeyValueDevice("sync_width:", str);
		sprintf(str, "%.0f ", device->reset_limit);
		AddKeyValueDevice("reset_limit:", str);
		sprintf(str, "%.0f ", device->gap_limit);
		AddKeyValueDevice("gap_limit:", str);


		// Output RfRaw line (if possible)
		if (hist_timings.bins_count <= 8) {
			// if there is no 3rd gap length output one long B1 code
			if (hist_gaps.bins_count <= 2) {
				hexstr_t hexstr = { .p = {0} };
				hexstr_push_byte(&hexstr, 0xaa);
				hexstr_push_byte(&hexstr, 0xb1);
				hexstr_push_byte(&hexstr, hist_timings.bins_count);
				for (uint32_t b = 0; b < hist_timings.bins_count; ++b) {
					double w = hist_timings.bins[b].mean * to_us;
					hexstr_push_word(&hexstr, (uint16_t)(w < USHRT_MAX ? w : USHRT_MAX));
				}
				for (uint32_t i = 0; i < data->num_pulses; ++i) {
					int32_t p = histogram_find_bin_index(&hist_timings, data->pulse[i]);
					int32_t g = histogram_find_bin_index(&hist_timings, data->gap[i]);
					if (p < 0 || g < 0) {
						//fprintf(stderr, "%s: this can't happen\n", __func__);
						exit(1);
					}
					hexstr_push_byte(&hexstr, 0x80 | (p << 4) | g);
				}
				hexstr_push_byte(&hexstr, 0x55);
				//fprintf(stderr, "view at https://triq.org/pdv/#");
				//hexstr_print(&hexstr, stderr);
				//fprintf(stderr, "\n");
			}
			// otherwise try to group as B0 codes
			else {
				// pick last gap length but a most the 4th
				int32_t limit_bin = MIN(3, hist_gaps.bins_count - 1);
				int32_t limit = hist_gaps.bins[limit_bin].min;
				hexstr_t hexstrs[HEXSTR_MAX_COUNT] = { {.p = {0}} };
				uint32_t hexstr_cnt = 0;
				uint32_t i = 0;
				while (i < data->num_pulses && hexstr_cnt < HEXSTR_MAX_COUNT) {
					hexstr_t *hexstr = &hexstrs[hexstr_cnt];
					hexstr_push_byte(hexstr, 0xaa);
					hexstr_push_byte(hexstr, 0xb0);
					hexstr_push_byte(hexstr, 0); // len
					hexstr_push_byte(hexstr, hist_timings.bins_count);
					hexstr_push_byte(hexstr, 1); // repeats
					for (uint32_t b = 0; b < hist_timings.bins_count; ++b) {
						double w = hist_timings.bins[b].mean * to_us;
						hexstr_push_word(hexstr, (uint16_t)(w < USHRT_MAX ? w : USHRT_MAX));
					}
					for (; i < data->num_pulses; ++i) {
						int32_t p = histogram_find_bin_index(&hist_timings, data->pulse[i]);
						int32_t g = histogram_find_bin_index(&hist_timings, data->gap[i]);
						if (p < 0 || g < 0) {
							//fprintf(stderr, "%s: this can't happen\n", __func__);
							exit(1);
						}
						hexstr_push_byte(hexstr, 0x80 | (p << 4) | g);
						if (data->gap[i] >= limit) {
							++i;
							break;
						}
					}
					hexstr_push_byte(hexstr, 0x55);
					hexstr->p[2] = hexstr->idx - 4 <= 255 ? hexstr->idx - 4 : 0; // len
					if (hexstr_cnt > 0 && hexstrs[hexstr_cnt - 1].idx == hexstr->idx
						&& !memcmp(&hexstrs[hexstr_cnt - 1].p[5], &hexstr->p[5], hexstr->idx - 5)) {
						hexstr->idx = 0; // clear
						hexstrs[hexstr_cnt - 1].p[4] ++; // repeats
					}
					else {
						hexstr_cnt++;
					}
				}

				//fprintf(stderr, "view at https://triq.org/pdv/#");
				//for (uint32_t j = 0; j < hexstr_cnt; ++j) {
	 /*               if (j > 0)
						fprintf(stderr, "+");*/
						/*             hexstr_print(&hexstrs[j], stderr);
								 }
								 fprintf(stderr, "\n");*/
								 /*           if (hexstr_cnt >= HEXSTR_MAX_COUNT) {
												fprintf(stderr, "Too many pulse groups (%u pulses missed in rfraw)\n", data->num_pulses - i);
											}*/
			}
		}
		if (device->modulation > OOK_PULSE_NRZS)  //--> FSK
		{
			sprintf(str, "%.0f hz", data->freq1_hz + (float)(data->fsk_f1_est / INT16_MAX * data->sample_rate / 2.0));//data->freq1_hz/1000000
			AddKeyValueDevice("freq1:", str);

			sprintf(str, "%.0f hz", data->freq2_hz + (float)(data->fsk_f2_est / INT16_MAX * data->sample_rate / 2.0));  //data->freq2_hz/1000000
			AddKeyValueDevice("freq2:", str);
		}
		else
		{
			sprintf(str, "%.0f hz", data->freq1_hz + (float)(data->fsk_f1_est / INT16_MAX * data->sample_rate / 2.0));//data->freq1_hz/1000000
			AddKeyValueDevice("freq:", str);

		}
		// Demodulate (if detected)
		if (device->modulation) {
			sprintf(str, "%d", device->modulation);
			AddKeyValueDevice("model", str);
			AddKeyValueDevice("channel", "9999");
			//fprintf(stderr, "Attempting demodulation... short_width: %.0f, long_width: %.0f, reset_limit: %.0f, sync_width: %.0f\n",
			//        device->short_width, device->long_width,
			//        device->reset_limit, device->sync_width);
			switch (device->modulation) {
			case FSK_PULSE_PCM:
				AddKeyValueDevice("Modulation:", "FSK_PULSE_PCM");
				/*           fprintf(stderr, "Use a flex decoder with -X 'n=name,m=FSK_PCM,s=%.0f,l=%.0f,r=%.0f'\n",
								   device->short_width, device->long_width, device->reset_limit);*/
				pulse_slicer_pcm(data, device, startPulses, device->modulation);
				break;
			case OOK_PULSE_PPM:
				AddKeyValueDevice("Modulation:", "OOK_PULSE_PPM");
				//fprintf(stderr, "Use a flex decoder with -X 'n=name,m=OOK_PPM,s=%.0f,l=%.0f,g=%.0f,r=%.0f'\n",
				//        device->short_width, device->long_width,
				//        device->gap_limit, device->reset_limit);
				data->gap[data->num_pulses - 1] = (int32_t)(device->reset_limit / to_us + 1); // Be sure to terminate package
				pulse_slicer_ppm(data, device, startPulses, device->modulation);
				break;
			case OOK_PULSE_PWM:
				AddKeyValueDevice("Modulation:", "OOK_PULSE_PWM");
				//fprintf(stderr, "Use a flex decoder with -X 'n=name,m=OOK_PWM,s=%.0f,l=%.0f,r=%.0f,g=%.0f,t=%.0f,y=%.0f'\n",
				//        device->short_width, device->long_width, device->reset_limit,
				//        device->gap_limit, device->tolerance, device->sync_width);
				data->gap[data->num_pulses - 1] = (int32_t)(device->reset_limit / to_us + 1); // Be sure to terminate package
				pulse_slicer_pwm(data, device, startPulses, device->modulation);
				break;
			case FSK_PULSE_PWM:
				AddKeyValueDevice("Modulation:", "FSK_PULSE_PWM");
				//fprintf(stderr, "Use a flex decoder with -X 'n=name,m=FSK_PWM,s=%.0f,l=%.0f,r=%.0f,g=%.0f,t=%.0f,y=%.0f'\n",
				//        device->short_width, device->long_width, device->reset_limit,
				//        device->gap_limit, device->tolerance, device->sync_width);
				data->gap[data->num_pulses - 1] = (int32_t)(device->reset_limit / to_us + 1); // Be sure to terminate package
				pulse_slicer_pwm(data, device, startPulses, device->modulation);
				break;
			case OOK_PULSE_MANCHESTER_ZEROBIT:
				AddKeyValueDevice("Modulation:", "OOK_PULSE_MANCHESTER_ZEROBIT");
				/*           fprintf(stderr, "Use a flex decoder with -X 'n=name,m=OOK_MC_ZEROBIT,s=%.0f,l=%.0f,r=%.0f'\n",
								   device->short_width, device->long_width, device->reset_limit);*/
				data->gap[data->num_pulses - 1] = (int32_t)(device->reset_limit / to_us + 1); // Be sure to terminate package
				pulse_slicer_manchester_zerobit(data, device, startPulses, device->modulation);
				break;
			case OOK_PULSE_PCM:
 				AddKeyValueDevice("Modulation:", "OOK_PULSE_PCM");
				/*           fprintf(stderr, "Use a flex decoder with -X 'n=name,m=OOK_MC_ZEROBIT,s=%.0f,l=%.0f,r=%.0f'\n",
								   device->short_width, device->long_width, device->reset_limit);*/
		 		data->gap[data->num_pulses - 1] = (int32_t)(device->reset_limit / to_us + 1); // Be sure to terminate package
				pulse_slicer_pcm(data, device, startPulses, device->modulation);
				break;
			default:
				AddKeyValueDevice("Modulation:", "Unsupported");
				//fprintf(stderr, "Unsupported\n");
			}
		}
	 //}
//***********************************************************************************************************
 ////   // Attempt to find a matching modulation
 ////   if (data->num_pulses == 1) {
 ////       fprintf(stderr, "Single pulse detected. Probably Frequency Shift Keying or just noise...\n");
 ////   }
 ////   else if (hist_pulses.bins_count == 1 && hist_gaps.bins_count == 1) {
 ////       fprintf(stderr, "Un-modulated signal. Maybe a preamble...\n");
 ////   }
 ////   else if (hist_pulses.bins_count == 1 && hist_gaps.bins_count > 2) {  //1->2
 ////       //fprintf(stderr, "Pulse Position Modulation with fixed pulse width\n");
 ////       device->modulation  = OOK_PULSE_PPM; // TODO: there is not FSK_PULSE_PPM
 ////       device->short_width = (float) (to_us * hist_gaps.bins[2].mean/2.0);  //0-->1
 ////       device->long_width  = (float)(to_us * hist_gaps.bins[2].mean);  //1-->2
 ////       device->gap_limit   = (float)(to_us * (hist_gaps.bins[2].max+1));   // marc +10 1-->2                       // Set limit above next lower gap
 ////       device->reset_limit = (float)(to_us * (hist_gaps.bins[2].max+1)); // marc +10 [hist_gaps.bins_count - 1].max + 1)->2 Set limit above biggest gap
 ////   }
 ////   else if (hist_pulses.bins_count == 2 && hist_gaps.bins_count == 1) {
 ////       //fprintf(stderr, "Pulse Width Modulation with fixed gap\n");
 ////       device->modulation  = (package_type == PULSE_DATA_FSK) ? FSK_PULSE_PWM : OOK_PULSE_PWM;
 ////       device->short_width = (float)(to_us * hist_pulses.bins[0].mean);
 ////       device->long_width  = (float)(to_us * hist_pulses.bins[1].mean);
 ////       device->tolerance   = (float)((device->long_width - device->short_width) * 0.4);
 ////       device->reset_limit = (float)(to_us * (hist_gaps.bins[hist_gaps.bins_count - 1].max + 1)); // Set limit above biggest gap
 ////   }
 ////   else if (hist_pulses.bins_count == 2 && hist_gaps.bins_count == 2 && hist_periods.bins_count == 1) {
 ////       //fprintf(stderr, "Pulse Width Modulation with fixed period\n");
 ////       device->modulation  = (package_type == PULSE_DATA_FSK) ? FSK_PULSE_PWM : OOK_PULSE_PWM;
 ////       device->short_width = (float)(to_us * hist_pulses.bins[0].mean);
 ////       device->long_width  = (float)(to_us * hist_pulses.bins[1].mean);
 ////       device->tolerance   = (float)((device->long_width - device->short_width) * 0.4);
 ////       device->reset_limit = (float)(to_us * (hist_gaps.bins[hist_gaps.bins_count - 1].max + 1)); // Set limit above biggest gap
 ////   }
 ////   else if (hist_pulses.bins_count == 2 && hist_gaps.bins_count == 2 && hist_periods.bins_count == 3) {
 ////       //fprintf(stderr, "Manchester coding\n");
 ////       device->modulation  = (package_type == PULSE_DATA_FSK) ? FSK_PULSE_MANCHESTER_ZEROBIT : OOK_PULSE_MANCHESTER_ZEROBIT;
 ////       device->short_width = (float)(to_us * MIN(hist_pulses.bins[0].mean, hist_pulses.bins[1].mean)); // Assume shortest pulse is half period
 ////       device->long_width  = 0;                                                               // Not used
 ////       device->reset_limit = (float)(to_us * (hist_gaps.bins[hist_gaps.bins_count - 1].max + 1));      // Set limit above biggest gap
 ////   }
 ////   else if (hist_pulses.bins_count == 2 && hist_gaps.bins_count >= 3) {
 ////       //fprintf(stderr, "Pulse Width Modulation with multiple packets\n");
 ////       device->modulation  = (package_type == PULSE_DATA_FSK) ? FSK_PULSE_PWM : OOK_PULSE_PWM;
 ////       device->short_width = (float)(to_us * hist_pulses.bins[0].mean);
 ////       device->long_width  = (float)(to_us * hist_pulses.bins[1].mean);
 ////       device->gap_limit   = (float)(to_us * (hist_gaps.bins[1].max + 1)); // Set limit above second gap
 ////       device->tolerance   = (float)((device->long_width - device->short_width) * 0.4);
 ////       device->reset_limit = (float)(to_us * (hist_gaps.bins[hist_gaps.bins_count - 1].max + 1)); // Set limit above biggest gap
 ////   }
 ////   else if ((hist_pulses.bins_count >= 3 && hist_gaps.bins_count >= 3)
 ////           && (abs(hist_pulses.bins[1].mean - 2*hist_pulses.bins[0].mean) <= hist_pulses.bins[0].mean/8)    // Pulses are multiples of shortest pulse
 ////           && (abs(hist_pulses.bins[2].mean - 3*hist_pulses.bins[0].mean) <= hist_pulses.bins[0].mean/8)
 ////           && (abs(hist_gaps.bins[0].mean   -   hist_pulses.bins[0].mean) <= hist_pulses.bins[0].mean/8)    // Gaps are multiples of shortest pulse
 ////           && (abs(hist_gaps.bins[1].mean   - 2*hist_pulses.bins[0].mean) <= hist_pulses.bins[0].mean/8)
 ////           && (abs(hist_gaps.bins[2].mean   - 3*hist_pulses.bins[0].mean) <= hist_pulses.bins[0].mean/8)) {
 ////       //fprintf(stderr, "Non Return to Zero coding (Pulse Code)\n");
 ////       device->modulation  = (package_type == PULSE_DATA_FSK) ? FSK_PULSE_PCM : OOK_PULSE_PCM;
 ////       device->short_width = (float)(to_us * hist_pulses.bins[0].mean);        // Shortest pulse is bit width
 ////       device->long_width  = (float)(to_us * hist_pulses.bins[0].mean);        // Bit period equal to pulse length (NRZ)
 ////       device->reset_limit = (float)(to_us * hist_pulses.bins[0].mean * 1024); // No limit to run of zeros...
	////	device->gap_limit = (float)(to_us * (hist_gaps.bins[0].mean * 25));  //add marc
 ////   }
 ////   else if (hist_pulses.bins_count == 3) {
 ////       //fprintf(stderr, "Pulse Width Modulation with sync/delimiter\n");
 ////       // Re-sort to find lowest pulse count index (is probably delimiter)
 ////       histogram_sort_count(&hist_pulses);
 ////       int32_t p1 = hist_pulses.bins[1].mean;
 ////       int32_t p2 = hist_pulses.bins[2].mean;
 ////       device->modulation  = (package_type == PULSE_DATA_FSK) ? FSK_PULSE_PWM : OOK_PULSE_PWM;
 ////       device->short_width = (float)(to_us * (p1 < p2 ? p1 : p2));                                // Set to shorter pulse width
 ////       device->long_width  = (float)(to_us * (p1 < p2 ? p2 : p1));                                // Set to longer pulse width
 ////       device->sync_width  = (float)(to_us * hist_pulses.bins[0].mean);                           // Set to lowest count pulse width
 ////       device->reset_limit = (float)(to_us * (hist_gaps.bins[hist_gaps.bins_count - 1].max + 1)); // Set limit above biggest gap
 ////   }
 ////   //else {
 ////   //    fprintf(stderr, "No clue...\n");
 ////  //}
	////if (hist_pulses.bins_count == 1 )
	////{
	////	sprintf(str, "%d ", hist_pulses.bins[0].mean / 8);
	////	AddKeyValueDevice("hist_pulses.bins[0].mean/8:", str);
	////}
	////if (hist_pulses.bins_count ==2 )
	////{
	////	sprintf(str, "%d ", abs(hist_pulses.bins[1].mean - 2 * hist_pulses.bins[0].mean));
	////	AddKeyValueDevice("abs(hist_pulses.bins[1].mean - 2*hist_pulses.bins[0].mean):", str);
	////}
	////if (hist_pulses.bins_count ==3 )
	////{
	////	sprintf(str, "%d ", abs(hist_pulses.bins[2].mean - 3 * hist_pulses.bins[0].mean));
	////	AddKeyValueDevice("abs(hist_pulses.bins[2].mean - 3*hist_pulses.bins[0].mean):", str);
	////}
	////if (hist_pulses.bins_count ==1 && hist_gaps.bins_count == 1)
	////{
	////	sprintf(str, "%d ", abs(hist_gaps.bins[0].mean - hist_pulses.bins[0].mean));
	////	AddKeyValueDevice("abs(hist_gaps.bins[0].mean   -   hist_pulses.bins[0].mean):", str);
	////}
	////if (hist_pulses.bins_count ==1 && hist_gaps.bins_count == 2)
	////{
	////	sprintf(str, "%d ", abs(hist_gaps.bins[1].mean - 2 * hist_pulses.bins[0].mean));
	////	AddKeyValueDevice("abs(hist_gaps.bins[1].mean   - 2*hist_pulses.bins[0].mean):", str);
	////}
	////if (hist_pulses.bins_count ==1 && hist_gaps.bins_count == 3)
	////{
	////	sprintf(str, "%d ", abs(hist_gaps.bins[2].mean - 3 * hist_pulses.bins[0].mean));
	////	AddKeyValueDevice("abs(hist_gaps.bins[2].mean   - 3*hist_pulses.bins[0].mean):", str);
	////}


	////		sprintf(str, "%.0f ", device->short_width);
	////		AddKeyValueDevice("short_width:", str);
	////		sprintf(str, "%.0f ", device->long_width);
	////		AddKeyValueDevice("long_width:", str);
	////		sprintf(str, "%.0f ", device->sync_width);
	////		AddKeyValueDevice("sync_width:", str);
	////		sprintf(str, "%.0f ", device->reset_limit);
	////		AddKeyValueDevice("reset_limit:", str);
	////		sprintf(str, "%.0f ", device->gap_limit);
	////		AddKeyValueDevice("gap_limit:", str);

	////
 ////   // Output RfRaw line (if possible)
 ////   if (hist_timings.bins_count <= 8) {
 ////       // if there is no 3rd gap length output one long B1 code
 ////       if (hist_gaps.bins_count <= 2) {
 ////           hexstr_t hexstr = {.p = {0}};
 ////           hexstr_push_byte(&hexstr, 0xaa);
 ////           hexstr_push_byte(&hexstr, 0xb1);
 ////           hexstr_push_byte(&hexstr, hist_timings.bins_count);
 ////           for (uint32_t b = 0; b < hist_timings.bins_count; ++b) {
 ////               double w = hist_timings.bins[b].mean * to_us;
 ////               hexstr_push_word(&hexstr,(uint16_t) (w < USHRT_MAX ? w : USHRT_MAX));
 ////           }
 ////           for (uint32_t i = 0; i < data->num_pulses; ++i) {
 ////               int32_t p = histogram_find_bin_index(&hist_timings, data->pulse[i]);
 ////               int32_t g = histogram_find_bin_index(&hist_timings, data->gap[i]);
 ////               if (p < 0 || g < 0) {
 ////                   //fprintf(stderr, "%s: this can't happen\n", __func__);
 ////                   exit(1);
 ////               }
 ////               hexstr_push_byte(&hexstr, 0x80 | (p << 4) | g);
 ////           }
 ////           hexstr_push_byte(&hexstr, 0x55);
 ////           //fprintf(stderr, "view at https://triq.org/pdv/#");
 ////           //hexstr_print(&hexstr, stderr);
 ////           //fprintf(stderr, "\n");
 ////       }
 ////       // otherwise try to group as B0 codes
 ////       else {
 ////           // pick last gap length but a most the 4th
 ////           int32_t limit_bin = MIN(3, hist_gaps.bins_count - 1);
 ////           int32_t limit = hist_gaps.bins[limit_bin].min;
 ////           hexstr_t hexstrs[HEXSTR_MAX_COUNT] = {{.p = {0}}};
 ////           uint32_t hexstr_cnt = 0;
 ////           uint32_t i = 0;
 ////           while (i < data->num_pulses && hexstr_cnt < HEXSTR_MAX_COUNT) {
 ////               hexstr_t *hexstr = &hexstrs[hexstr_cnt];
 ////               hexstr_push_byte(hexstr, 0xaa);
 ////               hexstr_push_byte(hexstr, 0xb0);
 ////               hexstr_push_byte(hexstr, 0); // len
 ////               hexstr_push_byte(hexstr, hist_timings.bins_count);
 ////               hexstr_push_byte(hexstr, 1); // repeats
 ////               for (uint32_t b = 0; b < hist_timings.bins_count; ++b) {
 ////                   double w =hist_timings.bins[b].mean * to_us;
 ////                   hexstr_push_word(hexstr, (uint16_t)(w < USHRT_MAX ? w : USHRT_MAX));
 ////               }
 ////               for (; i < data->num_pulses; ++i) {
 ////                   int32_t p = histogram_find_bin_index(&hist_timings, data->pulse[i]);
 ////                   int32_t g = histogram_find_bin_index(&hist_timings, data->gap[i]);
 ////                   if (p < 0 || g < 0) {
 ////                       //fprintf(stderr, "%s: this can't happen\n", __func__);
 ////                       exit(1);
 ////                   }
 ////                   hexstr_push_byte(hexstr, 0x80 | (p << 4) | g);
 ////                   if (data->gap[i] >= limit) {
 ////                       ++i;
 ////                       break;
 ////                   }
 ////               }
 ////               hexstr_push_byte(hexstr, 0x55);
 ////               hexstr->p[2] = hexstr->idx - 4 <= 255 ? hexstr->idx - 4 : 0; // len
 ////               if (hexstr_cnt > 0 && hexstrs[hexstr_cnt - 1].idx == hexstr->idx
 ////                       && !memcmp(&hexstrs[hexstr_cnt - 1].p[5], &hexstr->p[5], hexstr->idx - 5)) {
 ////                   hexstr->idx = 0; // clear
 ////                   hexstrs[hexstr_cnt - 1].p[4] ++; // repeats
 ////               } else {
 ////                   hexstr_cnt++;
 ////               }
 ////           }

 ////           //fprintf(stderr, "view at https://triq.org/pdv/#");
 ////           //for (uint32_t j = 0; j < hexstr_cnt; ++j) {
 /////*               if (j > 0)
 ////                   fprintf(stderr, "+");*/
 ////  /*             hexstr_print(&hexstrs[j], stderr);
 ////           }
 ////           fprintf(stderr, "\n");*/
 /////*           if (hexstr_cnt >= HEXSTR_MAX_COUNT) {
 ////               fprintf(stderr, "Too many pulse groups (%u pulses missed in rfraw)\n", data->num_pulses - i);
 ////           }*/
 ////       }
 ////   }
	////if (device->modulation > OOK_PULSE_NRZS)  //--> FSK
	////{
	////	sprintf(str, "%.0f hz", data->freq1_hz + (float)(data->fsk_f1_est / INT16_MAX * data->sample_rate / 2.0));//data->freq1_hz/1000000
	////	AddKeyValueDevice("freq1:", str);

	////	sprintf(str, "%.0f hz", data->freq2_hz + (float)(data->fsk_f2_est / INT16_MAX * data->sample_rate / 2.0));  //data->freq2_hz/1000000
	////	AddKeyValueDevice("freq2:", str);
	////}
	////else
	////{
	////	sprintf(str, "%.0f hz", data->freq1_hz + (float)(data->fsk_f1_est / INT16_MAX * data->sample_rate / 2.0));//data->freq1_hz/1000000
	////	AddKeyValueDevice("freq:", str);

	////}
 ////   // Demodulate (if detected)
 ////   if (device->modulation) {
	////	sprintf(str, "%d", device->modulation);
	////	AddKeyValueDevice("model", str);
	////	AddKeyValueDevice("channel", "9999");
 ////       //fprintf(stderr, "Attempting demodulation... short_width: %.0f, long_width: %.0f, reset_limit: %.0f, sync_width: %.0f\n",
 ////       //        device->short_width, device->long_width,
 ////       //        device->reset_limit, device->sync_width);
 ////       switch (device->modulation) {
 ////       case FSK_PULSE_PCM:
	////		AddKeyValueDevice("Modulation:", "FSK_PULSE_PCM");
 /////*           fprintf(stderr, "Use a flex decoder with -X 'n=name,m=FSK_PCM,s=%.0f,l=%.0f,r=%.0f'\n",
 ////                   device->short_width, device->long_width, device->reset_limit);*/
 ////           pulse_slicer_pcm(data, device, startPulses, device->modulation);
 ////           break;
 ////       case OOK_PULSE_PPM:
	////		AddKeyValueDevice("Modulation:", "OOK_PULSE_PPM");
 ////           //fprintf(stderr, "Use a flex decoder with -X 'n=name,m=OOK_PPM,s=%.0f,l=%.0f,g=%.0f,r=%.0f'\n",
 ////           //        device->short_width, device->long_width,
 ////           //        device->gap_limit, device->reset_limit);
 ////           data->gap[data->num_pulses - 1] = (int32_t) (device->reset_limit / to_us + 1); // Be sure to terminate package
 ////           pulse_slicer_ppm(data, device, startPulses, device->modulation);
 ////           break;
 ////       case OOK_PULSE_PWM:
	////		AddKeyValueDevice("Modulation:", "OOK_PULSE_PWM");
 ////           //fprintf(stderr, "Use a flex decoder with -X 'n=name,m=OOK_PWM,s=%.0f,l=%.0f,r=%.0f,g=%.0f,t=%.0f,y=%.0f'\n",
 ////           //        device->short_width, device->long_width, device->reset_limit,
 ////           //        device->gap_limit, device->tolerance, device->sync_width);
 ////           data->gap[data->num_pulses - 1] = (int32_t)(device->reset_limit / to_us + 1); // Be sure to terminate package
 ////           pulse_slicer_pwm(data, device, startPulses, device->modulation);
 ////           break;
 ////       case FSK_PULSE_PWM:
	////		AddKeyValueDevice("Modulation:", "FSK_PULSE_PWM");
 ////           //fprintf(stderr, "Use a flex decoder with -X 'n=name,m=FSK_PWM,s=%.0f,l=%.0f,r=%.0f,g=%.0f,t=%.0f,y=%.0f'\n",
 ////           //        device->short_width, device->long_width, device->reset_limit,
 ////           //        device->gap_limit, device->tolerance, device->sync_width);
 ////           data->gap[data->num_pulses - 1] = (int32_t)(device->reset_limit / to_us + 1); // Be sure to terminate package
 ////           pulse_slicer_pwm(data, device, startPulses, device->modulation);
 ////           break;
 ////       case OOK_PULSE_MANCHESTER_ZEROBIT:
	////		AddKeyValueDevice("Modulation:", "OOK_PULSE_MANCHESTER_ZEROBIT");
 /////*           fprintf(stderr, "Use a flex decoder with -X 'n=name,m=OOK_MC_ZEROBIT,s=%.0f,l=%.0f,r=%.0f'\n",
 ////                   device->short_width, device->long_width, device->reset_limit);*/
 ////           data->gap[data->num_pulses - 1] = (int32_t)(device->reset_limit / to_us + 1); // Be sure to terminate package
 ////           pulse_slicer_manchester_zerobit(data, device, startPulses, device->modulation);
 ////           break;
 ////       default:
	////		AddKeyValueDevice("Modulation:", "Unsupported");
 ////           //fprintf(stderr, "Unsupported\n");
 ////       }
 ////   }
//*****************************************************************************
    //fprintf(stderr, "\n");
#endif
}
