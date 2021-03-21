## Progrom with AVS ##
 
Get start with AVS:  
[https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/getting-started-with-the-alexa-voice-service](https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/getting-started-with-the-alexa-voice-service)

## The steps to progrom with AVS  
1. You should get the API document url of the overview, it will tell you about the AVS's funcations.   
[https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/content/avs-api-overview](https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/content/avs-api-overview)  

2. You should register a device on amazon server.  
register device url: [https://developer.amazon.com/avs/home.html#/](https://developer.amazon.com/avs/home.html#/)  
you can follow the step from the resigster a Raspberry-Pi device to AVS:  
[https://github.com/alexa/alexa-avs-sample-app/wiki/Raspberry-Pi](https://github.com/alexa/alexa-avs-sample-app/wiki/Raspberry-Pi)  

3. Then read the document about the interaction-model.  
 A client interacting with the Alexa Voice Service will regularly encounter events/directives that produce competing audio. For instance, a user may ask a question while Alexa is speaking or a previously scheduled alarm may trigger while music is streaming. The rules that govern the prioritization and handling of these inputs and outputs are referred to as the interaction model.  
[https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/reference/interaction-model](https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/reference/interaction-model)  

4. Understand the device how to login avs with app or website.  
[https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/docs/authorizing-your-alexa-enabled-mobile-app](https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/docs/authorizing-your-alexa-enabled-mobile-app)

5. Understand how to create an connction to AVS, which is how to create the downchannel !  
[https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/docs/managing-an-http-2-connection](https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/docs/managing-an-http-2-connection)

6. Understand the AVS's alerts  
[https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/reference/timers-and-alarms-conceptual-overview](https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/reference/timers-and-alarms-conceptual-overview)  

7. Understand the audio player  
[https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/reference/audioplayer-conceptual-overview](https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/reference/audioplayer-conceptual-overview)

8. Then you should to known how to packet the Http2 data which will send to AVS  
[https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/docs/avs-http2-requests](https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/docs/avs-http2-requests)  

9. You should understand the document clearly, then you can write the code for AVS.

## The API Document

1. sample app  
[https://github.com/alexa/alexa-avs-sample-app](https://github.com/alexa/alexa-avs-sample-app)

2. for raspberry-pi  
[https://github.com/alexa/alexa-avs-sample-app/wiki/Conexant2Mic-Raspberry-Pi](https://github.com/alexa/alexa-avs-sample-app/wiki/Conexant2Mic-Raspberry-Pi)

3. Managing an HTTP/2 Connection with AVS  
[https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/docs/managing-an-http-2-connection](https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/docs/managing-an-http-2-connection)

4. Interaction Model  
[https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/reference/interaction-model#voice-lifecycle](https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/reference/interaction-model#voice-lifecycle)

5. SpeechRecognizer Interface  
[https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/reference/speechrecognizer](https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/reference/speechrecognizer)

6. SpeechSynthesizer Interface  
[https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/reference/speechsynthesizer](https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/reference/speechsynthesizer)

7. Alerts Interface  
[https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/reference/alerts](https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/reference/alerts)

8. Understanding Alerts  
[https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/reference/timers-and-alarms-conceptual-overview](https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/reference/timers-and-alarms-conceptual-overview)

9. AudioPlayer Interface  
[https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/reference/audioplayer](https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/reference/audioplayer)

10. PlaybackController Interface  
[https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/reference/playbackcontroller](https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/reference/playbackcontroller)

11. Speaker Interface  
[https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/reference/speaker](https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/reference/speaker)

12. System Interface  
[https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/reference/system](https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/reference/system)

## The UI Design Guidelines
UX guidelines: the UX guide will tell you hou to design the login app and the promt  
[https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/content/alexa-voice-service-ux-design-guidelines](https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/content/alexa-voice-service-ux-design-guidelines)


## The Beta API URLS
1. Third part audio  
[https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/reference/third-party-distributed-audio](https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/reference/third-party-distributed-audio)  
2. Bluetooth Interface  
[https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/reference/bluetooth](https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/reference/bluetooth)  
3. Cloud-based Wake Word Verification (WWV)    
a. Ring Buffer  
[https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/reference/cloud-based-wake-word-verification](https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/reference/cloud-based-wake-word-verification)  
b. SpeechRecognizer Interface w/ WWV Support  
[https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/reference/speech-recognizer-wwv-support](https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/reference/speech-recognizer-wwv-support)  
c. GUIDE - Enable Wake Word Verification  
[https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/reference/enable-wake-word-verification](https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/reference/enable-wake-word-verification)  
4. Code Based Linking  
[https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/reference/code-based-linking](https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/reference/code-based-linking)  
5. GUI  
[https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/reference/developer-preview-templateruntime-interface](https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/reference/developer-preview-templateruntime-interface)  
6. Notifications  
[https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/reference/notifications](https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/reference/notifications)  

<font color=#ff0000 size=4 face="黑体">**Notice: You should used the beta profile to test the API**</font>

## Develop with C language
### The third part library your need:  

### Prepare the third libs:
<pre>
1. openssl/mbedtls:
      openssl (>=1.02e): https://www.openssl.org
      mbedtls (>=2.3.0): https://tls.mbed.org

2. json-c/cJson:
      json-c: https://github.com/json-c/json-c
      cJson:  https://github.com/DaveGamble/cJSON

3. The http2 C lib nghttp2:
      api document: http://www.nghttp2.org/documentation/index.html
      http2 rfc document: https://www.rfc-editor.org/rfc/pdfrfc/rfc7540.txt.pdf  
      source code: https://github.com/nghttp2/nghttp2  
      nghttp2 (>=1.19.0): http://www.nghttp2.org  
</pre>

<pre>
-------------------------------------example request json-------------------------------------
{
    "context": [
        {
            "header": {
                "namespace": "AudioPlayer",
                "name": "PlaybackState"
            },
            "payload": {
                "token": "{{STRING}}",
                "offsetInMilliseconds": {
                    {
                        LONG
                    }
                },
                "playerActivity": "{{STRING}}"
            }
        },
        {
            "header": {
                "namespace": "Alerts",
                "name": "AlertsState"
            },
            "payload": {
                "allAlerts": [
                    {
                        "token": "{{STRING}}",
                        "type": "{{STRING}}",
                        "scheduledTime": "{{STRING}}"
                    }
                ],
                "activeAlerts": [
                    {
                        "token": "{{STRING}}",
                        "type": "{{STRING}}",
                        "scheduledTime": "{{STRING}}"
                    }
                ]
            }
        },
        {
            "header": {
                "namespace": "Speaker",
                "name": "VolumeState"
            },
            "payload": {
                "volume": {
                    {
                        LONG
                    }
                },
                "muted": {
                    {
                        BOOLEAN
                    }
                }
            }
        },
        {
            "header": {
                "namespace": "SpeechSynthesizer",
                "name": "SpeechState"
            },
            "payload": {
                "token": "{{STRING}}",
                "offsetInMilliseconds": {
                    {
                        LONG
                    }
                },
                "playerActivity": "{{STRING}}"
            }
        }
    ],
    "event": {
        "header": {
            "namespace": "SpeechRecognizer",
            "name": "Recognize",
            "messageId": "{{STRING}}",
            "dialogRequestId": "{{STRING}}"
        },
        "payload": {
            "profile": "{{STRING}}",
            "format": "{{STRING}}"
        }
    }
}

{
    "directive": {
        "header": {
            "namespace": "SpeechRecognizer",
            "name": "StopCapture",
            "messageId": "a83d5a1a-2b63-4c86-a578-990393aa9dc0"
        },
        "payload": {

        }
    }
}
</pre>
