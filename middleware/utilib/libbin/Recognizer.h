//
// Created by yinshang on 15-6-9.
//

#ifndef DOUG_OS_RECOGNIZER_H
#define DOUG_OS_RECOGNIZER_H
#include "speechconfig.h"
#include "VadStatus.h"
#include "RecognizerTypes.h"
#include <string>
#include <fstream>

class OpuCodecProxy;
class VoiceSender;
class VoiceActivityDetectorInterface;

#define BUFFER_MAX_LEN (1024*4)
class Recognizer {
public:
    Recognizer(): mVoiceSenderPointer(NULL), mOpuCodecProxyPointer(NULL),
                 mVoiceActivityDetectorPointer(NULL),mSaveVadFile(false),
                 mVadFilePointer(NULL),mSaveVadFileSequence(0){};
    ~Recognizer();
    bool init(const SpeechConfig config);
    RecognizerStartResult start();
    RecognizerResult getResult();
    RecognizerResult stop();
    void cancel();
    void saveVadFile(std::string path);
    VadStatus update(short pcm[],int length); // 数据缓存到要求长度后 调用update_fix_len

private:
    VadStatus update_fix_len(short pcm[],int length);
    void sendPCM(short *pcm,int length);
    void cleanStagingResources();
    VoiceSender *mVoiceSenderPointer;
    OpuCodecProxy *mOpuCodecProxyPointer;
    VoiceActivityDetectorInterface *mVoiceActivityDetectorPointer;
    VadStatus mCurrentVadStatus;
    SpeechConfig mSpeechConfig;
    bool mSaveVadFile;
    std::string mVadFilePath;
    std::ofstream *mVadFilePointer;
    int mSaveVadFileSequence;
    int mFormat;
    short mVoiceUpdateBuffer[BUFFER_MAX_LEN];
    int mVoiceUpdateBufferLen;
};
#endif //DOUG_OS_RECOGNIZER_H
