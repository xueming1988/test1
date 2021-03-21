/*
	File:    AirPlayReceiverPOSIX.h
	Package: AirPlayAudioPOSIXReceiver
	Version: AirPlay_Audio_POSIX_Receiver_211.1.p8
	
	Disclaimer: IMPORTANT: This Apple software is supplied to you, by Apple Inc. ("Apple"), in your
	capacity as a current, and in good standing, Licensee in the MFi Licensing Program. Use of this
	Apple software is governed by and subject to the terms and conditions of your MFi License,
	including, but not limited to, the restrictions specified in the provision entitled ”Public
	Software”, and is further subject to your agreement to the following additional terms, and your
	agreement that the use, installation, modification or redistribution of this Apple software
	constitutes acceptance of these additional terms. If you do not agree with these additional terms,
	please do not use, install, modify or redistribute this Apple software.
	
	Subject to all of these terms and in consideration of your agreement to abide by them, Apple grants
	you, for as long as you are a current and in good-standing MFi Licensee, a personal, non-exclusive
	license, under Apple's copyrights in this original Apple software (the "Apple Software"), to use,
	reproduce, and modify the Apple Software in source form, and to use, reproduce, modify, and
	redistribute the Apple Software, with or without modifications, in binary form. While you may not
	redistribute the Apple Software in source form, should you redistribute the Apple Software in binary
	form, you must retain this notice and the following text and disclaimers in all such redistributions
	of the Apple Software. Neither the name, trademarks, service marks, or logos of Apple Inc. may be
	used to endorse or promote products derived from the Apple Software without specific prior written
	permission from Apple. Except as expressly stated in this notice, no other rights or licenses,
	express or implied, are granted by Apple herein, including but not limited to any patent rights that
	may be infringed by your derivative works or by other works in which the Apple Software may be
	incorporated.
	
	Unless you explicitly state otherwise, if you provide any ideas, suggestions, recommendations, bug
	fixes or enhancements to Apple in connection with this software (“Feedback”), you hereby grant to
	Apple a non-exclusive, fully paid-up, perpetual, irrevocable, worldwide license to make, use,
	reproduce, incorporate, modify, display, perform, sell, make or have made derivative works of,
	distribute (directly or indirectly) and sublicense, such Feedback in connection with Apple products
	and services. Providing this Feedback is voluntary, but if you do provide Feedback to Apple, you
	acknowledge and agree that Apple may exercise the license granted above without the payment of
	royalties or further consideration to Participant.
	
	The Apple Software is provided by Apple on an "AS IS" basis. APPLE MAKES NO WARRANTIES, EXPRESS OR
	IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY
	AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR
	IN COMBINATION WITH YOUR PRODUCTS.
	
	IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL DAMAGES
	(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
	PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION
	AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT
	(INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
	
	Copyright (C) 2012-2014 Apple Inc. All Rights Reserved.
*/
/*!
    @header AirPlay POSIX Receiver Platform APIs
    @discussion Platform APIs related to AirPlay POSIX Receiver.
*/

#ifndef	__AirPlayReceiverPOSIX_h__
#define	__AirPlayReceiverPOSIX_h__

#include "AirPlayReceiverServer.h"

typedef enum
{
    eAirPlayMetadataTitle       = 1,    // Title of the currently playing content
    eAirPlayMetadataAlbum       = 2,    // Album name of the currently playing content
    eAirPlayMetadataArtist      = 3,    // Artist name of the currently playing content
    eAirPlayMetadataComposer    = 4,    // Composer name of the currently playing content
    eAirPlayMetadataArtwork     = 5,    // Artwork of the currently playing content
    eAirPlayMetadataProgress    = 6     // Progress of the currently playing content
} AirPlayMetadataType;


typedef struct 
{
	void *								bufferPtr;		    // Pointer to the artwork image buffer
	size_t								bufferSize;	        // Number of bytes in the artwork image buffer
	char *								mimeStr;		    // Mime Type of he artwork image
} AirPlayAlbumArt_t;

typedef struct 
{
	uint32_t 							durationSeconds;	// Duration of the currently playing audio content (in seconds)
	uint32_t 							elapsedSeconds;		// Elapsed time of the currently playing audio content (in seconds)
} AirPlayProgressInfo_t;


//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverServerGetRef
	@internal
	@abstract	AirPlay Function that Platform can use to get the current AirPlay server Ref value.
*/
AirPlayReceiverServerRef
	AirPlayReceiverServerGetRef( void );


//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverPlatformSetMetadata
	@abstract	Platform function called by AirPlay to convey info (like artist, album etc) of the current content being played.

	@param	inType	    Type of info being conveyed
	@param	inValue	    Value. See description for more details

	@discussion	

	This function is called by AirPlay to inform Platform of a change in the value of the info for the currently
    playing content. Input parameter inType specifies the info type. The value of the metadata info is specified
    by the input parameter inValue. The type of the inValue parameter is dependent on the inType parameter as listed
    below:
    
	<pre>
	@textblock

              inType                         inValue
              -------                        -------
        - eAirPlayMetadataTitle              char *
        - eAirPlayMetadataAlbum              char *
        - eAirPlayMetadataArtist             char *
        - eAirPlayMetadataComposer           char *
        - eAirPlayMetadataArtwork            AirPlayAlbumArt_t *
        - eAirPlayMetadataProgress           AirPlayProgressInfo_t *

	@/textblock
	</pre>

    AirPlay calls this function whenever it detects a change in the metadata info. Platform should display
    the info to the end user if the accesssory has display capabilities.
*/
void 
    AirPlayReceiverPlatformSetMetadata( 
            AirPlayMetadataType inType, 
            void *inValue );


//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverPlatformGetSupportedMetadata
	@abstract	Platform function called by AirPlay to get the metadata features supported by the accessory. 

	@param	supportedMetadata	    Buffer to return the bitmask of supported metadata features.

	@discussion	

	This function is called by AirPlay to get the metadata features supported by the accessory. Platform
    should return a OR-ed bitmask of the values listed below to specify the various metadata features platform
    requires. This value should be based on the accessorie's ability to use the various metadata types. 
        
	<pre>
	@textblock

        kAirPlayFeature_AudioMetaDataArtwork        Artwork image
        kAirPlayFeature_AudioMetaDataText           Text metadata like Artist, Album etc.
        kAirPlayFeature_AudioMetaDataProgress       Duration and Elapsed time info

	@/textblock
	</pre>

    AirPlay will pass down only the specified metadata to the Plaform during AirPlay streaming. 
*/
void
    AirPlayReceiverPlatformGetSupportedMetadata( 
            AirPlayFeatures *supportedMetadata );


#endif	// __AirPlayReceiverPOSIX_h__
