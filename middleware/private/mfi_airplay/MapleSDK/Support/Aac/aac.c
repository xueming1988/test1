#ifdef LINKPLAY
#include <stdlib.h>
#include "DebugServices.h"
#include "aac.h"

ulog_define( AirAacDecoder,	kLogLevelInfo, kLogFlags_Default, "AacDecoder", NULL );
#define aac_ucat()						&log_category_from_name( AirAacDecoder )
#define aac_ulog( LEVEL, ... )			ulog( aac_ucat(), (LEVEL), __VA_ARGS__ )

OSStatus AacDecoder_Create(aac_decoder_ref* outDecoder)
{
    OSStatus err = kNoErr;
    aac_decoder_ref obj;

    obj = (aac_decoder_ref)calloc(1, sizeof(*obj));
    require_action(obj, exit, err = kNoMemoryErr);

    obj->hAac = faacDecOpen();

    require_action(obj, exit, err = kRequestErr);
    
    *outDecoder = obj;
    err = kNoErr;

    aac_ulog(kLogLevelInfo, "AAC Decoder inited at [0x%x].\n", obj);

exit:
    return (err);
}


OSStatus  AacDecoder_Delete(aac_decoder_ref inDecoder)
{
    if (inDecoder)
    {
        aac_ulog(kLogLevelInfo, "AAC Decoder destory at [0x%x].\n", inDecoder);
        
        if (inDecoder->hAac)
        {
            NeAACDecClose(inDecoder->hAac);
        }
        
        if ((inDecoder->remainData.remainedNum > 0) && inDecoder->remainData.remainBuff)
        {
            free(inDecoder->remainData.remainBuff);
            inDecoder->remainData.remainBuff = NULL;
            inDecoder->remainData.remainedNum = 0;
        }
        
        free(inDecoder);
    }

    return (kNoErr);
}


OSStatus AacDecoder_SetConfigure(aac_decoder_ref inDecoder, const AACParams *params)
{
    OSStatus err = kNoErr;

    require_action(inDecoder, exit, err = kParamErr);
    require_action(inDecoder->hAac, exit, err = kParamErr);
    require_action(params, exit, err = kParamErr);
    
    NeAACDecConfigurationPtr conf = NeAACDecGetCurrentConfiguration(inDecoder->hAac);
    require_action(conf, exit, err = kParamErr);
    
    //inDecoder->sampleRate = conf->defSampleRate;
    //inDecoder->channels     = params->numChannels;

    conf->defSampleRate           = 44100;
    conf->defObjectType           = LC;
    conf->outputFormat            = FAAD_FMT_16BIT;
    conf->dontUpSampleImplicitSBR = 1;

    /* Set the new configuration */
    NeAACDecSetConfiguration(inDecoder->hAac, conf);
    
exit:
    return (err);    
}


static OSStatus AacDecoder_alloc_remain_buff(aac_decoder_ref inDecoder, uint8_t *inBuffPtr, uint32_t inBuffSize)
{
    OSStatus err = kNoErr;
    uint8_t *resBuf = NULL;
    
    require_action(inBuffPtr, exit, err = kNoErr);
    require_action(inDecoder, exit, err = kNoErr);

    if (inBuffSize)
    {
        resBuf = calloc(1, (inBuffSize + 1));
        require_action(resBuf, exit, err = kNoMemoryErr);
        
        memcpy(resBuf, inBuffPtr, inBuffSize);
        inDecoder->remainData.remainBuff = resBuf;
        inDecoder->remainData.remainedNum = inBuffSize;
        aac_ulog(kLogLevelInfo, "AacDecoder_alloc_remain_buff([0x%x],size=[%u]).\n", inDecoder->remainData.remainBuff, inDecoder->remainData.remainedNum);
    }
    
exit:
    if (err == kNoMemoryErr)
        aac_ulog(kLogLevelInfo, "AacDecoder_alloc_remain_buff() fail for memory unenough.\n");
    
    return (err);    
}


/*
** reassemble buff for remained data
** return kAsyncNoErr if needed or NoErr if unneed.
** other err for assamble error.
*/
static OSStatus AacDecoder_buff_reassemble(uint8_t *inBuffPtr, uint32_t inBuffSize, 
            aac_decoder_ref inDecoder,uint8_t **outReassembleBufPtr, uint32_t *outReassembleSize)
{
    OSStatus err = kNoErr;
    uint8_t *resBuf = NULL;
    uint8_t have_remain_data = 0;

    require_action(inBuffPtr, exit, err = kNoErr);
    require_action(inDecoder, exit, err = kNoErr);    

    have_remain_data = ((inDecoder->remainData.remainedNum > 0) && inDecoder->remainData.remainBuff) ? 1 : 0;
    
    /* there were some remained data, need reassemble them */
    if (have_remain_data)
    {
        aac_ulog(kLogLevelInfo, "AAC buffer need sync, inBuffSize [%u].\n", inBuffSize);
        
        resBuf = calloc(1, (inBuffSize + inDecoder->remainData.remainedNum));
        require_action(resBuf, exit, err = kNoMemoryErr);

        memmove(resBuf, inDecoder->remainData.remainBuff, inDecoder->remainData.remainedNum);
        memmove(resBuf + inDecoder->remainData.remainedNum, inBuffPtr, inBuffSize);

        if (outReassembleBufPtr)
            *outReassembleBufPtr = resBuf;
        
        if (outReassembleSize)
            *outReassembleSize = (inBuffSize + inDecoder->remainData.remainedNum);

        err = kAsyncNoErr;/* buff sync */
    }
    
exit:

    /* free remained buff whatever err happened */
    if (have_remain_data)
    {
        free(inDecoder->remainData.remainBuff);
        inDecoder->remainData.remainBuff = NULL;
        inDecoder->remainData.remainedNum = 0;
    }
    
    return err;
}


OSStatus AacDecoder_Decode(aac_decoder_ref inDecoder, uint8_t *inBuffPtr, uint32_t inBuffSize, uint8_t *OutBuffPtr, uint32_t *OutSamplesSize)
{
    OSStatus err = kNoErr;
    faacDecFrameInfo hInfo; 
    uint32_t nDecodedSampleLen = 0;
    unsigned long samplerate;
    unsigned char channels;
    uint8_t *assembled_buf = NULL;
    int first_decode = 0, buff_sync_flag = 0;

    uint8_t *inputDecPtr = inBuffPtr;
    uint32_t inputDecSize  = inBuffSize;
    uint8_t *outPutDecPtr = OutBuffPtr;
    
    require_action(inDecoder, exit, err = kParamErr);

    if (kAsyncNoErr == AacDecoder_buff_reassemble(inBuffPtr, inBuffSize, inDecoder, &inputDecPtr, &inputDecSize))
    {
        buff_sync_flag = 1;
        assembled_buf = inputDecPtr;
    }

    if (!inDecoder->aac_dec_inited)
    {
        int faac_init = 0;
        unsigned long i_bps = 0; 
        faac_init = faacDecInit(inDecoder->hAac, inBuffPtr, inBuffSize, &samplerate, &channels, &i_bps);

        if (faac_init < 0) 
        {
            aac_ulog(kLogLevelInfo, "faacDecInit() error, use faacDecInit2 try again.\n");
            faac_init = faacDecInit2(inDecoder->hAac, inBuffPtr, inBuffSize, &samplerate, &channels);
            if (faac_init < 0) 
            {
                aac_ulog(kLogLevelInfo, "faacDecInit2() error, we have to cancel decoding.\n");
                err = kInternalErr;
                goto exit;
            }
        }
         
        inDecoder->aac_dec_inited = ((faac_init < 0) ? 0 : 1);

        if (inDecoder->aac_dec_inited)
        {
            aac_ulog(kLogLevelInfo, "NeAACDecInit successfully.\n");
            first_decode = 1;
            inputDecPtr = inBuffPtr + faac_init;
            inputDecSize -= faac_init;
        }
    }

    /* decode main process */
    do
    {
        uint8_t *curDecOutPtr = faacDecDecode(inDecoder->hAac, &hInfo, inputDecPtr, inputDecSize);

        if ((hInfo.error == 0) && (hInfo.samples == 0) && first_decode)
        {
            /* the first time decode, no err and nothing */
            err = kNoErr;
            *OutSamplesSize = 0;
            first_decode = 0;
            aac_ulog(kLogLevelInfo, "First time decoder init, nothing to sampled.\n");
            break;
        }

        if ((hInfo.error == 0) && (hInfo.samples > 0))
        {
 #ifdef PRINT_DEBUG_DECODER_INFO     
            printf("# decoded info: bytesconsumed %d, channels %d, header_type %d, object_type %d, samples %d, samplerate %d\n",   
            hInfo.bytesconsumed,   
            hInfo.channels, hInfo.header_type,   
            hInfo.object_type, hInfo.samples,   
            hInfo.samplerate);  
#endif
            /* decoded samples successfully */
            nDecodedSampleLen = (hInfo.channels) * (hInfo.samples);
            memmove(outPutDecPtr, curDecOutPtr, nDecodedSampleLen);
            
            *OutSamplesSize += (nDecodedSampleLen / 4); /* 4 for 16-bit stereo */ 
            outPutDecPtr  += nDecodedSampleLen;
        }
        else if (hInfo.error != 0)
        {
            /* Some error occurred while decoding this frame */
            aac_ulog(kLogLevelInfo, "NeAACDecode error: %d,%s\n", hInfo.error, NeAACDecGetErrorMessage(hInfo.error));
            err = kInternalErr;
            goto exit;
        }
        
        inputDecPtr  += hInfo.bytesconsumed;
        inputDecSize -= hInfo.bytesconsumed;   

        if ((inputDecSize > 0) && (nDecodedSampleLen > inputDecSize))
        {
            /* we have to reassemble audio buff manually */
            AacDecoder_alloc_remain_buff(inDecoder, inputDecPtr, inputDecSize);
            break;
        }        
    }while (hInfo.error == 0 && inputDecSize > 0);
    
exit:

    if (buff_sync_flag)
    {
        free(assembled_buf);
        assembled_buf = NULL;
    }
    
    return err;
}

#endif /* LINKPLAY */

