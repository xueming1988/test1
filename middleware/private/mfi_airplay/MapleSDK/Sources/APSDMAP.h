/*
	Copyright (C) 2014-2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple. Not to be used or disclosed without permission from Apple.
 */

#ifndef __APSDMAP_h__
#define __APSDMAP_h__

//#include "APSCommonServices.h"
//#include "APSDebugServices.h"

#ifdef __cplusplus
extern "C" {
#endif

//===========================================================================================================================
//	Configuration
//===========================================================================================================================

// DMAP_EXPORT_CONTENT_CODE_SYMBOLIC_NAMES: 1=Include constant names (e.g. "kDMAPItemIDCode") for content codes and names.

#if (!defined(DMAP_EXPORT_CONTENT_CODE_SYMBOLIC_NAMES))
#if (DEBUG)
#define DMAP_EXPORT_CONTENT_CODE_SYMBOLIC_NAMES 1
#else
#define DMAP_EXPORT_CONTENT_CODE_SYMBOLIC_NAMES 0
#endif
#endif

//===========================================================================================================================
//	Constants and Types
//===========================================================================================================================

/* Values returned with kDAAPSongDataKindCode */
enum {
    kDAAPSongDataKind_LocalFile = 0,
    kDAAPSongDataKind_RemoteStream = 1
};

/* DACP Player States */
enum {
    kDACPPlayerState_Undefined = 0,
    kDACPPlayerState_Playing = 1,
    kDACPPlayerState_Paused = 2,
    kDACPPlayerState_Stopped = 3,
    kDACPPlayerState_FastFwd = 4,
    kDACPPlayerState_Rewind = 5
};

/* Values returned with kDAAPSongContentRatingCode */
enum {
    kDAAPContentRating_NoRestrictions = 0, // default content rating, anyone can listen
    kDAAPContentRating_SomeAdvisory = 1, // parental advisory, no reason given
    kDAAPContentRating_CleanLyrics = 2 // parental advisory, clean version of an explict song
};

typedef uint32_t DMAPContentCode;
enum {
    kDMAPUndefinedCode = 0x00000000 //! not used & not sent in ContentCodes Response
};

/* Values returned as Content Code code value types */
typedef uint8_t DMAPCodeValueType;
enum {
    kDMAPCodeValueType_Undefined = 0,
    kDMAPCodeValueType_Boolean = 1, // Alias for UInt8 for clarity (1=true, 0 or not present=false).
    kDMAPCodeValueType_UInt8 = 1, /* unsigned 1 byte integer */
    kDMAPCodeValueType_SInt8 = 2, /* signed 1 byte integer */
    kDMAPCodeValueType_UInt16 = 3, /* unsigned 2 byte integer */
    kDMAPCodeValueType_SInt16 = 4, /* signed 2 byte integer */
    kDMAPCodeValueType_UInt32 = 5, /* unsigned 4 byte integer */
    kDMAPCodeValueType_SInt32 = 6, /* signed 4 byte integer */
    kDMAPCodeValueType_UInt64 = 7, /* unsigned 8 byte integer */
    kDMAPCodeValueType_SInt64 = 8, /* signed 8 byte integer */
    kDMAPCodeValueType_UTF8Characters = 9, /* UTF8 characters (NOT C nor Pascal String)  */
    kDMAPCodeValueType_Date = 10, /* Date as seconds since Jan 1, 1970 UTC */
    kDMAPCodeValueType_Version = 11, /* 32 bit value, high word = major version, low word = minor version */
    kDMAPCodeValueType_ArrayHeader = 12, /* arbitrary length container of tagged data */
    kDMAPCodeValueType_DictionaryHeader = 13, /* arbitrary length container of tagged data */
    kDMAPCodeValueType_Float32 = 14, /* 32-bit floating point value */
    kDMAPCodeValueType_CustomData = 15
};

// DMAPContentCodeEntry

typedef struct
{
    DMAPContentCode code;
    const char* name;
    DMAPCodeValueType type;
#if (DMAP_EXPORT_CONTENT_CODE_SYMBOLIC_NAMES)
    const char* codeSymbol;
    const char* nameSymbol;
#endif

} DMAPContentCodeEntry;

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

const DMAPContentCodeEntry* APSDMAPFindEntryByContentCode(DMAPContentCode inCode);

//===========================================================================================================================
//	.i Includes
//===========================================================================================================================

// Include the .i file multiple times with different options to declare constants, etc.

#define DMAP_DECLARE_CONTENT_CODE_CODES 1
#include "APSDMAP.i"

#define DMAP_DECLARE_CONTENT_CODE_NAMES 1
#include "APSDMAP.i"

#define DMAP_DECLARE_CONTENT_CODE_TABLE 1
#include "APSDMAP.i"

#ifdef __cplusplus
}
#endif

#endif // __APSDMAP_h__
