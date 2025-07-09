/** @file
    High-level utility functions for decoders.

    Copyright (C) 2018 Christian Zuckschwerdt

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#ifndef INCLUDE_DECODER_UTIL_H_
#define INCLUDE_DECODER_UTIL_H_

#include <stdarg.h>
#include "bitbuffer.h"
#include "data.h"
#include "r_device.h"

#if defined _MSC_VER || defined ESP32 // Microsoft Visual Studio or ESP32
    // MSC and ESP32 have something like C99 restrict as __restrict
    #ifndef restrict
    #define restrict  __restrict
    #endif
#endif
// Defined in newer <sal.h> for MSVC.
#ifndef _Printf_format_string_
#define _Printf_format_string_
#endif
uint16_t nbLine;
#if LISTEDEVICES
	void listDevices(struct r_cfg *cfg);
#endif
#if ANALYZER
	void testUnKnown(int32_t length, int32_t startPulses, int32_t package_type, r_device *decoder);
#endif
/// Create a new r_device, copy from dev_template if not NULL.
///
/// A user data memory of `user_data_size` bytes will be allocated if not `0`.
r_device *decoder_create(r_device const *dev_template, uint32_t user_data_size);

/// Get the user data pointer, otherwise NULL.
///
/// The memory can be freely used by a decoder and is of the size given to `decoder_create()`.
void *decoder_user_data(r_device *decoder);

void initDeviceToPlugin();

void razKeyValueDeviceToPlugin();

void AddKeyValueDevice(uint8_t * key, uint8_t *  value);
/// Output data.
void decoder_output_data(r_device *decoder, data_t *data, bitbuffer_t *bitbuffer, int32_t row, uint32_t nbRepeat, int32_t startPulses, uint16_t package_type);

/// Output log.
void decoder_output_log(r_device *decoder, int32_t level, data_t *data);

// be terse, a maximum msg length of 60 characters is supported on the decoder_log_ functions
// e.g. "FoobarCorp-XY3000: unexpected type code %02x"

/// Get the current verbosity level for the decoder.
///
/// @deprecated Should not be used, consider using only `decoder_log_` functions.
int32_t decoder_verbose(r_device *decoder);

/// Output a log message.
void decoder_log(r_device *decoder, int32_t level, uint8_t const *func, uint8_t const *msg);

/// Output a formatted log message.
void decoder_logf(r_device *decoder, int32_t level, uint8_t const *func, _Printf_format_string_ const uint8_t *format, ...)
#if defined(__GNUC__) || defined(__clang__)
        __attribute__((format(printf, 4, 5)))
#endif
        ;

/// Output a log message with the content of the bitbuffer.
void decoder_log_bitbuffer(r_device *decoder, int32_t level, uint8_t const *func, bitbuffer_t *bitbuffer, uint8_t const *msg, int32_t startPulses, uint16_t package_type);

/// Output a formatted log message with the content of the bitbuffer.
void decoder_logf_bitbuffer(r_device *decoder, int32_t level, uint8_t const *func, const bitbuffer_t *bitbuffer, _Printf_format_string_ const uint8_t *format, ...)
#if defined(__GNUC__) || defined(__clang__)
        __attribute__((format(printf, 5, 6)))
#endif
        ;

/// Output a log message with the content of a bit row (byte buffer).
void decoder_log_bitrow(r_device *decoder, int32_t level, uint8_t const *func, uint8_t const *bitrow, uint32_t bit_len, uint8_t const *msg);

/// Output a formatted log message with the content of a bit row (byte buffer).
void decoder_logf_bitrow(r_device *decoder, int32_t level, uint8_t const *func, uint8_t const *bitrow, uint32_t bit_len, _Printf_format_string_ const uint8_t *format, ...)
#if defined(__GNUC__) || defined(__clang__)
        __attribute__((format(printf, 6, 7)))
#endif
        ;

#endif /* INCLUDE_DECODER_UTIL_H_ */
