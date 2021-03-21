#ifdef LINKPLAY
#ifndef __AAC_H__
#define __AAC_H__
#include "AudioConverter.h"
#include "CommonServices.h"
#include "neaacdec.h"

typedef struct aac_remain_buff_s
{
    uint8_t *remainBuff;
    uint8_t remainedNum;
}AAC_REMAIN_BUFF_T;

typedef struct aac_decoder * aac_decoder_ref;
typedef struct aac_decoder
{
    uint8_t aac_dec_inited; /* AAC init flag for each instance */
    faacDecHandle hAac; /* decoder handle */
    //uint32_t sampleRate;
    //uint32_t channels;
    AAC_REMAIN_BUFF_T remainData;
}AAC_DECODER_T;


OSStatus AacDecoder_Create(aac_decoder_ref* outDecoder);
OSStatus AacDecoder_Delete(aac_decoder_ref inDecoder);
OSStatus AacDecoder_SetConfigure(aac_decoder_ref inDecoder, const AACParams *params);
OSStatus AacDecoder_Decode(aac_decoder_ref inDecoder, uint8_t *inBuffPtr, uint32_t inBuffSize, uint8_t *OutBuffPtr, uint32_t *OutBuffSize);

#endif /* __AAC_H__ */
#endif /* LINKPLAY */

