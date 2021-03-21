/*
	File:    APSDMAP.h
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
	
	Copyright (C) 2007-2014 Apple Inc. All Rights Reserved.
*/

#ifndef	__APSDMAP_h__
#define	__APSDMAP_h__

#include "APSCommonServices.h"
#include "APSDebugServices.h"

#ifdef __cplusplus
extern "C" {
#endif

//===========================================================================================================================
//	Configuration
//===========================================================================================================================

// DMAP_EXPORT_CONTENT_CODE_SYMBOLIC_NAMES: 1=Include constant names (e.g. "kDMAPItemIDCode") for content codes and names.

#if( !defined( DMAP_EXPORT_CONTENT_CODE_SYMBOLIC_NAMES ) )
	#if( DEBUG )
		#define	DMAP_EXPORT_CONTENT_CODE_SYMBOLIC_NAMES		1
	#else
		#define	DMAP_EXPORT_CONTENT_CODE_SYMBOLIC_NAMES		0
	#endif
#endif

//===========================================================================================================================
//	Constants and Types
//===========================================================================================================================

/* Values returned with kDAAPSongDataKindCode */
enum
{
	kDAAPSongDataKind_LocalFile				= 0,
	kDAAPSongDataKind_RemoteStream			= 1
};

/* DACP Player States */
enum
{
	kDACPPlayerState_Undefined	= 0,
	kDACPPlayerState_Playing	= 1,
	kDACPPlayerState_Paused		= 2,
	kDACPPlayerState_Stopped	= 3,
	kDACPPlayerState_FastFwd	= 4,
	kDACPPlayerState_Rewind		= 5
};

typedef uint32_t	DMAPContentCode;
enum
{
	kDMAPUndefinedCode = 0x00000000 //! not used & not sent in ContentCodes Response
};

/* Values returned as Content Code code value types */
typedef uint8_t		DMAPCodeValueType;
enum
{
	kDMAPCodeValueType_Undefined			= 0,
	kDMAPCodeValueType_Boolean				= 1,  // Alias for UInt8 for clarity (1=true, 0 or not present=false).
	kDMAPCodeValueType_UInt8				= 1,  /* unsigned 1 byte integer */
	kDMAPCodeValueType_SInt8				= 2,  /* signed 1 byte integer */
	kDMAPCodeValueType_UInt16				= 3,  /* unsigned 2 byte integer */
	kDMAPCodeValueType_SInt16				= 4,  /* signed 2 byte integer */
	kDMAPCodeValueType_UInt32				= 5,  /* unsigned 4 byte integer */
	kDMAPCodeValueType_SInt32				= 6,  /* signed 4 byte integer */
	kDMAPCodeValueType_UInt64				= 7,  /* unsigned 8 byte integer */
	kDMAPCodeValueType_SInt64				= 8,  /* signed 8 byte integer */
	kDMAPCodeValueType_UTF8Characters		= 9,  /* UTF8 characters (NOT C nor Pascal String)  */
	kDMAPCodeValueType_Date					= 10, /* Date as seconds since Jan 1, 1970 UTC */
	kDMAPCodeValueType_Version				= 11, /* 32 bit value, high word = major version, low word = minor version */
	kDMAPCodeValueType_ArrayHeader			= 12, /* arbitrary length container of tagged data */
	kDMAPCodeValueType_DictionaryHeader		= 13, /* arbitrary length container of tagged data */
	kDMAPCodeValueType_Float32				= 14, /* 32-bit floating point value */
	kDMAPCodeValueType_CustomData			= 15
};

// DMAPContentCodeEntry

typedef struct
{
	DMAPContentCode			code;
	const char *			name;
	DMAPCodeValueType		type;
#if( DMAP_EXPORT_CONTENT_CODE_SYMBOLIC_NAMES )
	const char *			codeSymbol;
	const char *			nameSymbol;
#endif

}	DMAPContentCodeEntry;

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

const DMAPContentCodeEntry *	APSDMAPFindEntryByContentCode( DMAPContentCode inCode );

//===========================================================================================================================
//	.i Includes
//===========================================================================================================================

// Include the .i file multiple times with different options to declare constants, etc.

#define DMAP_DECLARE_CONTENT_CODE_CODES		1
#include "APSDMAP.i"

#define DMAP_DECLARE_CONTENT_CODE_NAMES		1
#include "APSDMAP.i"

#define DMAP_DECLARE_CONTENT_CODE_TABLE		1
#include "APSDMAP.i"

#ifdef __cplusplus
}
#endif

#endif // __APSDMAP_h__
