#ifndef __VOICERECOGNIZEEMBED_H__
#define __VOICERECOGNIZEEMBED_H__

#ifdef WIN32
/*
 * windows default calling convention: __cdecl
 */
#ifdef VRAPI_EXPORTS
#define VRAPI_API __declspec(dllexport)
#else
#define VRAPI_API __declspec(dllimport)
#endif
#endif

#ifdef __linux__
#define VRAPI_API
#endif

#ifdef _ANDROID_PLATFORM_
#define VRAPI_API
#endif

#ifdef __APPLE__
#define VRAPI_API
#endif

typedef void* VoiceRecognizeEmbedHandle;

/**
 * @brief The result struct
 */
struct VoiceRecognizeResult {
	/**
	 * recognize result
	 * NULL means no the key word and sentence
	 */
	const char* text;

	/**
	 * you needn't the followings if you just need wake up recognition.
	 * they are used for 'address book semantic operation'
	 */
	// action
	const char* action;
	// name
	const char* name;
	// the id of gram, see the "Gram" file, -1 means no result
	int type;
};

/**
 * @brief: new the handle, read the resource and ready to recognize
 * must appear in pairs with 'VoiceRecognizeEmbedRelease'
 *
 * pResPath: the path of the resource
 * pResName: the name of the resource
 * pNameList: you needn't it if you just need wake up recognize.
 *            the person names list, separate the names by '\n'.
 *            name list is loaded from file when 'pNameList = 0', and
 *            the gram with '$name' will be ignored if the file isn't exist.
 */
VRAPI_API int VoiceRecognizeEmbedInit(VoiceRecognizeEmbedHandle *pHandle,
                                      const char *pResPath,
                                      const char *pResName,
                                      const char *pNameList = 0);

/**
 * @brief: ready a new recognize
 * must appear in pairs with 'VoiceRecognizeEmbedEnd'
 */
VRAPI_API int VoiceRecognizeEmbedBegin(VoiceRecognizeEmbedHandle handle);

/**
 * @brief: add the new voice wave to recognize
 * pWavBuf: the voice wave buffer
 * nWavLength: the voice wave length
 */
/*return 0 means recognizing continue*/
/*return 1 means recognizing finished, you can get result right now before 'VoiceRecognizeEmbedEnd'*/
/*return -1 means error*/
VRAPI_API int VoiceRecognizeEmbedAddData(VoiceRecognizeEmbedHandle handle,
        const char *pWavBuf,
        int nWavLength);
/**
 * @brief: tell the recognizer the wav is end
 * must appear in pairs with 'VoiceRecognizeEmbedBegin'
 */
VRAPI_API int VoiceRecognizeEmbedEnd(VoiceRecognizeEmbedHandle handle);

/**
 * @brief: get the result
 * if VoiceRecognizeEmbedAddData return 1 already, you can use this function directly
 * else you only can use it after VoiceRecognizeEmbedEnd.
 */
VRAPI_API int VoiceRecognizeEmbedGetResult(VoiceRecognizeEmbedHandle handle,
        VoiceRecognizeResult &result);

/**
 * @brief: release the handle
 * must appear in pairs with 'VoiceRecognizeEmbedInit'
 */
VRAPI_API void VoiceRecognizeEmbedRelease(VoiceRecognizeEmbedHandle *pHandle);


/**
 * @brief: update the name slot, you needn't it if you just need wake up recognize.
 * pNameList: the person names list, separate the names by '\n'.
 *            name list is loaded from file when 'pNameList = 0', and
 *            the gram with '$name' will be ignored if the file isn't exist.
 */
VRAPI_API int VoiceRecognizeEmbedUpdateNameSlot(VoiceRecognizeEmbedHandle handle,
        const char *pNameList);

/**
 * @brief: set the recognize <keyword set> index
 */
VRAPI_API int VoiceRecognizeEmbedSetKeywordSetIndex(VoiceRecognizeEmbedHandle handle,
        int nKeywordSetIndex);

/*
 * @brief: get the version of sdk & bin
 */
VRAPI_API int VoiceRecognizeEmbedGetVersion(VoiceRecognizeEmbedHandle handle,
        int *sdkVersion, int *binVersion);

/*
 * @brief: get the start offset of result
 */
VRAPI_API int VoiceRecognizeEmbedGetResultStartOffset(VoiceRecognizeEmbedHandle handle);

/*
 * @brief: get the end offset of result
 */
VRAPI_API int VoiceRecognizeEmbedGetResultEndOffset(VoiceRecognizeEmbedHandle handle);

#endif
