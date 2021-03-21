/*
	File:    APSDMAP.i
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

// WARNING: This is a special include-only file that is intended to be included by other files and is not standalone.

#if 0
#pragma mark == Macros ==
#endif

#if( defined( DMAP_DECLARE_CONTENT_CODE_CODES ) && DMAP_DECLARE_CONTENT_CODE_CODES )
	#define DMAP_TABLE_BEGIN()									enum {
	#define DMAP_TABLE_END()									kDMAPContentCodeEnd };
	#define DMAP_ENTRY( CODE_SYMBOL, CODE, NAME, TYPE )			CODE_SYMBOL = CODE, 
#elif( defined( DMAP_DECLARE_CONTENT_CODE_NAMES ) && DMAP_DECLARE_CONTENT_CODE_NAMES )
	#define DMAP_TABLE_BEGIN()
	#define DMAP_TABLE_END()
	#define DMAP_ENTRY( CODE_SYMBOL, CODE, NAME, TYPE )			extern const char CODE_SYMBOL ## NameStr[];
#elif( defined( DMAP_DEFINE_CONTENT_CODE_NAMES ) && DMAP_DEFINE_CONTENT_CODE_NAMES )
	#define DMAP_TABLE_BEGIN()
	#define DMAP_TABLE_END()
	#define DMAP_ENTRY( CODE_SYMBOL, CODE, NAME, TYPE )			const char CODE_SYMBOL ## NameStr[] = NAME;
#elif( defined( DMAP_DECLARE_CONTENT_CODE_TABLE ) && DMAP_DECLARE_CONTENT_CODE_TABLE )
	#define DMAP_TABLE_BEGIN()									extern const DMAPContentCodeEntry	kDMAPContentCodeTable[];
	#define DMAP_TABLE_END()
	#define DMAP_ENTRY( CODE_SYMBOL, CODE, NAME, TYPE )
#elif( defined( DMAP_DEFINE_CONTENT_CODE_TABLE ) && DMAP_DEFINE_CONTENT_CODE_TABLE )
	#if( DMAP_EXPORT_CONTENT_CODE_SYMBOLIC_NAMES )
		#define DMAP_TABLE_BEGIN()								const DMAPContentCodeEntry		kDMAPContentCodeTable[] = {
		#define DMAP_TABLE_END()								{ kDMAPUndefinedCode, "", kDMAPCodeValueType_Undefined, "", "" } };
		#define DMAP_ENTRY( CODE_SYMBOL, CODE, NAME, TYPE )		{ CODE, NAME, TYPE, # CODE_SYMBOL, # CODE_SYMBOL "NameStr" }, 
	#else
		#define DMAP_TABLE_BEGIN()								const DMAPContentCodeEntry		kDMAPContentCodeTable[] = {
		#define DMAP_TABLE_END()								{ kDMAPUndefinedCode, "", kDMAPCodeValueType_Undefined } };
		#define DMAP_ENTRY( CODE_SYMBOL, CODE, NAME, TYPE )		{ CODE, NAME, TYPE }, 
	#endif
#endif

DMAP_TABLE_BEGIN()

DMAP_ENTRY( kDAAPSongAlbumCode,						0x6173616C /* 'asal' */, "daap.songalbum",					kDMAPCodeValueType_UTF8Characters )
DMAP_ENTRY( kDAAPSongArtistCode,					0x61736172 /* 'asar' */, "daap.songartist",					kDMAPCodeValueType_UTF8Characters )
DMAP_ENTRY( kDAAPSongComposerCode,					0x61736370 /* 'ascp' */, "daap.songcomposer",				kDMAPCodeValueType_UTF8Characters )
DMAP_ENTRY( kDAAPSongContentRatingCode,             0x61736372 /* 'ascr' */, "daap.songcontentrating",            kDMAPCodeValueType_UInt8 )
DMAP_ENTRY( kDAAPSongDataKindCode,					0x6173646B /* 'asdk' */, "daap.songdatakind",				kDMAPCodeValueType_UInt8 )
DMAP_ENTRY( kDAAPSongDiscCountCode,					0x61736463 /* 'asdc' */, "daap.songdisccount",				kDMAPCodeValueType_UInt16 )
DMAP_ENTRY( kDAAPSongDiscNumberCode,				0x6173646E /* 'asdn' */, "daap.songdiscnumber",				kDMAPCodeValueType_UInt16 )
DMAP_ENTRY( kDAAPSongGenreCode,						0x6173676E /* 'asgn' */, "daap.songgenre",					kDMAPCodeValueType_UTF8Characters )
DMAP_ENTRY( kDAAPSongTimeCode,						0x6173746D /* 'astm' */, "daap.songtime",					kDMAPCodeValueType_UInt32 )
DMAP_ENTRY( kDAAPSongTrackCountCode,				0x61737463 /* 'astc' */, "daap.songtrackcount",				kDMAPCodeValueType_UInt16 )
DMAP_ENTRY( kDAAPSongTrackNumberCode,				0x6173746E /* 'astn' */, "daap.songtracknumber",			kDMAPCodeValueType_UInt16 )
DMAP_ENTRY( kDACPPlayerStateCode,					0x63617073 /* 'caps' */, "dacp.playerstate",				kDMAPCodeValueType_UInt8 )
DMAP_ENTRY( kDMAPContentCodesNameCode,				0x6D636E61 /* 'mcna' */, "dmap.contentcodesname",			kDMAPCodeValueType_UTF8Characters )
DMAP_ENTRY( kDMAPContentCodesNumberCode,			0x6D636E6D /* 'mcnm' */, "dmap.contentcodesnumber",			kDMAPCodeValueType_UInt32 )
DMAP_ENTRY( kDMAPContentCodesTypeCode,				0x6D637479 /* 'mcty' */, "dmap.contentcodestype",			kDMAPCodeValueType_UInt16 )
DMAP_ENTRY( kDMAPDictionaryCollectionCode,			0x6D64636C /* 'mdcl' */, "dmap.dictionary",					kDMAPCodeValueType_DictionaryHeader )
DMAP_ENTRY( kDMAPItemIDCode, 						0x6D696964 /* 'miid' */, "dmap.itemid",						kDMAPCodeValueType_UInt32 )
DMAP_ENTRY( kDMAPItemNameCode, 						0x6D696E6D /* 'minm' */, "dmap.itemname",					kDMAPCodeValueType_UTF8Characters )
DMAP_ENTRY( kDMAPListingItemCode,					0x6D6C6974 /* 'mlit' */, "dmap.listingitem",				kDMAPCodeValueType_DictionaryHeader )
DMAP_ENTRY( kDMAPPersistentIDCode,					0x6D706572 /* 'mper' */, "dmap.persistentid",				kDMAPCodeValueType_UInt64 )
DMAP_ENTRY( kDMAPServerInfoResponseCode, 			0x6D737276 /* 'msrv' */, "dmap.serverinforesponse",			kDMAPCodeValueType_DictionaryHeader )
DMAP_ENTRY( kDMAPStatusCode, 						0x6D737474 /* 'mstt' */, "dmap.status",						kDMAPCodeValueType_UInt32 )
DMAP_ENTRY( kDMAPStatusStringCode,					0x6D737473 /* 'msts' */, "dmap.statusstring",				kDMAPCodeValueType_UTF8Characters )
DMAP_ENTRY( kExtDAAPITMSSongIDCode,					0x61655349 /* 'aeSI' */, "com.apple.itunes.itms-songid",	kDMAPCodeValueType_UInt32 )

DMAP_TABLE_END()

// #undef macros so this file can be re-included again with different options.

#undef DMAP_TABLE_BEGIN
#undef DMAP_TABLE_END
#undef DMAP_ENTRY

#undef DMAP_DECLARE_CONTENT_CODE_CODES
#undef DMAP_DECLARE_CONTENT_CODE_NAMES
#undef DMAP_DEFINE_CONTENT_CODE_NAMES
#undef DMAP_DECLARE_CONTENT_CODE_TABLE
#undef DMAP_DEFINE_CONTENT_CODE_TABLE
