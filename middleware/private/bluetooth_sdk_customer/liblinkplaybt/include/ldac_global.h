#ifndef __LDAC_GLOBAL_H__
#define __LDAC_GLOBAL_H__

#include "avdt_api.h"
#include <stdbool.h>
#include <stdint.h>

typedef uint32_t (*a2dp_source_read_callback_t)(uint8_t* p_buf, uint32_t len);
// Prototype for a callback to enqueue A2DP Source packets for transmission.
// |p_buf| is the buffer with the audio data to enqueue. The callback is
// responsible for freeing |p_buf|.
// |frames_n| is the number of audio frames in |p_buf| - it is used for
// statistics purpose.
// Returns true if the packet was enqueued, otherwise false.
typedef bool (*a2dp_source_enqueue_callback_t)(BT_HDR* p_buf, size_t frames_n);
typedef struct {
    //btav_a2dp_codec_priority_t codec_priority_;  // Codec priority: must be unique
    //btav_a2dp_codec_priority_t default_codec_priority_;

    //btav_a2dp_codec_config_t codec_config_;
    //btav_a2dp_codec_config_t codec_capability_;
    //btav_a2dp_codec_config_t codec_local_capability_;
    //btav_a2dp_codec_config_t codec_selectable_capability_;

    // The optional user configuration. The values (if set) are used
    // as a preference when there is a choice. If a particular value
    // is not supported by the local or remote device, it is ignored.
    //btav_a2dp_codec_config_t codec_user_config_;

    // The selected audio feeding configuration.
    //btav_a2dp_codec_config_t codec_audio_config_;

    uint8_t ota_codec_config_[AVDT_CODEC_SIZE];
    uint8_t ota_codec_peer_capability_[AVDT_CODEC_SIZE];
    uint8_t ota_codec_peer_config_[AVDT_CODEC_SIZE];
} A2dpCodecConfig;

typedef struct {
  bool is_peer_edr;          // True if the A2DP peer supports EDR
  bool peer_supports_3mbps;  // True if the A2DP peer supports 3 Mbps EDR
  uint16_t peer_mtu;         // MTU of the A2DP peer
} tA2DP_ENCODER_INIT_PEER_PARAMS;



//ldac related MACROs
//function: A2DP_VendorInitCodecConfigLdac
typedef void (*tLDAC_FEEDING_RESET)(uint16_t ms);
typedef bool (*tLDAC_CODEC_CONFIG_INIT)(tAVDT_CFG* p_cfg);
typedef void (*tLDAC_SEND_FRAMES)(uint64_t timestamp_us, uint8_t* src, uint8_t* des);
typedef void (*tLDAC_ENCODER_INIT)(
    const tA2DP_ENCODER_INIT_PEER_PARAMS* p_peer_params,
    A2dpCodecConfig* a2dp_codec_config,
    a2dp_source_read_callback_t read_callback,
    a2dp_source_enqueue_callback_t enqueue_callback);
typedef bool (*tLDAC_CODEC_CONFIG_LDAC_INIT)();

typedef int (*tLDAC_ENCODE_DATA)(uint8_t* src, uint8_t* des, uint8_t* nb_frame);

typedef bool (*tLDAC_BUILD_CODEC_HEADER)(const uint8_t* p_codec_info,
                                     BT_HDR* p_buf,
                                     uint16_t frames_per_packet);
typedef void (*tLDAC_GET_NUM_FRAME_ITER)(uint8_t* num_of_iterations,
                                              uint8_t* num_of_frames,
                                              uint64_t timestamp_us);



#endif
