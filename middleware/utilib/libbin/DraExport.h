/**********************************************************************
 * DraExport.h, DRA decoder header file
 **********************************************************************/
/**********************************************************************
 * Copyright (C) 2007, Digital Rise. All rights reversed
 *
 * These software programs are available to the user under NDA. Without
 * the prior writen permission from Digital Rise Technology, neither the 
 * whole nor any part of these programs may be disclosed to any third 
 * party.
 **********************************************************************/


#ifndef _DRAEXPORT_H_
#define _DRAEXPORT_H_

#ifdef __cplusplus
extern "C" {
#endif


#ifdef DRADECDLL_EXPORTS
#define DRA_API __declspec(dllexport)
#else
#define DRA_API
#endif
/**************************************************************
 *                    Macro Definitions
 **************************************************************/
//Dowmmixing mode
#define DXMODE_UNUSE		0      //Without downmixing
#define DXMODE_10			1      //Multi-channel to 1/0 channel output
#define DXMODE_20			2      //Multi-channel to 2/0 channel output
#define DXMODE_LtRt			3      //Multi-channel to LtRt output
#define DXMODE_321			4      //Multi-channel to 3/2/1 channel output

//Dra decode error message
enum DRADEC_ERRMSG
{
	DRADECSUCCESS,	
	DRADECERR,
	DRADECERR_INVALIDBUF,
	DRADECERR_INVALIDSAMPLERATE,
	DRADECERR_INVALIDCHNUM,
	DRADECERR_NOSYNCWORD,
	DRADECERR_INVALIDPOINTER,
	DRADECERR_MEMOVERFLOW,
	DRADECERR_NOTINIT,
	DRADECERR_AUDIOPARAMETERCHANGED,
	DRADECERR_INVALIDSTREAM,
	DRADECERR_LIMIT

};

/**************************************************************
 *                    Type Definitions
 **************************************************************/


/***************************************************************
 * DRA frame information structure, outputed by DRA_DecodeFrame()
 * function after decoding.
****************************************************************/
typedef struct 
{
	int nSampleRate;		//Sampling rate
	int nChannelNormal;		//Number of  normal channels
	int nChannelLfe;		//Number of  LFE channel
    int fInstantBitrate;	//Bitrate of this frame

}dra_frame_info;

/**************************************************************
 * Dra initilize structure
 **************************************************************/
typedef struct 
{
	int initMode;				//0: initialize by input channel;   1: initialize by the frist frame
	int channel;				//channel for initialize mode 0
	int byteOrder;				//0: Inverted byte order    1: normal byte order
}dra_cfg;


/**************************************************************
 * Audio information structure
 **************************************************************/
typedef struct 
{
	int SampleRate;		  	//Sampling rate		
	int nChannelNormal;		//Number of  normal channels
	int nChannelLfe;		//Number of  LFE channel
	int JIC;				//Joint intensity code used flag
	int SumDiff;			//sum  difference code used flag	

}dra_audio_info;




/**********************************************************************
 *                 Interface functions
 **********************************************************************/

/**************************************************** 
 * DRA_InitDecode: Initialize decoding, must be executed 
 * once and only once at the beginnging of a decoder
 * application.
 *
 * Output parameters: 
 *
 *  pAudioInfo:
 *			Structure contains information of the
 *      input frame, these information maybe needed 
 *      for player applications.Only use in mode 1.
 *
 * Input parameters:
 *
 *  ppvDraDecoder:
 *			Point to the object of DRA decoder.
 *
 *  pDraCfg: 
 *			The structure contains setting for initializing. 	
 *  pInBuf: 
 *			Point to the buffer stores one DRA frame data 
 *    	for Initialize decoder.Only use in mode 1.
 * nInlength:
 *			Length of input DRA frame data.Only use in mode 1.
 *		
 *Return Values:
 *			If  decoder Initialize  successfully,the return value is zero,
 *			or else,it means fails.
 			
 *******************************************************/
 /****************************************************/
DRA_API int DRA_InitDecode(
	void **ppvDraDecoder,
	dra_cfg* pDraCfg,
	unsigned char* pDraFrameBuf, 
	int nLength,
	dra_audio_info* pAudioInfo
	);


/**************************************************** 
 * DRA_Release: Realse decoder resoure,must be called
 *
 * Input parameters:
 *
 *  ppvDraDecoder:
 *			Point to  the object of DRA decoder.
 *
 *Return Values:
 *			If  decoder realse  successfully,the return value is zero,
 *			or else,it means fails.
 ****************************************************/
DRA_API int DRA_Release(void **ppvDraDecoder);

/**************************************************** 
 * DRA_DecodeFrame: Decode one and only one DRA frame, must 
 * be called for each frame.  
 *
 * Output parameters: 
 *
 *  pOutBuf: 
 *		   Buffer stores PCM data after decoding arranged 
 *		   in the Microsoft WAVEFORMATEXTENSIBLE style,  
 *       he size of this buffer must >=  1024 * number  
 *       of channels * 2 bytes (or 3bytes)  , for a DRA 
 *			 frame contains 1024 samples, and the output 
 *			 PCM samples are 16-bits(2 bytes) formatted,
 *			or 24-bits(3bytes)formatted.
 * pOutLength:
 *			Length of output PCM data.
 *
 *  pFrameInfo:
 *			Structure contains information of the
 *      current frame, these information maybe needed 
 *      for player applications.
 *
 * Input parameters:
 *
 *  ppvDraDecoder:
 *			The object of DRA decoder.
 *  pInBuf: 
 *			Point to the buffer stores one DRA frame data 
 *      begin with a DRA sync_word.
 * nInlength:
 *			Length of input DRA frame data.
 *DownMixMode:
 *			Down mix mode setting,
 *			0: disable; 1: 1.0; 2: 2.0; 3: Lt/Rt; 4: 3/2/1(5.1ch)
 *nBitPcm:
 *			PCM precision setting, only 16bit or 24bit valid.
 *Return Values:
 *			If  DRA frame decode  successfully,the return value is zero,
 *			or else,it means decode fails.
 			
 *******************************************************/
DRA_API int DRA_DecodeFrame(
	void *ppvDraDecoder,
	unsigned char* pInBuf,
	int nInlength,
	unsigned char* pOutBuf,
	int* pOutLength,
	int DownMixMode,			
	int nBitPcm,					
	dra_frame_info* pFrameInfo
	);


/**********************************************************
* DRA_GetMemSize: Inquires size of memory needed by this decode 
*				library.
*
* Input parameters:
* 
* Channel: 
*			Number of audio channels.	
* DownMixMode:
*			Down mix mode setting.
*
* Return Values:
*			Return the size of memory needed by the decoder library.
*			If the return value is -1, it means that the decoder library
*			doesn't need to allocate memory by player applications, 
*			and this decode library is the non-NOMALLOC version.
**********************************************************/
DRA_API int DRA_GetMemSize(
	int Channel,
	int DownMixMode
	);



#ifdef __cplusplus
}
#endif


#endif

