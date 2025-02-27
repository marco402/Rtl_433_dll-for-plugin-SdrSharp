/** @file
    Various utility functions for use by applications

    Copyright (C) 2015 Tommy Vestermark

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include "r_util.h"
#include "fatal.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void get_time_now(struct timeval *tv)
{
    int32_t ret = gettimeofday(tv, NULL);
    if (ret)
        perror("gettimeofday");
}

uint8_t *format_time_str(uint8_t *buf, uint8_t const *format, int32_t with_tz, time_t time_secs)
{
    time_t etime;
    struct tm tm_info;

    if (time_secs == 0) {
        time(&etime);
    }
    else {
        etime = time_secs;
    }

#ifdef _WIN32 /* MinGW might have localtime_r but apparently not MinGW64 */
    localtime_s(&tm_info, &etime); // win32 doesn't have localtime_r()
#else
    localtime_r(&etime, &tm_info); // thread-safe
#endif

    if (!format || !*format)
        format = "%Y-%m-%d %H:%M:%S";

    size_t l = strftime(buf, LOCAL_TIME_BUFLEN, format, &tm_info);
    if (with_tz) {
        strftime(buf + l, LOCAL_TIME_BUFLEN - l, "%z", &tm_info);
        if (!strcmp(buf + l, "+0000"))
            strcpy(buf + l, "Z"); // NOLINT
    }
    return buf;
}

uint8_t *usecs_time_str(uint8_t *buf, uint8_t const *format, int32_t with_tz, struct timeval *tv)
{
    struct timeval now;
    struct tm tm_info;

    if (!tv) {
        tv = &now;
        get_time_now(tv);
    }

    time_t t_secs = tv->tv_sec;
#ifdef _WIN32 /* MinGW might have localtime_r but apparently not MinGW64 */
    localtime_s(&tm_info, &t_secs); // win32 doesn't have localtime_r()
#else
    localtime_r(&t_secs, &tm_info); // thread-safe
#endif

    if (!format || !*format)
        format = "%Y-%m-%d %H:%M:%S";

    size_t l = strftime(buf, LOCAL_TIME_BUFLEN, format, &tm_info);
    l += snprintf(buf + l, LOCAL_TIME_BUFLEN - l, ".%06ld", (long)tv->tv_usec);
    if (with_tz) {
        strftime(buf + l, LOCAL_TIME_BUFLEN - l, "%z", &tm_info);
        if (!strcmp(buf + l, "+0000"))
            strcpy(buf + l, "Z"); // NOLINT
    }
    return buf;
}

uint8_t *sample_pos_str(float sample_file_pos, uint8_t *buf)
{
    snprintf(buf, LOCAL_TIME_BUFLEN, "@%fs", sample_file_pos);
    return buf;
}

float celsius2fahrenheit(float celsius)
{
  return celsius * (9.0f / 5.0f) + 32;
}


float fahrenheit2celsius(float fahrenheit)
{
    return (fahrenheit - 32) * (5.0f / 9.0f);
}


float kmph2mph(float kmph)
{
    return kmph * (1.0f / 1.609344f);
}

float mph2kmph(float mph)
{
    return mph * 1.609344f;
}


float mm2inch(float mm)
{
    return mm * 0.039370f;
}

float inch2mm(float inch)
{
    return inch * 25.4f;
}


float kpa2psi(float kpa)
{
    return kpa * (1.0f / 6.89475729f);
}

float psi2kpa(float psi)
{
    return psi * 6.89475729f;
}


float hpa2inhg(float hpa)
{
    return hpa * (1.0f / 33.8639f);
}

float inhg2hpa(float inhg)
{
    return inhg * 33.8639f;
}


bool str_endswith(uint8_t const *restrict str, uint8_t const *restrict suffix)
{
    if (!suffix) {
        return true;
    }
    if (!str) {
        return false;
    }
    int32_t str_len = (int32_t)strlen(str);
    int32_t suffix_len = (int32_t)strlen(suffix);

    return (str_len >= suffix_len) &&
           (0 == strcmp(str + (str_len - suffix_len), suffix));
}

// Original string replacement function was found here:
// https://stackoverflow.com/questions/779875/what-is-the-function-to-replace-string-in-c/779960#779960
//
// You must free the result if result is non-NULL.
uint8_t *str_replace(uint8_t const *orig, uint8_t const *rep, uint8_t const *with)
{
    uint8_t *result;  // the return string
    uint8_t const *ins; // the next insert point
    uint8_t *tmp;     // varies
    int32_t len_rep;   // length of rep (the string to remove)
    int32_t len_with;  // length of with (the string to replace rep with)
    int32_t len_front; // distance between rep and end of last rep
    int32_t count;     // number of replacements

    // sanity checks and initialization
    if (!orig || !rep)
        return NULL;
    len_rep = (int32_t)strlen(rep);
    if (len_rep == 0)
        return NULL; // empty rep causes infinite loop during count
    if (!with)
        with = "";
    len_with = (int32_t)strlen(with);

    // count the number of replacements needed
    ins = orig;
    for (count = 0; (tmp = strstr(ins, rep)); ++count) {
        ins = tmp + len_rep;
    }

    tmp = result = malloc(strlen(orig) + (len_with - len_rep) * (size_t)count + 1);
    if (!result) {
        WARN_MALLOC("str_replace()");
        return NULL; // NOTE: returns NULL on alloc failure.
    }

    // first time through the loop, all the variables are set correctly
    // from here on,
    //    tmp points to the end of the result string
    //    ins points to the next occurrence of rep in orig
    //    orig points to the remainder of orig after "end of rep"
    while (count--) {
        ins = strstr(orig, rep);
        len_front = (int32_t)(ins - orig);
        tmp = strncpy(tmp, orig, len_front) + len_front;
        tmp = strcpy(tmp, with) + len_with; // NOLINT
        orig += len_front + len_rep; // move to next "end of rep"
    }
    strcpy(tmp, orig); // NOLINT
    return result;
}

// Make a more readable string for a frequency.
uint8_t const *nice_freq (double freq)
{
  static uint8_t buf[30];

  if (freq >= 1E9)
     snprintf (buf, sizeof(buf), "%.3fGHz", freq/1E9);
  else if (freq >= 1E6)
     snprintf (buf, sizeof(buf), "%.3fMHz", freq/1E6);
  else if (freq >= 1E3)
     snprintf (buf, sizeof(buf), "%.3fkHz", freq/1E3);
  else
     snprintf (buf, sizeof(buf), "%f", freq);
  return (buf);
}
