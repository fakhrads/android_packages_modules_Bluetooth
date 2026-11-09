/******************************************************************************
    Copyright (c) 2018, The Linux Foundation. All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted (subject to the limitations in the
    disclaimer below) provided that the following conditions are met:

       * Redistributions of source code must retain the above copyright
         notice, this list of conditions and the following disclaimer.

       * Redistributions in binary form must reproduce the above
         copyright notice, this list of conditions and the following
         disclaimer in the documentation and/or other materials provided
         with the distribution.

       * Neither the name of The Linux Foundation nor the names of its
         contributors may be used to endorse or promote products derived
         from this software without specific prior written permission.

    NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
    GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
    HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
    WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
    MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
    IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
    ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
    GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
    IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
    OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
    IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************/

/******************************************************************************
 *
 *  Utility functions to help build and parse the aptX-adaptive Codec Information
 *  Element and Media Payload.
 *
 ******************************************************************************/

#define LOG_TAG "a2dp_vendor_aptx_adaptive"

#include "stack/include/a2dp_vendor_aptx_adaptive.h"

#include <bluetooth/log.h>
#include <string.h>

#include "osi/include/properties.h"
#include "stack/include/a2dp_vendor.h"
#include "stack/include/a2dp_vendor_aptx_adaptive_encoder.h"
#include "stack/include/a2dp_codec_api.h"
#include "internal_include/bt_target.h"
#include "internal_include/bt_trace.h"
// #include "bt_utils.h"
#include "hardware/bt_av.h"
#include "osi/include/osi.h"

using namespace bluetooth;

/* aptX-adaptive Source codec capabilities */
static const tA2DP_APTX_ADAPTIVE_CIE a2dp_aptx_adaptive_src_caps = {
        A2DP_APTX_ADAPTIVE_VENDOR_ID,                                              /* vendorId */
        A2DP_APTX_ADAPTIVE_CODEC_ID_BLUETOOTH,                                     /* codecId */
        A2DP_APTX_ADAPTIVE_SAMPLERATE_48000 | A2DP_APTX_ADAPTIVE_SAMPLERATE_96000, /* sampleRate */
        A2DP_APTX_ADAPTIVE_SOURCE_TYPE_2,
        (A2DP_APTX_ADAPTIVE_CHANNELS_JOINT_STEREO | A2DP_APTX_ADAPTIVE_CHANNELS_STEREO |
         A2DP_APTX_ADAPTIVE_CHANNELS_TWS_STEREO | A2DP_APTX_ADAPTIVE_CHANNELS_TWS_MONO |
         A2DP_APTX_ADAPTIVE_CHANNELS_TWS_PLUS),
        {A2DP_APTX_ADAPTIVE_TTP_LL_0, A2DP_APTX_ADAPTIVE_TTP_LL_1, A2DP_APTX_ADAPTIVE_TTP_HQ_0,
         A2DP_APTX_ADAPTIVE_TTP_HQ_1, A2DP_APTX_ADAPTIVE_TTP_TWS_0, A2DP_APTX_ADAPTIVE_TTP_TWS_1,
         A2DP_APTX_ADAPTIVE_RESERVED_15THBYTE, A2DP_APTX_ADAPTIVE_CAP_EXT_VER_NUM,
         A2DP_APTX_ADAPTIVE_SUPPORTED_FEATURES, A2DP_APTX_ADAPTIVE_FIRST_SETUP_PREF,
         A2DP_APTX_ADAPTIVE_SECOND_SETUP_PREF, A2DP_APTX_ADAPTIVE_THIRD_SETUP_PREF,
         A2DP_APTX_ADAPTIVE_FOURTH_SETUP_PREF, A2DP_APTX_ADAPTIVE_EOC0, A2DP_APTX_ADAPTIVE_EOC1},

        BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24, /* bits_per_sample */
        {0}};

/* Default aptX-adaptive codec configuration */
static const tA2DP_APTX_ADAPTIVE_CIE a2dp_aptx_adaptive_offload_caps = {
        A2DP_APTX_ADAPTIVE_VENDOR_ID,                                              /* vendorId */
        A2DP_APTX_ADAPTIVE_CODEC_ID_BLUETOOTH,                                     /* codecId */
        A2DP_APTX_ADAPTIVE_SAMPLERATE_48000 | A2DP_APTX_ADAPTIVE_SAMPLERATE_96000, /* sampleRate */
        A2DP_APTX_ADAPTIVE_SOURCE_TYPE_1,
        (A2DP_APTX_ADAPTIVE_CHANNELS_JOINT_STEREO | A2DP_APTX_ADAPTIVE_CHANNELS_STEREO |
         A2DP_APTX_ADAPTIVE_CHANNELS_TWS_STEREO | A2DP_APTX_ADAPTIVE_CHANNELS_TWS_MONO |
         A2DP_APTX_ADAPTIVE_CHANNELS_TWS_PLUS), /* channelMode */
        {A2DP_APTX_ADAPTIVE_TTP_LL_0, A2DP_APTX_ADAPTIVE_TTP_LL_1, A2DP_APTX_ADAPTIVE_TTP_HQ_0,
         A2DP_APTX_ADAPTIVE_TTP_HQ_1, A2DP_APTX_ADAPTIVE_TTP_TWS_0, A2DP_APTX_ADAPTIVE_TTP_TWS_1,
         A2DP_APTX_ADAPTIVE_RESERVED_15THBYTE, A2DP_APTX_ADAPTIVE_CAP_EXT_VER_NUM,
         A2DP_APTX_ADAPTIVE_SUPPORTED_FEATURES, A2DP_APTX_ADAPTIVE_FIRST_SETUP_PREF,
         A2DP_APTX_ADAPTIVE_SECOND_SETUP_PREF, A2DP_APTX_ADAPTIVE_THIRD_SETUP_PREF,
         A2DP_APTX_ADAPTIVE_FOURTH_SETUP_PREF, A2DP_APTX_ADAPTIVE_EOC0, A2DP_APTX_ADAPTIVE_EOC1},

        BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24, /* bits_per_sample */
        {0}};

/* Default aptX-adaptive R2.1 codec configuration */
static const tA2DP_APTX_ADAPTIVE_CIE a2dp_aptx_adaptive_r2_1_offload_caps = {
        A2DP_APTX_ADAPTIVE_VENDOR_ID,                                              /* vendorId */
        A2DP_APTX_ADAPTIVE_CODEC_ID_BLUETOOTH,                                     /* codecId */
        A2DP_APTX_ADAPTIVE_SAMPLERATE_48000 | A2DP_APTX_ADAPTIVE_SAMPLERATE_96000, /* sampleRate */
        A2DP_APTX_ADAPTIVE_SOURCE_TYPE_1,
        (A2DP_APTX_ADAPTIVE_CHANNELS_JOINT_STEREO | A2DP_APTX_ADAPTIVE_CHANNELS_STEREO |
         A2DP_APTX_ADAPTIVE_CHANNELS_TWS_STEREO | A2DP_APTX_ADAPTIVE_CHANNELS_TWS_MONO |
         A2DP_APTX_ADAPTIVE_CHANNELS_TWS_PLUS), /* channelMode */
        {A2DP_APTX_ADAPTIVE_TTP_LL_0, A2DP_APTX_ADAPTIVE_TTP_LL_1, A2DP_APTX_ADAPTIVE_TTP_HQ_0,
         A2DP_APTX_ADAPTIVE_TTP_HQ_1, A2DP_APTX_ADAPTIVE_TTP_TWS_0, A2DP_APTX_ADAPTIVE_TTP_TWS_1,
         A2DP_APTX_ADAPTIVE_RESERVED_15THBYTE, A2DP_APTX_ADAPTIVE_CAP_EXT_VER_NUM,
         A2DP_APTX_ADAPTIVE_R2_1_SUPPORTED_FEATURES, A2DP_APTX_ADAPTIVE_FIRST_SETUP_PREF,
         A2DP_APTX_ADAPTIVE_SECOND_SETUP_PREF, A2DP_APTX_ADAPTIVE_THIRD_SETUP_PREF,
         A2DP_APTX_ADAPTIVE_FOURTH_SETUP_PREF, A2DP_APTX_ADAPTIVE_EOC0, A2DP_APTX_ADAPTIVE_EOC1},

        BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24, /* bits_per_sample */
        {0}};

/* Default aptX-adaptive R2.2 codec configuration */
static const tA2DP_APTX_ADAPTIVE_CIE a2dp_aptx_adaptive_r2_2_offload_caps = {
        A2DP_APTX_ADAPTIVE_VENDOR_ID,          /* vendorId */
        A2DP_APTX_ADAPTIVE_CODEC_ID_BLUETOOTH, /* codecId */
        A2DP_APTX_ADAPTIVE_SAMPLERATE_44100 | A2DP_APTX_ADAPTIVE_SAMPLERATE_48000 |
                A2DP_APTX_ADAPTIVE_SAMPLERATE_96000, /* sampleRate */
        A2DP_APTX_ADAPTIVE_SOURCE_TYPE_1,
        (A2DP_APTX_ADAPTIVE_CHANNELS_JOINT_STEREO | A2DP_APTX_ADAPTIVE_CHANNELS_STEREO |
         A2DP_APTX_ADAPTIVE_CHANNELS_TWS_STEREO | A2DP_APTX_ADAPTIVE_CHANNELS_TWS_MONO |
         A2DP_APTX_ADAPTIVE_CHANNELS_TWS_PLUS), /* channelMode */
        {A2DP_APTX_ADAPTIVE_TTP_LL_0, A2DP_APTX_ADAPTIVE_TTP_LL_1, A2DP_APTX_ADAPTIVE_TTP_HQ_0,
         A2DP_APTX_ADAPTIVE_TTP_HQ_1, A2DP_APTX_ADAPTIVE_TTP_TWS_0, A2DP_APTX_ADAPTIVE_TTP_TWS_1,
         A2DP_APTX_ADAPTIVE_RESERVED_15THBYTE, A2DP_APTX_ADAPTIVE_CAP_EXT_VER_NUM,
         A2DP_APTX_ADAPTIVE_R2_2_SUPPORTED_FEATURES, A2DP_APTX_ADAPTIVE_FIRST_SETUP_PREF,
         A2DP_APTX_ADAPTIVE_SECOND_SETUP_PREF, A2DP_APTX_ADAPTIVE_THIRD_SETUP_PREF,
         A2DP_APTX_ADAPTIVE_FOURTH_SETUP_PREF, A2DP_APTX_ADAPTIVE_EOC0, A2DP_APTX_ADAPTIVE_EOC1},

        BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24, /* bits_per_sample */
        {0}};

/* Default aptX-adaptive codec configuration */
static const tA2DP_APTX_ADAPTIVE_CIE a2dp_aptx_adaptive_default_src_config = {
        A2DP_APTX_ADAPTIVE_VENDOR_ID,          /* vendorId */
        A2DP_APTX_ADAPTIVE_CODEC_ID_BLUETOOTH, /* codecId */
        A2DP_APTX_ADAPTIVE_SAMPLERATE_48000,   /* sampleRate */
        A2DP_APTX_ADAPTIVE_SOURCE_TYPE_2,
        A2DP_APTX_ADAPTIVE_CHANNELS_STEREO | A2DP_APTX_ADAPTIVE_CHANNELS_TWS_PLUS, /* channelMode */
        {A2DP_APTX_ADAPTIVE_TTP_LL_0, A2DP_APTX_ADAPTIVE_TTP_LL_1, A2DP_APTX_ADAPTIVE_TTP_HQ_0,
         A2DP_APTX_ADAPTIVE_TTP_HQ_1, A2DP_APTX_ADAPTIVE_TTP_TWS_0, A2DP_APTX_ADAPTIVE_TTP_TWS_1,
         A2DP_APTX_ADAPTIVE_RESERVED_15THBYTE, A2DP_APTX_ADAPTIVE_CAP_EXT_VER_NUM,
         A2DP_APTX_ADAPTIVE_SUPPORTED_FEATURES, A2DP_APTX_ADAPTIVE_FIRST_SETUP_PREF,
         A2DP_APTX_ADAPTIVE_SECOND_SETUP_PREF, A2DP_APTX_ADAPTIVE_THIRD_SETUP_PREF,
         A2DP_APTX_ADAPTIVE_FOURTH_SETUP_PREF, A2DP_APTX_ADAPTIVE_EOC0, A2DP_APTX_ADAPTIVE_EOC1},

        BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24, /* bits_per_sample */
        {0}};

/* Default aptX-adaptive offload codec configuration */
static const tA2DP_APTX_ADAPTIVE_CIE a2dp_aptx_adaptive_default_offload_config = {
        A2DP_APTX_ADAPTIVE_VENDOR_ID,          /* vendorId */
        A2DP_APTX_ADAPTIVE_CODEC_ID_BLUETOOTH, /* codecId */
        A2DP_APTX_ADAPTIVE_SAMPLERATE_48000,   /* sampleRate */
        A2DP_APTX_ADAPTIVE_SOURCE_TYPE_1,
        (A2DP_APTX_ADAPTIVE_CHANNELS_JOINT_STEREO | A2DP_APTX_ADAPTIVE_CHANNELS_STEREO |
         A2DP_APTX_ADAPTIVE_CHANNELS_TWS_STEREO | A2DP_APTX_ADAPTIVE_CHANNELS_TWS_MONO |
         A2DP_APTX_ADAPTIVE_CHANNELS_TWS_PLUS), /* channelMode */
        {A2DP_APTX_ADAPTIVE_TTP_LL_0, A2DP_APTX_ADAPTIVE_TTP_LL_1, A2DP_APTX_ADAPTIVE_TTP_HQ_0,
         A2DP_APTX_ADAPTIVE_TTP_HQ_1, A2DP_APTX_ADAPTIVE_TTP_TWS_0, A2DP_APTX_ADAPTIVE_TTP_TWS_1,
         A2DP_APTX_ADAPTIVE_RESERVED_15THBYTE, A2DP_APTX_ADAPTIVE_CAP_EXT_VER_NUM,
         A2DP_APTX_ADAPTIVE_SUPPORTED_FEATURES, A2DP_APTX_ADAPTIVE_FIRST_SETUP_PREF,
         A2DP_APTX_ADAPTIVE_SECOND_SETUP_PREF, A2DP_APTX_ADAPTIVE_THIRD_SETUP_PREF,
         A2DP_APTX_ADAPTIVE_FOURTH_SETUP_PREF, A2DP_APTX_ADAPTIVE_EOC0, A2DP_APTX_ADAPTIVE_EOC1},

        BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24, /* bits_per_sample */
        {0}};

/* Default aptX-adaptive R2.1 offload codec configuration */
static const tA2DP_APTX_ADAPTIVE_CIE a2dp_aptx_adaptive_r2_1_default_offload_config = {
        A2DP_APTX_ADAPTIVE_VENDOR_ID,          /* vendorId */
        A2DP_APTX_ADAPTIVE_CODEC_ID_BLUETOOTH, /* codecId */
        A2DP_APTX_ADAPTIVE_SAMPLERATE_48000,   /* sampleRate */
        A2DP_APTX_ADAPTIVE_SOURCE_TYPE_1,
        (A2DP_APTX_ADAPTIVE_CHANNELS_JOINT_STEREO | A2DP_APTX_ADAPTIVE_CHANNELS_STEREO |
         A2DP_APTX_ADAPTIVE_CHANNELS_TWS_STEREO | A2DP_APTX_ADAPTIVE_CHANNELS_TWS_MONO |
         A2DP_APTX_ADAPTIVE_CHANNELS_TWS_PLUS), /* channelMode */
        {A2DP_APTX_ADAPTIVE_TTP_LL_0, A2DP_APTX_ADAPTIVE_TTP_LL_1, A2DP_APTX_ADAPTIVE_TTP_HQ_0,
         A2DP_APTX_ADAPTIVE_TTP_HQ_1, A2DP_APTX_ADAPTIVE_TTP_TWS_0, A2DP_APTX_ADAPTIVE_TTP_TWS_1,
         A2DP_APTX_ADAPTIVE_RESERVED_15THBYTE, A2DP_APTX_ADAPTIVE_CAP_EXT_VER_NUM,
         A2DP_APTX_ADAPTIVE_R2_1_SUPPORTED_FEATURES, A2DP_APTX_ADAPTIVE_FIRST_SETUP_PREF,
         A2DP_APTX_ADAPTIVE_SECOND_SETUP_PREF, A2DP_APTX_ADAPTIVE_THIRD_SETUP_PREF,
         A2DP_APTX_ADAPTIVE_FOURTH_SETUP_PREF, A2DP_APTX_ADAPTIVE_EOC0, A2DP_APTX_ADAPTIVE_EOC1},

        BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24, /* bits_per_sample */
        {0}};

/* Default aptX-adaptive R2.2 offload codec configuration */
static const tA2DP_APTX_ADAPTIVE_CIE a2dp_aptx_adaptive_r2_2_default_offload_config = {
        A2DP_APTX_ADAPTIVE_VENDOR_ID,          /* vendorId */
        A2DP_APTX_ADAPTIVE_CODEC_ID_BLUETOOTH, /* codecId */
        A2DP_APTX_ADAPTIVE_SAMPLERATE_48000,   /* sampleRate */
        A2DP_APTX_ADAPTIVE_SOURCE_TYPE_1,
        (A2DP_APTX_ADAPTIVE_CHANNELS_JOINT_STEREO | A2DP_APTX_ADAPTIVE_CHANNELS_STEREO |
         A2DP_APTX_ADAPTIVE_CHANNELS_TWS_STEREO | A2DP_APTX_ADAPTIVE_CHANNELS_TWS_MONO |
         A2DP_APTX_ADAPTIVE_CHANNELS_TWS_PLUS), /* channelMode */
        {A2DP_APTX_ADAPTIVE_TTP_LL_0, A2DP_APTX_ADAPTIVE_TTP_LL_1, A2DP_APTX_ADAPTIVE_TTP_HQ_0,
         A2DP_APTX_ADAPTIVE_TTP_HQ_1, A2DP_APTX_ADAPTIVE_TTP_TWS_0, A2DP_APTX_ADAPTIVE_TTP_TWS_1,
         A2DP_APTX_ADAPTIVE_RESERVED_15THBYTE, A2DP_APTX_ADAPTIVE_CAP_EXT_VER_NUM,
         A2DP_APTX_ADAPTIVE_R2_2_SUPPORTED_FEATURES, A2DP_APTX_ADAPTIVE_FIRST_SETUP_PREF,
         A2DP_APTX_ADAPTIVE_SECOND_SETUP_PREF, A2DP_APTX_ADAPTIVE_THIRD_SETUP_PREF,
         A2DP_APTX_ADAPTIVE_FOURTH_SETUP_PREF, A2DP_APTX_ADAPTIVE_EOC0, A2DP_APTX_ADAPTIVE_EOC1},

        BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24, /* bits_per_sample */
        {0}};

static const tA2DP_APTX_ADAPTIVE_CIE a2dp_aptx_adaptive_r1_offload_caps = {
        A2DP_APTX_ADAPTIVE_VENDOR_ID,          /* vendorId */
        A2DP_APTX_ADAPTIVE_CODEC_ID_BLUETOOTH, /* codecId */
        A2DP_APTX_ADAPTIVE_SAMPLERATE_48000,   /* sampleRate */
        A2DP_APTX_ADAPTIVE_SOURCE_TYPE_1,
        (A2DP_APTX_ADAPTIVE_CHANNELS_JOINT_STEREO | A2DP_APTX_ADAPTIVE_CHANNELS_STEREO |
         A2DP_APTX_ADAPTIVE_CHANNELS_TWS_STEREO |
         A2DP_APTX_ADAPTIVE_CHANNELS_TWS_MONO), /* channelMode */
        {A2DP_APTX_ADAPTIVE_TTP_LL_0, A2DP_APTX_ADAPTIVE_TTP_LL_1, A2DP_APTX_ADAPTIVE_TTP_HQ_0,
         A2DP_APTX_ADAPTIVE_TTP_HQ_1, A2DP_APTX_ADAPTIVE_TTP_TWS_0, A2DP_APTX_ADAPTIVE_TTP_TWS_1,
         0x00, A2DP_APTX_ADAPTIVE_EOC0, A2DP_APTX_ADAPTIVE_EOC1, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00},

        BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24, /* bits_per_sample */
        {0}};

static const tA2DP_APTX_ADAPTIVE_CIE a2dp_aptx_adaptive_r1_default_offload_config = {
        A2DP_APTX_ADAPTIVE_VENDOR_ID,          /* vendorId */
        A2DP_APTX_ADAPTIVE_CODEC_ID_BLUETOOTH, /* codecId */
        A2DP_APTX_ADAPTIVE_SAMPLERATE_48000,   /* sampleRate */
        A2DP_APTX_ADAPTIVE_SOURCE_TYPE_1,
        (A2DP_APTX_ADAPTIVE_CHANNELS_JOINT_STEREO | A2DP_APTX_ADAPTIVE_CHANNELS_STEREO |
         A2DP_APTX_ADAPTIVE_CHANNELS_TWS_STEREO |
         A2DP_APTX_ADAPTIVE_CHANNELS_TWS_MONO), /* channelMode */
        {A2DP_APTX_ADAPTIVE_TTP_LL_0, A2DP_APTX_ADAPTIVE_TTP_LL_1, A2DP_APTX_ADAPTIVE_TTP_HQ_0,
         A2DP_APTX_ADAPTIVE_TTP_HQ_1, A2DP_APTX_ADAPTIVE_TTP_TWS_0, A2DP_APTX_ADAPTIVE_TTP_TWS_1,
         0x00, A2DP_APTX_ADAPTIVE_EOC0, A2DP_APTX_ADAPTIVE_EOC1, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00},

        BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24, /* bits_per_sample */
        {0}};

tA2DP_APTX_ADAPTIVE_CIE a2dp_aptx_adaptive_caps, a2dp_aptx_adaptive_default_config;

static const tA2DP_ENCODER_INTERFACE a2dp_encoder_interface_aptx_adaptive = {
        a2dp_vendor_aptx_adaptive_encoder_init,
        a2dp_vendor_aptx_adaptive_encoder_cleanup,
        a2dp_vendor_aptx_adaptive_feeding_reset,
        a2dp_vendor_aptx_adaptive_feeding_flush,
        a2dp_vendor_aptx_adaptive_get_encoder_interval_ms,  // _get_encoder_interval_ms
        a2dp_vendor_aptx_adaptive_get_effective_frame_size,
        a2dp_vendor_aptx_adaptive_send_frames,
        nullptr  // set_transmit_queue_length
};

UNUSED_ATTR static tA2DP_STATUS A2DP_CodecInfoMatchesCapabilityAptxAdaptive(
        const tA2DP_APTX_ADAPTIVE_CIE* p_cap, const uint8_t* p_codec_info, bool is_peer_codec_info);

// Builds the aptX-adaptive Media Codec Capabilities byte sequence beginning from the
// LOSC octet. |media_type| is the media type |AVDT_MEDIA_TYPE_*|.
// |p_ie| is a pointer to the aptX-adaptive Codec Information Element information.
// The result is stored in |p_result|. Returns A2DP_SUCCESS on success,
// otherwise the corresponding A2DP error status code.
static tA2DP_STATUS A2DP_BuildInfoAptxAdaptive(uint8_t media_type,
                                               const tA2DP_APTX_ADAPTIVE_CIE* p_ie,
                                               uint8_t* p_result) {
  if (p_ie == NULL || p_result == NULL) {
    return AVDTP_UNSUPPORTED_CONFIGURATION;
  }

  *p_result++ = A2DP_APTX_ADAPTIVE_CODEC_LEN;
  *p_result++ = (media_type << 4);
  *p_result++ = A2DP_MEDIA_CT_NON_A2DP;
  *p_result++ = (uint8_t)(p_ie->vendorId & 0x000000FF);
  *p_result++ = (uint8_t)((p_ie->vendorId & 0x0000FF00) >> 8);
  *p_result++ = (uint8_t)((p_ie->vendorId & 0x00FF0000) >> 16);
  *p_result++ = (uint8_t)((p_ie->vendorId & 0xFF000000) >> 24);
  *p_result++ = (uint8_t)(p_ie->codecId & 0x00FF);
  *p_result++ = (uint8_t)((p_ie->codecId & 0xFF00) >> 8);
  *p_result++ = p_ie->sampleRate | p_ie->sourceType;
  *p_result++ = p_ie->channelMode;

  memcpy(p_result, &(p_ie->aptx_data), sizeof(p_ie->aptx_data));
  p_result += 18;
  memset(p_result, 0x0, sizeof(p_ie->reserved_data));
  p_result += sizeof(p_ie->reserved_data);

  return A2DP_SUCCESS;
}

// Parses the aptX-adaptive Media Codec Capabilities byte sequence beginning from the
// LOSC octet. The result is stored in |p_ie|. The byte sequence to parse is
// |p_codec_info|. If |is_capability| is true, the byte sequence is
// codec capabilities, otherwise is codec configuration.
// Returns A2DP_SUCCESS on success, otherwise the corresponding A2DP error
// status code.
static tA2DP_STATUS A2DP_ParseInfoAptxAdaptive(tA2DP_APTX_ADAPTIVE_CIE* p_ie,
                                               const uint8_t* p_codec_info, bool is_capability) {
  uint8_t losc;
  uint8_t media_type;
  tA2DP_CODEC_TYPE codec_type;

  log::info("p_ie = {}, p_codec_info = {}", std::format_ptr(p_ie), std::format_ptr(p_codec_info));
  if (p_ie == NULL || p_codec_info == NULL) {
    return AVDTP_UNSUPPORTED_CONFIGURATION;
  }

  // Check the codec capability length
  losc = *p_codec_info++;
  log::info("losc: 0x{:x}", losc);

  if (losc != A2DP_APTX_ADAPTIVE_CODEC_LEN) {
    log::info("A2DP_APTX_ADAPTIVE_CODEC_LEN fail");
    return AVDTP_UNSUPPORTED_CONFIGURATION;
  }

  media_type = (*p_codec_info++) >> 4;
  codec_type = static_cast<tA2DP_CODEC_TYPE>(*p_codec_info++);
  log::info("media_type: {}, codec_type: 0x{:x}", media_type, codec_type);
  /* Check the Media Type and Media Codec Type */
  if (media_type != AVDT_MEDIA_TYPE_AUDIO || codec_type != A2DP_MEDIA_CT_NON_A2DP) {
    log::info("A2DP_MEDIA_CT_NON_A2DP ID");
    return AVDTP_UNSUPPORTED_CONFIGURATION;
  }

  // Check the Vendor ID and Codec ID */
  p_ie->vendorId = (*p_codec_info & 0x000000FF) | (*(p_codec_info + 1) << 8 & 0x0000FF00) |
                   (*(p_codec_info + 2) << 16 & 0x00FF0000) |
                   (*(p_codec_info + 3) << 24 & 0xFF000000);
  p_codec_info += 4;
  p_ie->codecId = (*p_codec_info & 0x00FF) | (*(p_codec_info + 1) << 8 & 0xFF00);
  p_codec_info += 2;
  log::info("codecId: {:2x}, vendorId: {:4x}", p_ie->codecId, p_ie->vendorId);
  if (p_ie->vendorId != A2DP_APTX_ADAPTIVE_VENDOR_ID ||
      p_ie->codecId != A2DP_APTX_ADAPTIVE_CODEC_ID_BLUETOOTH) {
    log::info("A2DP_APTX_ADAPTIVE ID WRONG CODEC");
    return AVDTP_UNSUPPORTED_CONFIGURATION;
  }

  p_ie->sourceType = *p_codec_info & 0x07;
  p_ie->sampleRate = *p_codec_info & 0xF8;
  p_codec_info++;

  p_ie->channelMode = *p_codec_info & 0x3F;
  p_codec_info++;
  log::info("channelMode: 0x{:x}, sourceType: 0x{:x}, sampleRate: 0x{:x}", p_ie->channelMode,
                                                          p_ie->sourceType, p_ie->sampleRate);

  memcpy(&(p_ie->aptx_data), p_codec_info, sizeof(p_ie->aptx_data));
  log::info("aptx_adaptive_sup_features: 0x{:4x}", p_ie->aptx_data.aptx_adaptive_sup_features);
  p_codec_info += 18;

  if (is_capability) {
    return A2DP_SUCCESS;
  }

  //  if (A2DP_BitsSet(p_ie->sampleRate) != A2DP_SET_ONE_BIT)
  //    return A2DP_BAD_SAMP_FREQ;
  if (A2DP_BitsSet(p_ie->channelMode) != A2DP_SET_ONE_BIT) {
    return AVDTP_UNSUPPORTED_CONFIGURATION;
  }

  log::info("btav_a2dp_codec_bits_per_sample_t: {}", sizeof(btav_a2dp_codec_bits_per_sample_t));

  return A2DP_SUCCESS;
}

bool A2DP_IsVendorSourceCodecValidAptxAdaptive(const uint8_t* p_codec_info) {
  tA2DP_APTX_ADAPTIVE_CIE cfg_cie;

  /* Use a liberal check when parsing the codec info */
  return (A2DP_ParseInfoAptxAdaptive(&cfg_cie, p_codec_info, false) == A2DP_SUCCESS) ||
         (A2DP_ParseInfoAptxAdaptive(&cfg_cie, p_codec_info, true) == A2DP_SUCCESS);
}

bool A2DP_GetAptxAdaptiveCIE(const uint8_t* p_codec_info, tA2DP_APTX_ADAPTIVE_CIE* cfg_cie) {
  tA2DP_STATUS status = A2DP_ParseInfoAptxAdaptive(cfg_cie, p_codec_info, false);
  log::info("status: {}", status);
  if (status == A2DP_SUCCESS) {
    return true;
  }
  log::error("returning false");
  return false;
  // return (A2DP_ParseInfoAptxAdaptive(cfg_cie, p_codec_info, false) ==
  //         A2DP_SUCCESS);
}
bool A2DP_IsVendorPeerSinkCodecValidAptxAdaptive(const uint8_t* p_codec_info) {
  tA2DP_APTX_ADAPTIVE_CIE cfg_cie;

  /* Use a liberal check when parsing the codec info */
  return (A2DP_ParseInfoAptxAdaptive(&cfg_cie, p_codec_info, false) == A2DP_SUCCESS) ||
         (A2DP_ParseInfoAptxAdaptive(&cfg_cie, p_codec_info, true) == A2DP_SUCCESS);
}

// Checks whether A2DP aptX-adaptive codec configuration matches with a device's
// codec capabilities. |p_cap| is the aptX-adaptive codec configuration.
// |p_codec_info| is the device's codec capabilities.
// If |is_capability| is true, the byte sequence is codec capabilities,
// otherwise is codec configuration.
// |p_codec_info| contains the codec capabilities for a peer device that
// is acting as an A2DP source.
// Returns A2DP_SUCCESS if the codec configuration matches with capabilities,
// otherwise the corresponding A2DP error status code.
static tA2DP_STATUS A2DP_CodecInfoMatchesCapabilityAptxAdaptive(
        const tA2DP_APTX_ADAPTIVE_CIE* p_cap, const uint8_t* p_codec_info, bool is_capability) {
  tA2DP_STATUS status;
  tA2DP_APTX_ADAPTIVE_CIE cfg_cie;

  /* parse configuration */
  status = A2DP_ParseInfoAptxAdaptive(&cfg_cie, p_codec_info, is_capability);
  if (status != A2DP_SUCCESS) {
    return status;
  }

  /* verify that each parameter is in range */

  /* sampling frequency */
  if ((cfg_cie.sampleRate & p_cap->sampleRate) == 0) {
    return A2DP_INVALID_SAMPLING_FREQUENCY;
  }

  /* channel mode */
  if ((cfg_cie.channelMode & p_cap->channelMode) == 0) {
    return A2DP_INVALID_CHANNEL_MODE;
  }

  return A2DP_SUCCESS;
}

bool A2DP_VendorUsesRtpHeaderAptxAdaptive(UNUSED_ATTR bool content_protection_enabled,
                                          UNUSED_ATTR const uint8_t* p_codec_info) {
  return true;
}

const char* A2DP_VendorCodecNameAptxAdaptive(UNUSED_ATTR const uint8_t* p_codec_info) {
  return "aptX-adaptive";
}

bool A2DP_VendorCodecTypeEqualsAptxAdaptive(const uint8_t* p_codec_info_a,
                                            const uint8_t* p_codec_info_b) {
  tA2DP_APTX_ADAPTIVE_CIE aptx_adaptive_cie_a;
  tA2DP_APTX_ADAPTIVE_CIE aptx_adaptive_cie_b;

  // Check whether the codec info contains valid data
  tA2DP_STATUS a2dp_status = A2DP_ParseInfoAptxAdaptive(&aptx_adaptive_cie_a, p_codec_info_a, true);
  if (a2dp_status != A2DP_SUCCESS) {
    return false;
  }
  a2dp_status = A2DP_ParseInfoAptxAdaptive(&aptx_adaptive_cie_b, p_codec_info_b, true);
  if (a2dp_status != A2DP_SUCCESS) {
    return false;
  }

  return true;
}

bool A2DP_VendorCodecEqualsAptxAdaptive(const uint8_t* p_codec_info_a,
                                        const uint8_t* p_codec_info_b) {
  tA2DP_APTX_ADAPTIVE_CIE aptx_adaptive_cie_a;
  tA2DP_APTX_ADAPTIVE_CIE aptx_adaptive_cie_b;

  // Check whether the codec info contains valid data
  tA2DP_STATUS a2dp_status = A2DP_ParseInfoAptxAdaptive(&aptx_adaptive_cie_a, p_codec_info_a, true);
  if (a2dp_status != A2DP_SUCCESS) {
    return false;
  }
  a2dp_status = A2DP_ParseInfoAptxAdaptive(&aptx_adaptive_cie_b, p_codec_info_b, true);
  if (a2dp_status != A2DP_SUCCESS) {
    return false;
  }

  return (aptx_adaptive_cie_a.sampleRate == aptx_adaptive_cie_b.sampleRate) &&
         (aptx_adaptive_cie_a.channelMode == aptx_adaptive_cie_b.channelMode);
}

int A2DP_VendorGetTrackSampleRateAptxAdaptive(const uint8_t* p_codec_info) {
  tA2DP_APTX_ADAPTIVE_CIE aptx_adaptive_cie;

  // Check whether the codec info contains valid data
  tA2DP_STATUS a2dp_status = A2DP_ParseInfoAptxAdaptive(&aptx_adaptive_cie, p_codec_info, false);
  if (a2dp_status != A2DP_SUCCESS) {
    return -1;
  }

  if (aptx_adaptive_cie.sampleRate == A2DP_APTX_ADAPTIVE_SAMPLERATE_44100 ||
      aptx_adaptive_cie.sampleRate == (A2DP_APTX_ADAPTIVE_SAMPLERATE_44100|A2DP_APTX_ADAPTIVE_SAMPLERATE_RESERVED)) {
    return 44100;
  }
  if (aptx_adaptive_cie.sampleRate == A2DP_APTX_ADAPTIVE_SAMPLERATE_48000 ||
      aptx_adaptive_cie.sampleRate == (A2DP_APTX_ADAPTIVE_SAMPLERATE_48000|A2DP_APTX_ADAPTIVE_SAMPLERATE_RESERVED)) {
    return 48000;
  }
  if (aptx_adaptive_cie.sampleRate == A2DP_APTX_ADAPTIVE_SAMPLERATE_96000) {
    return 96000;
  }

  return -1;
}

int A2DP_VendorGetTrackBitsPerSampleAptxAdaptive(const uint8_t* p_codec_info) {
  tA2DP_APTX_ADAPTIVE_CIE aptx_adaptive_cie;

  // Check whether the codec info contains valid data
  tA2DP_STATUS a2dp_status = A2DP_ParseInfoAptxAdaptive(&aptx_adaptive_cie, p_codec_info, false);
  if (a2dp_status != A2DP_SUCCESS) {
    return -1;
  }

  return 16;
}

int A2DP_VendorGetTrackChannelCountAptxAdaptive(const uint8_t* p_codec_info) {
  tA2DP_APTX_ADAPTIVE_CIE aptx_adaptive_cie;

  // Check whether the codec info contains valid data
  tA2DP_STATUS a2dp_status = A2DP_ParseInfoAptxAdaptive(&aptx_adaptive_cie, p_codec_info, false);
  if (a2dp_status != A2DP_SUCCESS) {
    return -1;
  }

  switch (aptx_adaptive_cie.channelMode) {
    case A2DP_APTX_ADAPTIVE_CHANNELS_MONO:
      return 1;
    case A2DP_APTX_ADAPTIVE_CHANNELS_TWS_MONO:
      return 1;
    case A2DP_APTX_ADAPTIVE_CHANNELS_STEREO:
      return 2;
    case A2DP_APTX_ADAPTIVE_CHANNELS_JOINT_STEREO:
      return 2;
    case A2DP_APTX_ADAPTIVE_CHANNELS_TWS_STEREO:
      return 2;
  }

  return -1;
}

bool A2DP_VendorGetPacketTimestampAptxAdaptive(UNUSED_ATTR const uint8_t* p_codec_info,
                                               const uint8_t* p_data, uint32_t* p_timestamp) {
  // TODO: Is this function really codec-specific?
  *p_timestamp = *(const uint32_t*)p_data;
  return true;
}

bool A2DP_VendorBuildCodecHeaderAptxAdaptive(UNUSED_ATTR const uint8_t* p_codec_info,
                                             UNUSED_ATTR BT_HDR* p_buf,
                                             UNUSED_ATTR uint16_t frames_per_packet) {
  // Nothing to do
  return true;
}

bool A2DP_VendorDumpCodecInfoAptxAdaptive(const uint8_t* p_codec_info) {
  tA2DP_STATUS a2dp_status;
  tA2DP_APTX_ADAPTIVE_CIE aptx_adaptive_cie;

  a2dp_status = A2DP_ParseInfoAptxAdaptive(&aptx_adaptive_cie, p_codec_info, true);
  if (a2dp_status != A2DP_SUCCESS) {
    return false;
  }

  return true;
}

std::string A2DP_VendorCodecInfoStringAptxAd(const uint8_t* p_codec_info) {
  std::stringstream res;
  std::string field;
  tA2DP_STATUS a2dp_status;
  tA2DP_APTX_ADAPTIVE_CIE aptxAd_cie;

  a2dp_status = A2DP_ParseInfoAptxAdaptive(&aptxAd_cie, p_codec_info, true);
  if (a2dp_status != A2DP_SUCCESS) {
    res << "A2DP_ParseInfoAptxAdaptive fail: " << loghex(static_cast<uint8_t>(a2dp_status));
    return res.str();
  }

  res << "\tname: Aptx Adaptive\n";

  // Sample frequency
  field.clear();
  AppendField(&field, (aptxAd_cie.sampleRate == 0), "NONE");
  AppendField(&field, (aptxAd_cie.sampleRate & A2DP_APTX_ADAPTIVE_SAMPLERATE_44100), "44100");
  AppendField(&field, (aptxAd_cie.sampleRate & A2DP_APTX_ADAPTIVE_SAMPLERATE_48000), "48000");
  AppendField(&field, (aptxAd_cie.sampleRate & A2DP_APTX_ADAPTIVE_SAMPLERATE_96000), "96000");
  res << "\tsamp_freq: " << field << " (" << loghex(aptxAd_cie.sampleRate) << ")\n";

  // Channel mode
  field.clear();
  AppendField(&field, (aptxAd_cie.channelMode == 0), "NONE");
  AppendField(&field, (aptxAd_cie.channelMode & A2DP_APTX_ADAPTIVE_CHANNELS_MONO), "Mono");
  AppendField(&field, (aptxAd_cie.channelMode & A2DP_APTX_ADAPTIVE_CHANNELS_STEREO), "Stereo");
  AppendField(&field, (aptxAd_cie.channelMode & A2DP_APTX_ADAPTIVE_CHANNELS_JOINT_STEREO),
              "Joint Stereo");
  res << "\tch_mode: " << field << " (" << loghex(aptxAd_cie.channelMode) << ")\n";

  return res.str();
}

const tA2DP_ENCODER_INTERFACE* A2DP_VendorGetEncoderInterfaceAptxAdaptive(
        const uint8_t* p_codec_info) {
  if (!A2DP_IsVendorSourceCodecValidAptxAdaptive(p_codec_info)) {
    return NULL;
  }

  return &a2dp_encoder_interface_aptx_adaptive;
}

bool A2DP_VendorAdjustCodecAptxAdaptive(uint8_t* p_codec_info) {
  tA2DP_APTX_ADAPTIVE_CIE cfg_cie;

  // Nothing to do: just verify the codec info is valid
  if (A2DP_ParseInfoAptxAdaptive(&cfg_cie, p_codec_info, true) != A2DP_SUCCESS) {
    return false;
  }

  return true;
}

tA2DP_STATUS A2DP_VendorIsCodecConfigMatchAptxAdaptive(const uint8_t* p_codec_info) {
  tA2DP_APTX_ADAPTIVE_CIE aptx_adaptive_cie;

  tA2DP_STATUS a2dp_status = A2DP_ParseInfoAptxAdaptive(&aptx_adaptive_cie, p_codec_info, false);
  return a2dp_status;
}

btav_a2dp_codec_index_t A2DP_VendorSourceCodecIndexAptxAdaptive(
        [[maybe_unused]] const uint8_t* p_codec_info) {
  return BTAV_A2DP_CODEC_INDEX_SOURCE_APTX_ADAPTIVE;
}

const char* A2DP_VendorCodecIndexStrAptxAdaptive(void) { return "aptX-adaptive"; }

bool A2DP_VendorInitCodecConfigAptxAdaptive(AvdtpSepConfig* p_cfg) {
  /*if (!A2DP_IsCodecEnabled(BTAV_A2DP_CODEC_INDEX_SOURCE_APTX_ADAPTIVE)) {
    LOG_ERROR(LOG_TAG, "%s: APTX-ADAPTIVE disabled in both SW and HW mode",
                                                        __func__);
    return false;
  }*/

  if (A2DP_BuildInfoAptxAdaptive(AVDT_MEDIA_TYPE_AUDIO, &a2dp_aptx_adaptive_caps,
                                 p_cfg->codec_info) != A2DP_SUCCESS) {
    return false;
  }

#if (BTA_AV_CO_CP_SCMS_T == TRUE)
  /* Content protection info - support SCMS-T */
  uint8_t* p = p_cfg->protect_info;
  *p++ = AVDT_CP_LOSC;
  UINT16_TO_STREAM(p, AVDT_CP_SCMS_T_ID);
  p_cfg->num_protect = 1;
#endif

  return true;
}

// Certain chip versions have no working aptX Adaptive implementation. The BT
// transport HAL publishes the chip-version string in
// persist.vendor.qcom.bluetooth.soc_ver (DataHandler::SetSocVersionProperty ->
// convertChipVerToStr). Returns true if the running SoC is on the aptX Adaptive
// unsupported list, so callers (e.g. A2dpCodecs::init) can disable the codec's
// SEP and keep it out of GET_ALL_CAPABILITIES, Developer Options and the codec
// picker.
//
bool A2DP_IsAptxAdaptiveDisabledForSoc() {
  static const char* const kAptxAdaptiveUnsupportedSocVersions[] = {
      "COMANCHE_VER_1_0_1", "COMANCHE_VER_1_1",     "COMANCHE_VER_1_2_UMC",
      "COMANCHE_VER_1_2",   "COMANCHE_VER_1_3_UMC", "COMANCHE_VER_1_3_TSMC",
      "COMANCHE_VER_1_3",   "APACHE_VER_1_0_0",     "APACHE_VER_1_1_0",
      "APACHE_VER_1_2_0",   "APACHE_VER_1_2_1",     "CHEROKEE_VER_2_0",
      "CHEROKEE_VER_2_0_1", "CHEROKEE_VER_2_1",     "CHEROKEE_VER_2_1_1",
  };
  char soc_ver[PROPERTY_VALUE_MAX] = {'\0'};
  osi_property_get("persist.vendor.qcom.bluetooth.soc_ver", soc_ver, "");
  for (const char* unsupported : kAptxAdaptiveUnsupportedSocVersions) {
    if (!strcmp(soc_ver, unsupported)) {
      log::info("aptX Adaptive disabled for unsupported SoC {}", soc_ver);
      return true;
    }
  }
  return false;
}

A2dpCodecConfigAptxAdaptive::A2dpCodecConfigAptxAdaptive(btav_a2dp_codec_priority_t codec_priority)
    : A2dpCodecConfig(BTAV_A2DP_CODEC_INDEX_SOURCE_APTX_ADAPTIVE, bluetooth::a2dp::CodecId::APTX_AD,
                      "aptX-adaptive", codec_priority) {
  // Compute the local capability
  log::info("A2dpCodecConfigAptxAdaptive");
  if (A2DP_IsCodecEnabledInOffload(BTAV_A2DP_CODEC_INDEX_SOURCE_APTX_ADAPTIVE)) {
    log::debug("BTAV_A2DP_CODEC_INDEX_SOURCE_APTX_ADAPTIVE");
    if (A2DP_Get_Aptx_AdaptiveR2_2_Supported()) {
      log::debug("A2DP_Get_Aptx_AdaptiveR2_2_Supported");
      a2dp_aptx_adaptive_caps = a2dp_aptx_adaptive_r2_2_offload_caps;
      a2dp_aptx_adaptive_default_config = a2dp_aptx_adaptive_r2_2_default_offload_config;
    } else if (A2DP_Get_Aptx_AdaptiveR2_1_Supported()) {
      log::debug("A2DP_Get_Aptx_AdaptiveR2_1_Supported");
      a2dp_aptx_adaptive_caps = a2dp_aptx_adaptive_r2_1_offload_caps;
      a2dp_aptx_adaptive_default_config = a2dp_aptx_adaptive_r2_1_default_offload_config;
      if (A2DP_Get_Source_Aptx_Adaptive_SplitTx_Supported()) {
        log::debug("A2DP_Get_Source_Aptx_Adaptive_SplitTx_Supported");
        a2dp_aptx_adaptive_caps.aptx_data.aptx_adaptive_sup_features |=
                A2DP_APTX_ADAPTIVE_SOURCE_SPILT_TX_SUPPORTED;
        a2dp_aptx_adaptive_default_config.aptx_data.aptx_adaptive_sup_features |=
                A2DP_APTX_ADAPTIVE_SOURCE_SPILT_TX_SUPPORTED;
      }
    } else {
      if (/*getOffloadCaps().find("aptxadaptiver2") == std::string::npos*/ false) {
        a2dp_aptx_adaptive_caps = a2dp_aptx_adaptive_r1_offload_caps;
        a2dp_aptx_adaptive_default_config = a2dp_aptx_adaptive_r1_default_offload_config;
      } else {
        a2dp_aptx_adaptive_caps = a2dp_aptx_adaptive_offload_caps;
        a2dp_aptx_adaptive_default_config = a2dp_aptx_adaptive_default_offload_config;
      }
    }
  } else {
    log::debug("Codec is not APTX_ADAPTIVE");
    a2dp_aptx_adaptive_caps = a2dp_aptx_adaptive_src_caps;
    a2dp_aptx_adaptive_default_config = a2dp_aptx_adaptive_default_src_config;
  }
  if (a2dp_aptx_adaptive_caps.sampleRate & A2DP_APTX_ADAPTIVE_SAMPLERATE_44100) {
    codec_local_capability_.sample_rate |= BTAV_A2DP_CODEC_SAMPLE_RATE_44100;
  }
  if (a2dp_aptx_adaptive_caps.sampleRate & A2DP_APTX_ADAPTIVE_SAMPLERATE_48000) {
    codec_local_capability_.sample_rate |= BTAV_A2DP_CODEC_SAMPLE_RATE_48000;
  }
  if (a2dp_aptx_adaptive_caps.sampleRate & A2DP_APTX_ADAPTIVE_SAMPLERATE_96000) {
    codec_local_capability_.sample_rate |= BTAV_A2DP_CODEC_SAMPLE_RATE_96000;
  }
  codec_local_capability_.bits_per_sample = a2dp_aptx_adaptive_caps.bits_per_sample;
  if (a2dp_aptx_adaptive_caps.channelMode & A2DP_APTX_ADAPTIVE_CHANNELS_MONO) {
    codec_local_capability_.channel_mode |= BTAV_A2DP_CODEC_CHANNEL_MODE_MONO;
  }
  if (a2dp_aptx_adaptive_caps.channelMode & A2DP_APTX_ADAPTIVE_CHANNELS_TWS_MONO) {
    codec_local_capability_.channel_mode |= BTAV_A2DP_CODEC_CHANNEL_MODE_MONO;
  }
  if (a2dp_aptx_adaptive_caps.channelMode & A2DP_APTX_ADAPTIVE_CHANNELS_STEREO) {
    codec_local_capability_.channel_mode |= BTAV_A2DP_CODEC_CHANNEL_MODE_STEREO;
  }

  if (a2dp_aptx_adaptive_caps.channelMode & A2DP_APTX_ADAPTIVE_CHANNELS_JOINT_STEREO) {
    codec_local_capability_.channel_mode |= BTAV_A2DP_CODEC_CHANNEL_MODE_STEREO;
  }
  if (a2dp_aptx_adaptive_caps.channelMode & A2DP_APTX_ADAPTIVE_CHANNELS_TWS_STEREO) {
    codec_local_capability_.channel_mode |= BTAV_A2DP_CODEC_CHANNEL_MODE_STEREO;
  }
  log::debug("A2dpCodecConfigAptxAdaptive completed");
}

A2dpCodecConfigAptxAdaptive::~A2dpCodecConfigAptxAdaptive() {}

bool A2dpCodecConfigAptxAdaptive::init() {
  if (true /*A2DP_IsCodecEnabledInOffload(BTAV_A2DP_CODEC_INDEX_SOURCE_APTX_ADAPTIVE)*/) {
    return true;
  } /*else if(!A2DP_IsCodecEnabledInSoftware(BTAV_A2DP_CODEC_INDEX_SOURCE_APTX_ADAPTIVE)){
    LOG_DEBUG(LOG_TAG, "%s: APTX-Adaptive disabled in both SW and HW mode", __func__);
    return false;
  } else {
    LOG_DEBUG(LOG_TAG, "%s: APTX-Adaptive enabled in SW mode", __func__);
  }*/

  // Load the encoder
  if (!A2DP_VendorLoadEncoderAptxAdaptive()) {
    return false;
  }

  return true;
}

bool A2dpCodecConfigAptxAdaptive::useRtpHeaderMarkerBit() const { return false; }

//
// Selects the best sample rate from |sampleRate|.
// The result is stored in |p_result| and p_codec_config|.
// Returns true if a selection was made, otherwise false.
//
static bool select_best_sample_rate(uint8_t sampleRate, tA2DP_APTX_ADAPTIVE_CIE* p_result,
                                    btav_a2dp_codec_config_t* p_codec_config) {
  if (sampleRate & A2DP_APTX_ADAPTIVE_SAMPLERATE_44100) {
    p_result->sampleRate = A2DP_APTX_ADAPTIVE_SAMPLERATE_44100;
    p_codec_config->sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_44100;
    return true;
  }
  if (sampleRate & A2DP_APTX_ADAPTIVE_SAMPLERATE_48000) {
    p_result->sampleRate = A2DP_APTX_ADAPTIVE_SAMPLERATE_48000;
    p_codec_config->sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_48000;
    return true;
  }
  if (sampleRate & A2DP_APTX_ADAPTIVE_SAMPLERATE_96000) {
    p_result->sampleRate = A2DP_APTX_ADAPTIVE_SAMPLERATE_96000;
    p_codec_config->sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_96000;
    return true;
  }
  return false;
}

//
// Selects the audio sample rate from |p_codec_audio_config|.
// |sampleRate| contains the capability.
// The result is stored in |p_result| and |p_codec_config|.
// Returns true if a selection was made, otherwise false.
//
static bool select_audio_sample_rate(const btav_a2dp_codec_config_t* p_codec_audio_config,
                                     uint8_t sampleRate, tA2DP_APTX_ADAPTIVE_CIE* p_result,
                                     btav_a2dp_codec_config_t* p_codec_config) {
  switch (p_codec_audio_config->sample_rate) {
    case BTAV_A2DP_CODEC_SAMPLE_RATE_44100:
      if (sampleRate & A2DP_APTX_ADAPTIVE_SAMPLERATE_44100) {
        p_result->sampleRate = A2DP_APTX_ADAPTIVE_SAMPLERATE_44100;
        p_codec_config->sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_44100;
        return true;
      }
      break;
    case BTAV_A2DP_CODEC_SAMPLE_RATE_48000:
      if (sampleRate & A2DP_APTX_ADAPTIVE_SAMPLERATE_48000) {
        p_result->sampleRate = A2DP_APTX_ADAPTIVE_SAMPLERATE_48000;
        p_codec_config->sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_48000;
        return true;
      }
      break;
    case BTAV_A2DP_CODEC_SAMPLE_RATE_96000:
      if (sampleRate & A2DP_APTX_ADAPTIVE_SAMPLERATE_96000) {
        p_result->sampleRate = A2DP_APTX_ADAPTIVE_SAMPLERATE_96000;
        p_codec_config->sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_96000;
        return true;
      }
      break;
    case BTAV_A2DP_CODEC_SAMPLE_RATE_88200:
    case BTAV_A2DP_CODEC_SAMPLE_RATE_176400:
    case BTAV_A2DP_CODEC_SAMPLE_RATE_192000:
    case BTAV_A2DP_CODEC_SAMPLE_RATE_NONE:
    default:
      break;
  }
  return false;
}

//
// Selects the best bits per sample.
// The result is stored in |p_codec_config|.
// Returns true if a selection was made, otherwise false.
//
static bool select_best_bits_per_sample(btav_a2dp_codec_config_t* p_codec_config) {
  p_codec_config->bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24;
  return true;
}

//
// Selects the audio bits per sample from |p_codec_audio_config|.
// The result is stored in |p_codec_config|.
// Returns true if a selection was made, otherwise false.
//
static bool select_audio_bits_per_sample(const btav_a2dp_codec_config_t* p_codec_audio_config,
                                         btav_a2dp_codec_config_t* p_codec_config) {
  switch (p_codec_audio_config->bits_per_sample) {
    case BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24:
      p_codec_config->bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24;
      return true;
    case BTAV_A2DP_CODEC_BITS_PER_SAMPLE_32:
      p_codec_config->bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_32;
      return true;
    case BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16:
      p_codec_config->bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16;
      return true;
    case BTAV_A2DP_CODEC_BITS_PER_SAMPLE_NONE:
      break;
  }
  return false;
}

//
// Selects the best channel mode from |channelMode|.
// The result is stored in |p_result| and |p_codec_config|.
// Returns true if a selection was made, otherwise false.
//
static bool select_best_channel_mode(uint8_t channelMode, tA2DP_APTX_ADAPTIVE_CIE* p_result,
                                     btav_a2dp_codec_config_t* p_codec_config) {
  if (channelMode & A2DP_APTX_ADAPTIVE_CHANNELS_TWS_PLUS) {
    p_result->channelMode = A2DP_APTX_ADAPTIVE_CHANNELS_TWS_PLUS;
    p_codec_config->channel_mode = BTAV_A2DP_CODEC_CHANNEL_MODE_STEREO;
    return true;
  }

  if (channelMode & A2DP_APTX_ADAPTIVE_CHANNELS_STEREO) {
    p_result->channelMode = A2DP_APTX_ADAPTIVE_CHANNELS_STEREO;
    p_codec_config->channel_mode = BTAV_A2DP_CODEC_CHANNEL_MODE_STEREO;
    return true;
  }

  if (channelMode & A2DP_APTX_ADAPTIVE_CHANNELS_TWS_STEREO) {
    p_result->channelMode = A2DP_APTX_ADAPTIVE_CHANNELS_TWS_STEREO;
    p_codec_config->channel_mode = BTAV_A2DP_CODEC_CHANNEL_MODE_STEREO;
    return true;
  }

  if (channelMode & A2DP_APTX_ADAPTIVE_CHANNELS_JOINT_STEREO) {
    p_result->channelMode = A2DP_APTX_ADAPTIVE_CHANNELS_JOINT_STEREO;
    p_codec_config->channel_mode = BTAV_A2DP_CODEC_CHANNEL_MODE_STEREO;
    return true;
  }

  if (channelMode & A2DP_APTX_ADAPTIVE_CHANNELS_MONO) {
    p_result->channelMode = A2DP_APTX_ADAPTIVE_CHANNELS_MONO;
    p_codec_config->channel_mode = BTAV_A2DP_CODEC_CHANNEL_MODE_MONO;
    return true;
  }

  if (channelMode & A2DP_APTX_ADAPTIVE_CHANNELS_TWS_MONO) {
    p_result->channelMode = A2DP_APTX_ADAPTIVE_CHANNELS_TWS_MONO;
    p_codec_config->channel_mode = BTAV_A2DP_CODEC_CHANNEL_MODE_MONO;
    return true;
  }
  return false;
}

//
// Selects the audio channel mode from |p_codec_audio_config|.
// |channelMode| contains the capability.
// The result is stored in |p_result| and |p_codec_config|.
// Returns true if a selection was made, otherwise false.
//
static bool select_audio_channel_mode(const btav_a2dp_codec_config_t* p_codec_audio_config,
                                      uint8_t channelMode, tA2DP_APTX_ADAPTIVE_CIE* p_result,
                                      btav_a2dp_codec_config_t* p_codec_config) {
  switch (p_codec_audio_config->channel_mode) {
    case BTAV_A2DP_CODEC_CHANNEL_MODE_MONO:
      if (channelMode & A2DP_APTX_ADAPTIVE_CHANNELS_MONO) {
        p_result->channelMode = A2DP_APTX_ADAPTIVE_CHANNELS_MONO;
        p_codec_config->channel_mode = BTAV_A2DP_CODEC_CHANNEL_MODE_MONO;
        return true;
      }
      if (channelMode & A2DP_APTX_ADAPTIVE_CHANNELS_TWS_MONO) {
        p_result->channelMode = A2DP_APTX_ADAPTIVE_CHANNELS_TWS_MONO;
        p_codec_config->channel_mode = BTAV_A2DP_CODEC_CHANNEL_MODE_MONO;
        return true;
      }
      break;
    case BTAV_A2DP_CODEC_CHANNEL_MODE_STEREO:
      if (channelMode & A2DP_APTX_ADAPTIVE_CHANNELS_TWS_PLUS) {
        p_result->channelMode = A2DP_APTX_ADAPTIVE_CHANNELS_TWS_PLUS;
        p_codec_config->channel_mode = BTAV_A2DP_CODEC_CHANNEL_MODE_STEREO;
        return true;
      }
      if (channelMode & A2DP_APTX_ADAPTIVE_CHANNELS_STEREO) {
        p_result->channelMode = A2DP_APTX_ADAPTIVE_CHANNELS_STEREO;
        p_codec_config->channel_mode = BTAV_A2DP_CODEC_CHANNEL_MODE_STEREO;
        return true;
      }
      if (channelMode & A2DP_APTX_ADAPTIVE_CHANNELS_JOINT_STEREO) {
        p_result->channelMode = A2DP_APTX_ADAPTIVE_CHANNELS_JOINT_STEREO;
        p_codec_config->channel_mode = BTAV_A2DP_CODEC_CHANNEL_MODE_STEREO;
        return true;
      }
      if (channelMode & A2DP_APTX_ADAPTIVE_CHANNELS_TWS_STEREO) {
        p_result->channelMode = A2DP_APTX_ADAPTIVE_CHANNELS_TWS_STEREO;
        p_codec_config->channel_mode = BTAV_A2DP_CODEC_CHANNEL_MODE_STEREO;
        return true;
      }
      break;
    case BTAV_A2DP_CODEC_CHANNEL_MODE_NONE:
      break;
  }

  return false;
}

tA2DP_STATUS A2dpCodecConfigAptxAdaptive::setCodecConfig(const uint8_t* p_peer_codec_info,
                                                 bool is_capability,
                                                 uint8_t* p_result_codec_config) {
  std::lock_guard<std::recursive_mutex> lock(codec_mutex_);
  tA2DP_APTX_ADAPTIVE_CIE sink_info_cie;
  tA2DP_APTX_ADAPTIVE_CIE result_config_cie;
  uint8_t sampleRate;
  uint8_t channelMode;

  uint8_t sink_byte_17th = 0x00, src_byte_17th = 0x00, byte_negotiated_17th = 0x00;
  // Save the internal state
  btav_a2dp_codec_config_t saved_codec_config = codec_config_;
  btav_a2dp_codec_config_t saved_codec_selectable_capability = codec_selectable_capability_;
  btav_a2dp_codec_config_t saved_codec_user_config = codec_user_config_;
  btav_a2dp_codec_config_t saved_codec_audio_config = codec_audio_config_;
  bluetooth::a2dp::MediaCodecCapabilities saved_ota_codec_config = ota_codec_config_;
  bluetooth::a2dp::MediaCodecCapabilities saved_ota_codec_peer_capability =
          ota_codec_peer_capability_;
  bluetooth::a2dp::MediaCodecCapabilities saved_ota_codec_peer_config = ota_codec_peer_config_;

  tA2DP_STATUS status =
          A2DP_ParseInfoAptxAdaptive(&sink_info_cie, p_peer_codec_info, is_capability);
  if (status != A2DP_SUCCESS) {
    log::error("Can't parse peer's Sink capabilities: error = {}", status);
    goto fail;
  }

  // Build the preferred configuration
  memset(&result_config_cie, 0, sizeof(result_config_cie));
  result_config_cie.vendorId = a2dp_aptx_adaptive_caps.vendorId;
  result_config_cie.codecId = a2dp_aptx_adaptive_caps.codecId;

  log::info("Sink additional supported features: 0x{:4x}",
            sink_info_cie.aptx_data.aptx_adaptive_sup_features);
  log::info("Sink cap ext ver num: 0x{:x}", sink_info_cie.aptx_data.cap_ext_ver_num);

  if (/*(getOffloadCaps().find("aptxadaptiver2") == std::string::npos)*/ false ||
      (sink_info_cie.aptx_data.cap_ext_ver_num == 0)) {
    result_config_cie.aptx_data = a2dp_aptx_adaptive_r1_offload_caps.aptx_data;
    if (sink_info_cie.aptx_data.cap_ext_ver_num == 0) {
      log::info("Sink supports R1.0 decoder");
      result_config_cie.aptx_data = sink_info_cie.aptx_data;
    }
    log::info("Select Aptx Adaptive R1 config");
  } else {
    sink_byte_17th = (sink_info_cie.aptx_data.aptx_adaptive_sup_features & 0xFF);
    src_byte_17th = (a2dp_aptx_adaptive_caps.aptx_data.aptx_adaptive_sup_features & 0xFF);
    byte_negotiated_17th = (((sink_byte_17th >> 4) | (src_byte_17th >> 4)) << 4) |
                           ((sink_byte_17th & src_byte_17th) & 0x0F);
    log::info("Sink byte: 0x{:x}, src byte: 0x{:x}, byte negotiated: 0x{:x}",
                       sink_byte_17th, src_byte_17th, byte_negotiated_17th);
    if (A2DP_Get_Aptx_AdaptiveR2_2_Supported()) {
      log::info("Select Aptx Adaptive R2.2 config");
      result_config_cie.aptx_data = a2dp_aptx_adaptive_r2_2_offload_caps.aptx_data;
      if ((sink_info_cie.aptx_data.aptx_adaptive_sup_features &
           APTX_ADAPTIVE_SINK_R2_2_SUPPORT_CAP) &&
          (sink_info_cie.aptx_data.cap_ext_ver_num == A2DP_APTX_ADAPTIVE_CAP_EXT_VER_NUM)) {
        log::info("Sink supports R2.2 decoder");
        result_config_cie.aptx_data.aptx_adaptive_sup_features =
                (sink_info_cie.aptx_data.aptx_adaptive_sup_features & 0xFFFFFF00) |
                byte_negotiated_17th;
        a2dp_aptx_adaptive_caps.sampleRate |= A2DP_APTX_ADAPTIVE_SAMPLERATE_44100;
        a2dp_aptx_adaptive_default_config.sampleRate = A2DP_APTX_ADAPTIVE_SAMPLERATE_44100;
        codec_config_.codec_specific_3 &= ~(int64_t)APTX_ADAPTIVE_R2_2_SUPPORT_MASK;
        codec_config_.codec_specific_3 |= (int64_t)APTX_ADAPTIVE_R2_2_SUPPORT_AVAILABLE;
      } else {
        log::info("Sink doesn't support R2.2 decoder, limit local sample rate caps");
        log::info("Sink supports R2.x decoder");
        a2dp_aptx_adaptive_caps.sampleRate &= ~A2DP_APTX_ADAPTIVE_SAMPLERATE_44100;
        a2dp_aptx_adaptive_default_config.sampleRate = A2DP_APTX_ADAPTIVE_SAMPLERATE_48000;
        result_config_cie.aptx_data.aptx_adaptive_sup_features =
                (sink_info_cie.aptx_data.aptx_adaptive_sup_features & 0xFFFFFF00) |
                byte_negotiated_17th;
        codec_config_.codec_specific_3 &= ~(int64_t)APTX_ADAPTIVE_R2_2_SUPPORT_MASK;
        codec_config_.codec_specific_3 |= (int64_t)APTX_ADAPTIVE_R2_2_SUPPORT_NOT_AVAILABLE;
      }
    }else if (A2DP_Get_Aptx_AdaptiveR2_1_Supported()) {
      log::info("Select Aptx Adaptive R2.1 config");
      result_config_cie.aptx_data = a2dp_aptx_adaptive_r2_1_offload_caps.aptx_data;
      result_config_cie.aptx_data.aptx_adaptive_sup_features =
              (sink_info_cie.aptx_data.aptx_adaptive_sup_features & 0xFFFFFF00) |
              byte_negotiated_17th;
    } else {
      log::info("Select Aptx Adaptive R2 config");
      result_config_cie.aptx_data = a2dp_aptx_adaptive_offload_caps.aptx_data;
      result_config_cie.aptx_data.aptx_adaptive_sup_features =
              (sink_info_cie.aptx_data.aptx_adaptive_sup_features & 0xFFFFFF00) |
              byte_negotiated_17th;
    }
    log::info("res cfg subfeat 0x{:4x}",result_config_cie.aptx_data.aptx_adaptive_sup_features);
  }

  // Select the sample frequency
  sampleRate = a2dp_aptx_adaptive_caps.sampleRate & sink_info_cie.sampleRate;
  log::debug("Sample rate: source caps = 0x{:x} "
              "sink info = 0x{:x}",
              a2dp_aptx_adaptive_caps.sampleRate,
              sink_info_cie.sampleRate);
  codec_config_.sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_NONE;
  switch (codec_user_config_.sample_rate) {
    case BTAV_A2DP_CODEC_SAMPLE_RATE_44100:
      if (sampleRate & A2DP_APTX_ADAPTIVE_SAMPLERATE_44100) {
        result_config_cie.sampleRate = A2DP_APTX_ADAPTIVE_SAMPLERATE_44100;
        codec_config_.sample_rate = codec_user_config_.sample_rate;
      }
      break;
    case BTAV_A2DP_CODEC_SAMPLE_RATE_48000:
      if (sampleRate & A2DP_APTX_ADAPTIVE_SAMPLERATE_48000) {
        result_config_cie.sampleRate = A2DP_APTX_ADAPTIVE_SAMPLERATE_48000;
        codec_config_.sample_rate = codec_user_config_.sample_rate;
      }
      break;
    case BTAV_A2DP_CODEC_SAMPLE_RATE_96000:
      if (sampleRate & A2DP_APTX_ADAPTIVE_SAMPLERATE_96000) {
        result_config_cie.sampleRate = A2DP_APTX_ADAPTIVE_SAMPLERATE_96000;
        codec_config_.sample_rate = codec_user_config_.sample_rate;
      }
      break;
    case BTAV_A2DP_CODEC_SAMPLE_RATE_88200:
    case BTAV_A2DP_CODEC_SAMPLE_RATE_176400:
    case BTAV_A2DP_CODEC_SAMPLE_RATE_192000:
    case BTAV_A2DP_CODEC_SAMPLE_RATE_NONE:
    default:
      codec_config_.sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_NONE;
      break;
  }

  // Select the sample frequency if there is no user preference
  do {
    // Compute the selectable capability
    codec_selectable_capability_.sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_NONE;
    if (sampleRate & A2DP_APTX_ADAPTIVE_SAMPLERATE_44100) {
      codec_selectable_capability_.sample_rate |= BTAV_A2DP_CODEC_SAMPLE_RATE_44100;
    }
    if (sampleRate & A2DP_APTX_ADAPTIVE_SAMPLERATE_48000) {
      codec_selectable_capability_.sample_rate |= BTAV_A2DP_CODEC_SAMPLE_RATE_48000;
    }
    if (sampleRate & A2DP_APTX_ADAPTIVE_SAMPLERATE_96000) {
      codec_selectable_capability_.sample_rate |= BTAV_A2DP_CODEC_SAMPLE_RATE_96000;
    }

    if (codec_config_.sample_rate != BTAV_A2DP_CODEC_SAMPLE_RATE_NONE) {
      break;
    }

    // Compute the common capability
    if (sampleRate & A2DP_APTX_ADAPTIVE_SAMPLERATE_44100) {
    }
    if (sampleRate & A2DP_APTX_ADAPTIVE_SAMPLERATE_48000) {
    }
    if (sampleRate & A2DP_APTX_ADAPTIVE_SAMPLERATE_96000) {
    }

    // No user preference - try the codec audio config
    if (select_audio_sample_rate(&codec_audio_config_, sampleRate, &result_config_cie,
                                 &codec_config_)) {
      break;
    }

    // No user preference - try the default config
    if (select_best_sample_rate(
                a2dp_aptx_adaptive_default_config.sampleRate & sink_info_cie.sampleRate,
                &result_config_cie, &codec_config_)) {
      break;
    }

    // No user preference - use the best match
    if (select_best_sample_rate(sampleRate, &result_config_cie, &codec_config_)) {
      break;
    }
  } while (false);
  if (codec_config_.sample_rate == BTAV_A2DP_CODEC_SAMPLE_RATE_NONE) {
    log::error("Cannot match sample frequency: source caps = 0x{:x} "
              "sink info = 0x{:x}",
              a2dp_aptx_adaptive_caps.sampleRate, sink_info_cie.sampleRate);
    goto fail;
  }

  // Select the bits per sample
  // NOTE: this information is NOT included in the aptX-adaptive A2DP codec
  // description that is sent OTA.
  codec_config_.bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_NONE;
  switch (codec_user_config_.bits_per_sample) {
    case BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16:
      codec_config_.bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_NONE;
      break;

    case BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24:
      codec_config_.bits_per_sample = codec_user_config_.bits_per_sample;
      break;
    case BTAV_A2DP_CODEC_BITS_PER_SAMPLE_32:

    case BTAV_A2DP_CODEC_BITS_PER_SAMPLE_NONE:
      codec_config_.bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_NONE;
      break;
  }
  // Select the bits per sample if there is no user preference
  do {
    // Compute the selectable capability
    codec_selectable_capability_.bits_per_sample = a2dp_aptx_adaptive_caps.bits_per_sample;

    if (codec_config_.bits_per_sample != BTAV_A2DP_CODEC_BITS_PER_SAMPLE_NONE) {
      break;
    }

    // Compute the common capability

    // No user preference - try the codec audio config
    if (select_audio_bits_per_sample(&codec_audio_config_, &codec_config_)) {
      break;
    }

    // No user preference - try the default config
    if (select_best_bits_per_sample(&codec_config_)) {
      break;
    }

    // No user preference - use the best match
    // NOTE: no-op - kept here for consistency
    if (select_best_bits_per_sample(&codec_config_)) {
      break;
    }
  } while (false);
  if (codec_config_.bits_per_sample == BTAV_A2DP_CODEC_BITS_PER_SAMPLE_NONE) {
    log::error("Cannot match bits per sample: user preference = 0x{:4x}",
                codec_user_config_.bits_per_sample);
    goto fail;
  }

  // Select the channel mode
  channelMode = a2dp_aptx_adaptive_caps.channelMode & sink_info_cie.channelMode;
  codec_config_.channel_mode = BTAV_A2DP_CODEC_CHANNEL_MODE_NONE;
  log::error("Codec_user_config_.channel_mode: {:x} channelMode: {:x}",
                  codec_user_config_.channel_mode, channelMode);
  switch (codec_user_config_.channel_mode) {
    case BTAV_A2DP_CODEC_CHANNEL_MODE_MONO:
      if (channelMode & A2DP_APTX_ADAPTIVE_CHANNELS_MONO) {
        result_config_cie.channelMode = A2DP_APTX_ADAPTIVE_CHANNELS_MONO;
        codec_config_.channel_mode = codec_user_config_.channel_mode;
        break;
      }
      if (channelMode & A2DP_APTX_ADAPTIVE_CHANNELS_TWS_MONO) {
        result_config_cie.channelMode = A2DP_APTX_ADAPTIVE_CHANNELS_MONO;
        codec_config_.channel_mode = codec_user_config_.channel_mode;
        break;
      }
      break;
    case BTAV_A2DP_CODEC_CHANNEL_MODE_STEREO:
      if (channelMode & A2DP_APTX_ADAPTIVE_CHANNELS_TWS_PLUS) {
        result_config_cie.channelMode = A2DP_APTX_ADAPTIVE_CHANNELS_TWS_PLUS;
        codec_config_.channel_mode = codec_user_config_.channel_mode;
        break;
      }
      if (channelMode & A2DP_APTX_ADAPTIVE_CHANNELS_STEREO) {
        result_config_cie.channelMode = A2DP_APTX_ADAPTIVE_CHANNELS_STEREO;
        codec_config_.channel_mode = codec_user_config_.channel_mode;
        break;
      }
      if (channelMode & A2DP_APTX_ADAPTIVE_CHANNELS_JOINT_STEREO) {
        result_config_cie.channelMode = A2DP_APTX_ADAPTIVE_CHANNELS_JOINT_STEREO;
        codec_config_.channel_mode = codec_user_config_.channel_mode;
        break;
      }
      if (channelMode & A2DP_APTX_ADAPTIVE_CHANNELS_TWS_STEREO) {
        result_config_cie.channelMode = A2DP_APTX_ADAPTIVE_CHANNELS_TWS_STEREO;
        codec_config_.channel_mode = codec_user_config_.channel_mode;
        break;
      }
      break;
    case BTAV_A2DP_CODEC_CHANNEL_MODE_NONE:
      codec_config_.channel_mode = BTAV_A2DP_CODEC_CHANNEL_MODE_NONE;
      break;
  }
  // Select the channel mode if there is no user preference
  do {
    // Compute the selectable capability
    if (channelMode & A2DP_APTX_ADAPTIVE_CHANNELS_MONO) {
      codec_selectable_capability_.channel_mode |= BTAV_A2DP_CODEC_CHANNEL_MODE_MONO;
    }
    if (channelMode & A2DP_APTX_ADAPTIVE_CHANNELS_TWS_MONO) {
      codec_selectable_capability_.channel_mode |= BTAV_A2DP_CODEC_CHANNEL_MODE_MONO;
    }
    if (channelMode & A2DP_APTX_ADAPTIVE_CHANNELS_JOINT_STEREO) {
      codec_selectable_capability_.channel_mode |= BTAV_A2DP_CODEC_CHANNEL_MODE_STEREO;
    }
    if (channelMode & A2DP_APTX_ADAPTIVE_CHANNELS_TWS_STEREO) {
      codec_selectable_capability_.channel_mode |= BTAV_A2DP_CODEC_CHANNEL_MODE_STEREO;
    }
    if (channelMode & A2DP_APTX_ADAPTIVE_CHANNELS_STEREO) {
      codec_selectable_capability_.channel_mode |= BTAV_A2DP_CODEC_CHANNEL_MODE_STEREO;
    }
    if (channelMode & A2DP_APTX_ADAPTIVE_CHANNELS_TWS_PLUS) {
      codec_selectable_capability_.channel_mode |= BTAV_A2DP_CODEC_CHANNEL_MODE_STEREO;
    }

    if (codec_config_.channel_mode != BTAV_A2DP_CODEC_CHANNEL_MODE_NONE) {
      break;
    }

    // Compute the common capability
    if (channelMode & A2DP_APTX_ADAPTIVE_CHANNELS_MONO) {
    }
    if (channelMode & A2DP_APTX_ADAPTIVE_CHANNELS_TWS_MONO) {
    }
    if (channelMode & A2DP_APTX_ADAPTIVE_CHANNELS_STEREO) {
    }
    if (channelMode & A2DP_APTX_ADAPTIVE_CHANNELS_JOINT_STEREO) {
    }
    if (channelMode & A2DP_APTX_ADAPTIVE_CHANNELS_TWS_STEREO) {
    }
    if (channelMode & A2DP_APTX_ADAPTIVE_CHANNELS_TWS_PLUS) {
    }

    // No user preference - try the codec audio config
    if (select_audio_channel_mode(&codec_audio_config_, channelMode, &result_config_cie,
                                  &codec_config_)) {
      break;
    }

    // No user preference - try the default config
    if (select_best_channel_mode(
                a2dp_aptx_adaptive_default_config.channelMode & sink_info_cie.channelMode,
                &result_config_cie, &codec_config_)) {
      break;
    }

    // No user preference - use the best match
    if (select_best_channel_mode(channelMode, &result_config_cie, &codec_config_)) {
      break;
    }
  } while (false);
  if (codec_config_.channel_mode == BTAV_A2DP_CODEC_CHANNEL_MODE_NONE) {
    log::error("Cannot match channel mode: source caps = 0x{:X} "
              "sink info = 0x{:x}", a2dp_aptx_adaptive_caps.channelMode,
              sink_info_cie.channelMode);
    goto fail;
  }

  result_config_cie.sourceType = a2dp_aptx_adaptive_caps.sourceType;

  memset(result_config_cie.reserved_data, 0, sizeof(result_config_cie.reserved_data));

  if (A2DP_BuildInfoAptxAdaptive(AVDT_MEDIA_TYPE_AUDIO, &result_config_cie,
                                 p_result_codec_config) != A2DP_SUCCESS) {
    log::error("Channel mode: source caps = 0x{:X}"
              "sink info = 0x{:X}", a2dp_aptx_adaptive_caps.channelMode,
              sink_info_cie.channelMode);
    goto fail;
  }

  if (codec_user_config_.codec_specific_2 != codec_config_.codec_specific_2) {
    codec_config_.codec_specific_2 = codec_user_config_.codec_specific_2;
  }
  if (codec_user_config_.codec_specific_3 != codec_config_.codec_specific_3) {
    codec_user_config_.codec_specific_3 = codec_config_.codec_specific_3;
  }

  log::error("Setting codec_config_.codec_specific_4 to channelMode: {:x}", channelMode);
  // Store the channel mode in spare field
  codec_config_.codec_specific_4 &= (int64_t)CHANNEL_MODE_BACK_CHANNEL_MASK;
  codec_config_.codec_specific_4 |= (int64_t)channelMode << 24;

  // Create a local copy of the peer codec capability/config, and the
  // result codec config.
  if (is_capability) {
    status = A2DP_BuildInfoAptxAdaptive(AVDT_MEDIA_TYPE_AUDIO, &sink_info_cie,
                                        ota_codec_peer_capability_.data());
  } else {
    status = A2DP_BuildInfoAptxAdaptive(AVDT_MEDIA_TYPE_AUDIO, &sink_info_cie,
                                        ota_codec_peer_config_.data());
  }
  CHECK(status == A2DP_SUCCESS);
  status = A2DP_BuildInfoAptxAdaptive(AVDT_MEDIA_TYPE_AUDIO, &result_config_cie, ota_codec_config_.data());
  CHECK(status == A2DP_SUCCESS);
  return A2DP_SUCCESS;

fail:
  // Restore the internal state
  codec_config_ = saved_codec_config;
  codec_selectable_capability_ = saved_codec_selectable_capability;
  codec_user_config_ = saved_codec_user_config;
  codec_audio_config_ = saved_codec_audio_config;
  ota_codec_config_ = saved_ota_codec_config;
  ota_codec_peer_capability_ = saved_ota_codec_peer_capability;
  ota_codec_peer_config_ = saved_ota_codec_peer_config;
  return status;
}

bool A2dpCodecConfigAptxAdaptive::setPeerCodecCapabilities(const uint8_t* p_peer_codec_cap) {
  std::lock_guard<std::recursive_mutex> lock(codec_mutex_);
  tA2DP_APTX_ADAPTIVE_CIE sink_info_cie;
  uint8_t channelMode = BTAV_A2DP_CODEC_CHANNEL_MODE_NONE;
  uint8_t sampleRate = BTAV_A2DP_CODEC_SAMPLE_RATE_NONE;

  const tA2DP_APTX_ADAPTIVE_CIE* p_saved_aptx_ad_caps = &a2dp_aptx_adaptive_caps;

  btav_a2dp_codec_config_t saved_codec_selectable_capability = codec_selectable_capability_;
  bluetooth::a2dp::MediaCodecCapabilities saved_ota_codec_peer_capability =
          ota_codec_peer_capability_;

  tA2DP_STATUS status = A2DP_ParseInfoAptxAdaptive(&sink_info_cie, p_peer_codec_cap, true);
  if (status != A2DP_SUCCESS) {
    log::error("failed to parse remote capability: error = {}", status);
    goto fail;
  }

  // Compute the selectable Sampling Rate
  sampleRate = p_saved_aptx_ad_caps->sampleRate & sink_info_cie.sampleRate;
  if (sampleRate & A2DP_APTX_ADAPTIVE_SAMPLERATE_44100) {
    codec_selectable_capability_.sample_rate |= BTAV_A2DP_CODEC_SAMPLE_RATE_44100;
  }

  if (sampleRate & A2DP_APTX_ADAPTIVE_SAMPLERATE_48000) {
    codec_selectable_capability_.sample_rate |= BTAV_A2DP_CODEC_SAMPLE_RATE_48000;
  }

  if (sampleRate & A2DP_APTX_ADAPTIVE_SAMPLERATE_96000) {
    codec_selectable_capability_.sample_rate |= BTAV_A2DP_CODEC_SAMPLE_RATE_96000;
  }

  // Compute the selectable bps
  codec_selectable_capability_.bits_per_sample = p_saved_aptx_ad_caps->bits_per_sample;

  // Compute the selectable channel mode
  channelMode = p_saved_aptx_ad_caps->channelMode & sink_info_cie.channelMode;
  if (channelMode & A2DP_APTX_ADAPTIVE_CHANNELS_MONO) {
    codec_selectable_capability_.channel_mode |= BTAV_A2DP_CODEC_CHANNEL_MODE_MONO;
  }

  if (channelMode & A2DP_APTX_ADAPTIVE_CHANNELS_STEREO) {
    codec_selectable_capability_.channel_mode |= BTAV_A2DP_CODEC_CHANNEL_MODE_STEREO;
  }

  if (channelMode & A2DP_APTX_ADAPTIVE_CHANNELS_JOINT_STEREO) {
    codec_selectable_capability_.channel_mode |= BTAV_A2DP_CODEC_CHANNEL_MODE_STEREO;
  }

  status = A2DP_BuildInfoAptxAdaptive(AVDT_MEDIA_TYPE_AUDIO, &sink_info_cie,
                                      ota_codec_peer_capability_.data());
  CHECK(status == A2DP_SUCCESS);
  return true;

fail:
  codec_selectable_capability_ = saved_codec_selectable_capability;
  ota_codec_peer_capability_ = saved_ota_codec_peer_capability;
  return false;
}
