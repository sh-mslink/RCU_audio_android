/******************************************************************************
 *
 *  Copyright (C) 2009-2012 Broadcom Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

/*****************************************************************************
 *
 *  Filename:      audio_a2dp_hw.h
 *
 *  Description:
 *
 *****************************************************************************/

#ifndef AUDIO_SKLRM_HW_H
#define AUDIO_SKLRM_HW_H

/*****************************************************************************
**  Constants & Macros
******************************************************************************/

#define SKLRM_AUDIO_HARDWARE_INTERFACE "audio.sklrm"


#define AUDIO_STREAM_DEFAULT_RATE          16000
#define AUDIO_STREAM_DEFAULT_FORMAT        AUDIO_FORMAT_PCM_16_BIT
#define AUDIO_STREAM_DEFAULT_CHANNEL_FLAG  AUDIO_CHANNEL_IN_MONO

// AUDIO_STREAM_OUTPUT_BUFFER_SZ controls the size of the audio socket buffer.
// If one assumes the write buffer is always full during normal BT playback,
// then increasing this value increases our playback latency.
//
// FIXME: AUDIO_STREAM_OUTPUT_BUFFER_SZ should be controlled by the actual audio
// sample rate rather than being constant.
//
// FIXME: The BT HAL should consume data at a constant rate.
// AudioFlinger assumes that the HAL draws data at a constant rate, which is true
// for most audio devices; however, the BT engine reads data at a variable rate
// (over the short term), which confuses both AudioFlinger as well as applications
// which deliver data at a (generally) fixed rate.
//
// 20 * 512 is not sufficient size to smooth the variability for some BT devices,
// resulting in mixer sleep and throttling. We increase this to 28 * 512 to help
// reduce the effect of variable data consumption.


// AUDIO_STREAM_OUTPUT_BUFFER_PERIODS controls how the socket buffer is divided
// for AudioFlinger data delivery. The AudioFlinger mixer delivers data in chunks
// of AUDIO_STREAM_OUTPUT_BUFFER_SZ / AUDIO_STREAM_OUTPUT_BUFFER_PERIODS. If
// the number of periods is 2, the socket buffer represents "double buffering"
// of the AudioFlinger mixer buffer.
//
// In general, AUDIO_STREAM_OUTPUT_BUFFER_PERIODS * 16 * 4 should be a divisor of
// AUDIO_STREAM_OUTPUT_BUFFER_SZ.
//
// These values should be chosen such that
//
// AUDIO_STREAM_BUFFER_SIZE * 1000 / (AUDIO_STREAM_OUTPUT_BUFFER_PERIODS
//         * AUDIO_STREAM_DEFAULT_RATE * 4) > 20 (ms)
//
// to avoid introducing the FastMixer in AudioFlinger. Using the FastMixer results in
// unnecessary latency and CPU overhead for Bluetooth.



/*****************************************************************************
**  Type definitions for callback functions
******************************************************************************/

/*****************************************************************************
**  Type definitions and return values
******************************************************************************/

/*****************************************************************************
**  Extern variables and functions
******************************************************************************/

/*****************************************************************************
**  Functions
******************************************************************************/


/*****************************************************************************
**
** Function
**
** Description
**
** Returns
**
******************************************************************************/

#endif /* SKLRM_AUDIO_HW_H */

