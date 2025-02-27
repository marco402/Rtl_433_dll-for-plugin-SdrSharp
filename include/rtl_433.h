/** @file
    Definition of r_cfg application structure.
*/

#ifndef INCLUDE_RTL_433_H_
#define INCLUDE_RTL_433_H_

#include <stdint.h>
#include "list.h"
#include <time.h>
#include <signal.h>
#include "dll_rtl_433.h"
#define DEFAULT_SAMPLE_RATE 250000
#define DEFAULT_FREQUENCY 433920000
#ifdef DLL_RTL_433
#define DEFAULT_DISABLED 0
#endif
#define DEFAULT_HOP_TIME (60 * 10)
#define DEFAULT_ASYNC_BUF_NUMBER 0         // Force use of default value (librtlsdr default: 15)
#define DEFAULT_BUF_LENGTH (16 * 32 * 512) // librtlsdr default
#define FSK_PULSE_DETECTOR_LIMIT 800000000

#define MINIMAL_BUF_LENGTH 512
#define MAXIMAL_BUF_LENGTH (256 * 16384)
#define SIGNAL_GRABBER_BUFFER (12 * DEFAULT_BUF_LENGTH)
#define MAX_FREQS 32

#define INPUT_LINE_MAX 8192 /**< enough for a complete textual bitbuffer (25*256) */

struct sdr_dev;
struct r_device;
struct mg_mgr;

typedef enum {
    CONVERT_NATIVE,
    CONVERT_SI,
    CONVERT_CUSTOMARY,
} conversion_mode_t;

typedef enum {
    REPORT_TIME_DEFAULT,
    REPORT_TIME_DATE,
    REPORT_TIME_SAMPLES,
    REPORT_TIME_UNIX,
    REPORT_TIME_ISO,
    REPORT_TIME_OFF,
} time_mode_t;

typedef enum {
    DEVICE_MODE_QUIT,
    DEVICE_MODE_RESTART,
    DEVICE_MODE_PAUSE,
    DEVICE_MODE_MANUAL,
} device_mode_t;

typedef enum {
    DEVICE_STATE_STOPPED,
    DEVICE_STATE_STARTING,
    DEVICE_STATE_GRACE,
    DEVICE_STATE_STARTED,
} device_state_t;

typedef struct r_cfg {
	struct dm_state *demod;
#ifndef DLL_RTL_433
    device_mode_t dev_mode;   ///< Input device run mode
    device_state_t dev_state; ///< Input device run state
#endif
    uint8_t *dev_query;
    uint8_t const *dev_info;
    uint8_t *gain_str;
    uint8_t *settings_str;
    int32_t ppm_error;
    uint32_t out_block_size;
    uint8_t const *test_data;
    list_t in_files;
    uint8_t const *in_filename;
    int32_t in_replay;
    volatile sig_atomic_t hop_now;
    volatile sig_atomic_t exit_async;
    volatile sig_atomic_t exit_code; ///< 0=no err, 1=params or cmd line err, 2=sdr device read error, 3=usb init error, 5=USB error (reset), other=other error
    int32_t frequencies;
    int32_t frequency_index;
    uint32_t frequency[MAX_FREQS];
    uint32_t center_frequency;
    int32_t fsk_pulse_detect_mode;
    int32_t hop_times;
    int32_t hop_time[MAX_FREQS];
    time_t hop_start_time;
    int32_t duration;
    time_t stop_time;
    int32_t after_successful_events_flag;
    uint32_t samp_rate;
    uint64_t input_pos;
    uint32_t bytes_to_read;
    struct sdr_dev *dev;
    int32_t grab_mode; ///< Signal grabber mode: 0=off, 1=all, 2=unknown, 3=known
    int32_t raw_mode;  ///< Raw pulses printing mode: 0=off, 1=all, 2=unknown, 3=known
    int32_t verbosity; ///< 0=normal, 1=verbose, 2=verbose decoders, 3=debug decoders, 4=trace decoding.
    int32_t verbose_bits;
    conversion_mode_t conversion_mode;
    int32_t report_meta;
    int32_t report_noise;
    int32_t report_protocol;
    time_mode_t report_time;
    int32_t report_time_hires;
    int32_t report_time_tz;
    int32_t report_time_utc;
    int32_t report_description;
    int32_t report_stats;
    int32_t stats_interval;
    volatile sig_atomic_t stats_now;
    time_t stats_time;
    int32_t no_default_devices;
    struct r_device *devices;
    uint16_t num_r_devices;
    list_t data_tags;
    list_t output_handler;
    list_t raw_handler;
    int32_t has_logout;
    //struct dm_state *demod;
    uint8_t const *sr_filename;
    int32_t sr_execopen;
#ifndef DLL_RTL_433
    int32_t watchdog; ///< SDR acquire stall watchdog
#endif
	/* global stats */
	time_t running_since;           ///< program start time statistic
	uint32_t total_frames_count;    ///< total frames recieved statistic
	uint32_t total_frames_squelch;  ///< total frames with noise only statistic
	uint32_t total_frames_ook;      ///< total frames with ook demod statistic
	uint32_t total_frames_fsk;      ///< total frames with fsk demod statistic
	uint32_t total_frames_events;   ///< total frames with decoder events statistic
	/* sdr stats */
	time_t sdr_since; ///< time of last SDR connect statistic
	/* per report interval stats */
	time_t frames_since;    ///< time at start of report interval statistic
	uint32_t frames_ook;    ///< counter of ook demods for report interval statistic
	uint32_t frames_fsk;    ///< counter of fsk demods for report interval statistic
	uint32_t frames_events; ///< counter of decoder events for report interval statistic
	struct mg_mgr *mgr;
} r_cfg_t;

#endif /* INCLUDE_RTL_433_H_ */
