/*
	File:    AirPlayReceiverServer.c
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

#include "AirPlayReceiverServer.h"
#include "AirPlayReceiverServerPriv.h"

#include "AirPlayCommon.h"
#include "AirPlaySettings.h"
#include "AirPlayUtils.h"
#include "AirPlayVersion.h"
#include "AirTunesServer.h"
#include "NetUtils.h"
#include "RandomNumberUtils.h"
#include "StringUtils.h"
#include "SystemUtils.h"
#include "TickUtils.h"

#if( TARGET_OS_DARWIN || ( TARGET_OS_POSIX && DEBUG ) )
	#include "DebugIPCUtils.h"
#endif
	#include <signal.h>

#include CF_HEADER
#include CF_RUNTIME_HEADER
#include "dns_sd.h"
#include LIBDISPATCH_HEADER

#if( AIRPLAY_DACP )
	#include "AirTunesDACP.h"
#endif

#include "AirPlayAudioReceiverPosixReleaseVersion.h"

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

#if( AIRPLAY_CONFIG_FILES )
	static char *
		AirPlayGetConfigCString( 
			CFStringRef			inKey,
			char *				inBuffer, 
			size_t				inMaxLen, 
			OSStatus *			outErr );
	static void _ProcessLimitedUIElementsConfigArray( const void *value, void *context );
	static void _ProcessLimitedUIElementsConfigArrayDictionaryEntry( const void *key, const void *value, void *context );
#endif
static OSStatus	AirPlayGetDeviceModel( char *inBuffer, size_t inMaxLen );

static void _GetTypeID( void *inContext );
static void	_GlobalInitialize( void *inContext );
#if( AIRPLAY_CONFIG_FILES )
	static OSStatus	_InitializeConfig( AirPlayReceiverServerRef inServer );
#endif
static void	_Finalize( CFTypeRef inCF );
#if( AIRPLAY_DACP )
	static void	_HandleDACPStatus( OSStatus inStatus, void *inContext );
#endif
#if( AIRPLAY_PASSWORDS )
	static void	_UpdatePIN( AirPlayReceiverServerRef inServer, Boolean inForceUpdate );
	static void	_UpdatePINTimerFired( void *inArg );
	static void	_UpdatePINTimerCanceled( void *inArg );
#endif
static void	_UpdatePrefs( AirPlayReceiverServerRef inServer );

static void DNSSD_API
	_BonjourRegistrationHandler( 
		DNSServiceRef		inRef, 
		DNSServiceFlags		inFlags, 
		DNSServiceErrorType	inError, 
		const char *		inName,
		const char *		inType,
		const char *		inDomain,
		void *				inContext );
static void	_RestartBonjour( AirPlayReceiverServerRef inServer );
static void	_RetryBonjour( void *inArg );
static void	_StopBonjour( AirPlayReceiverServerRef inServer, const char *inReason );
static void	_UpdateBonjour( AirPlayReceiverServerRef inServer );
static OSStatus	_UpdateBonjourAirPlay( AirPlayReceiverServerRef inServer, CFDataRef *outTXTRecord );
#if( AIRPLAY_RAOP_SERVER )
	static OSStatus	_UpdateBonjourRAOP( AirPlayReceiverServerRef inServer, CFDataRef *outTXTRecord );
#endif

static void	_StartServers( AirPlayReceiverServerRef inServer );
static void	_StopServers( AirPlayReceiverServerRef inServer );
static void	_UpdateServers( AirPlayReceiverServerRef inServer );

//===========================================================================================================================
//	Globals
//===========================================================================================================================

static const CFRuntimeClass		kAirPlayReceiverServerClass = 
{
	0,							// version
	"AirPlayReceiverServer",	// className
	NULL,						// init
	NULL,						// copy
	_Finalize,					// finalize
	NULL,						// equal -- NULL means pointer equality.
	NULL,						// hash  -- NULL means pointer hash.
	NULL,						// copyFormattingDesc
	NULL,						// copyDebugDesc
	NULL,						// reclaim
	NULL						// refcount
};

static dispatch_once_t			gAirPlayReceiverInitOnce		= 0;
static dispatch_once_t			gAirPlayReceiverServerInitOnce	= 0;
static CFTypeID					gAirPlayReceiverServerTypeID	= _kCFRuntimeNotATypeID;
AirPlayReceiverServerRef		gAirPlayReceiverServer			= NULL;

ulog_define( AirPlayReceiverServer, kLogLevelNotice, kLogFlags_Default, "AirPlay",  NULL );
#define aprs_ucat()					&log_category_from_name( AirPlayReceiverServer )
#define aprs_ulog( LEVEL, ... )		ulog( aprs_ucat(), (LEVEL), __VA_ARGS__ )
#define aprs_dlog( LEVEL, ... )		dlogc( aprs_ucat(), (LEVEL), __VA_ARGS__ )

//===========================================================================================================================
//	AirPlayCopyServerInfo
//===========================================================================================================================

CFDictionaryRef
	AirPlayCopyServerInfo(
		AirPlayReceiverSessionRef inSession,
		CFArrayRef inProperties,
		uint8_t *inMACAddr,
		OSStatus *outErr )
{
	CFDictionaryRef				result = NULL;
	OSStatus					err;
	CFMutableDictionaryRef		info;
	uint8_t						buf[ 32 ];
	char						macStr[ 128 ];
	char						str[ PATH_MAX + 1 ];
	uint64_t					u64;
	CFTypeRef					obj;
	CFDataRef					data;
	
	info = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( info, exit, err = kNoMemoryErr );
	
	// AudioFormats
	
#if( AIRPLAY_CONFIG_FILES )
	obj = CFDictionaryGetValue( gAirPlayReceiverServer->config, CFSTR( kAirPlayProperty_AudioFormats ) );
	CFRetainNullSafe( obj );
	if( !obj )
#endif
	{
		obj = AirPlayReceiverServerPlatformCopyProperty( gAirPlayReceiverServer, kCFObjectFlagDirect,
			CFSTR( kAirPlayProperty_AudioFormats ), NULL, NULL );
	}
	if( obj )
	{
		CFDictionarySetValue( info, CFSTR( kAirPlayProperty_AudioFormats ), obj );
		CFRelease( obj );
	}
	
	// AudioLatencies
	
#if( AIRPLAY_CONFIG_FILES )
	obj = CFDictionaryGetValue( gAirPlayReceiverServer->config, CFSTR( kAirPlayProperty_AudioLatencies ) );
	CFRetainNullSafe( obj );
	if( !obj )
#endif
	{
		obj = AirPlayReceiverServerPlatformCopyProperty( gAirPlayReceiverServer, kCFObjectFlagDirect,
														CFSTR( kAirPlayProperty_AudioLatencies ), NULL, NULL );
	}
	if( obj )
	{
		CFDictionarySetValue( info, CFSTR( kAirPlayProperty_AudioLatencies ), obj );
		CFRelease( obj );
	}
	
	// BluetoothIDs
	
	obj = AirPlayReceiverServerPlatformCopyProperty( gAirPlayReceiverServer, kCFObjectFlagDirect, 
		CFSTR( kAirPlayProperty_BluetoothIDs ), NULL, NULL );
	if( obj )
	{
		CFDictionarySetValue( info, CFSTR( kAirPlayProperty_BluetoothIDs ), obj );
		CFRelease( obj );
	}
	
	// DeviceID
	
	AirPlayGetDeviceID( buf );
	MACAddressToCString( buf, str );
	CFDictionarySetCString( info, CFSTR( kAirPlayKey_DeviceID ), str, kSizeCString );
	
	// Displays
	
	if( inSession )
	{
		obj = AirPlayReceiverSessionPlatformCopyProperty( inSession, kCFObjectFlagDirect, 
			CFSTR( kAirPlayProperty_Displays ), NULL, NULL );
		if( obj )
		{
			CFDictionarySetValue( info, CFSTR( kAirPlayKey_Displays ), obj );
			CFRelease( obj );
		}
	}
	
	// Features
	
	u64 = AirPlayGetFeatures();
	if( u64 != 0 ) CFDictionarySetInt64( info, CFSTR( kAirPlayKey_Features ), u64 );
	
	// FirmwareRevision
	
#if( AIRPLAY_CONFIG_FILES )
	obj = NULL;
	*str = '\0';
	AirPlayGetConfigCString( CFSTR( kAirPlayKey_FirmwareRevision ), str, sizeof( str ), NULL );
	if( *str != '\0' ) CFDictionarySetCString( info, CFSTR( kAirPlayKey_FirmwareRevision ), str, kSizeCString );
	else
#endif
	{
		obj = AirPlayReceiverServerPlatformCopyProperty( gAirPlayReceiverServer, kCFObjectFlagDirect, 
			CFSTR( kAirPlayKey_FirmwareRevision ), NULL, NULL );
	}
	if( obj )
	{
		CFDictionarySetValue( info, CFSTR( kAirPlayKey_FirmwareRevision ), obj );
		CFRelease( obj );
	}
	
	// HardwareRevision
	
#if( AIRPLAY_CONFIG_FILES )
	obj = NULL;
	*str = '\0';
	AirPlayGetConfigCString( CFSTR( kAirPlayKey_HardwareRevision ), str, sizeof( str ), NULL );
	if( *str != '\0' ) CFDictionarySetCString( info, CFSTR( kAirPlayKey_HardwareRevision ), str, kSizeCString );
	else
#endif
	{
		obj = AirPlayReceiverServerPlatformCopyProperty( gAirPlayReceiverServer, kCFObjectFlagDirect, 
			CFSTR( kAirPlayKey_HardwareRevision ), NULL, NULL );
	}
	if( obj )
	{
		CFDictionarySetValue( info, CFSTR( kAirPlayKey_HardwareRevision ), obj );
		CFRelease( obj );
	}
	
	// Interface MAC Address
	
	if( inMACAddr )
	{
		MACAddressToCString( inMACAddr, macStr );
		CFDictionarySetCString( info, CFSTR( kAirPlayKey_MACAddress ), macStr, kSizeCString );
	}
	
	// LimitedUIElements
	
#if( AIRPLAY_CONFIG_FILES )
	obj = CFDictionaryGetValue( gAirPlayReceiverServer->config, CFSTR( kAirPlayKey_LimitedUIElements ) );
	if( obj )
	{
		if( CFIsType( obj, CFArray ) )
		{
			CFMutableArrayRef	elements = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
			CFArrayApplyFunction( (CFArrayRef) obj, CFRangeMake( 0, CFArrayGetCount( (CFArrayRef) obj ) ),
				_ProcessLimitedUIElementsConfigArray, elements);
			obj = elements;
		}
		else
		{
			obj = NULL;
		}
	}
	if( !obj )
#endif
	{
		obj = AirPlayReceiverServerPlatformCopyProperty( gAirPlayReceiverServer, kCFObjectFlagDirect,
			CFSTR( kAirPlayKey_LimitedUIElements ), NULL, NULL );
	}
	if( obj )
	{
		CFDictionarySetValue( info, CFSTR( kAirPlayKey_LimitedUIElements ), obj );
		CFRelease( obj );
	}
	
	// LimitedUI
	obj = AirPlayReceiverServerPlatformCopyProperty( gAirPlayReceiverServer, kCFObjectFlagDirect,
		CFSTR( kAirPlayKey_LimitedUI ), NULL, NULL );
	if( obj )
	{
		CFDictionarySetValue( info, CFSTR( kAirPlayKey_LimitedUI ), obj );
		CFRelease( obj );
	}
	
	// Manufacturer
	
#if( AIRPLAY_CONFIG_FILES )
	obj = NULL;
	*str = '\0';
	AirPlayGetConfigCString( CFSTR( kAirPlayKey_Manufacturer ), str, sizeof( str ), NULL );
	if( *str != '\0' ) CFDictionarySetCString( info, CFSTR( kAirPlayKey_Manufacturer ), str, kSizeCString );
	else
#endif
	{
		obj = AirPlayReceiverServerPlatformCopyProperty( gAirPlayReceiverServer, kCFObjectFlagDirect, 
			CFSTR( kAirPlayKey_Manufacturer ), NULL, NULL );
	}
	if( obj )
	{
		CFDictionarySetValue( info, CFSTR( kAirPlayKey_Manufacturer ), obj );
		CFRelease( obj );
	}
	
	// Model
	
	*str = '\0';
	AirPlayGetDeviceModel( str, sizeof( str ) );
	CFDictionarySetCString( info, CFSTR( kAirPlayKey_Model ), str, kSizeCString );
	
	// Modes
	
	if( inSession )
	{
		obj = AirPlayReceiverSessionPlatformCopyProperty( inSession, kCFObjectFlagDirect, 
			CFSTR( kAirPlayProperty_Modes ), NULL, NULL );
		if( obj )
		{
			CFDictionarySetValue( info, CFSTR( kAirPlayProperty_Modes ), obj );
			CFRelease( obj );
		}
	}
	
	// Name
	
	*str = '\0';
	AirPlayGetDeviceName( str, sizeof( str ) );
	CFDictionarySetCString( info, CFSTR( kAirPlayKey_Name ), str, kSizeCString );
	
	// ProtocolVersion
	
	if( strcmp( kAirPlayProtocolVersionStr, "1.0" ) != 0 )
	{
		CFDictionarySetCString( info, CFSTR( kAirPlayKey_ProtocolVersion ), kAirPlayProtocolVersionStr, kSizeCString );
	}
	
	// NightMode

	obj = AirPlayReceiverServerPlatformCopyProperty( gAirPlayReceiverServer, kCFObjectFlagDirect,
		CFSTR( kAirPlayKey_NightMode ), NULL, NULL );
	if( obj )
	{
		CFDictionarySetValue( info, CFSTR( kAirPlayKey_NightMode ), obj );
		CFRelease( obj );
	}
	
	// OEMIcon
	
	if( inSession )
	{
#if( AIRPLAY_CONFIG_FILES )
		obj = NULL;
		*str = '\0';
		AirPlayGetConfigCString( CFSTR( kAirPlayProperty_OEMIconPath ), str, sizeof( str ), NULL );
		if( *str != '\0' ) obj = CFDataCreateWithFilePath( str, NULL );
		if( !obj )
#endif
		{
			obj = AirPlayReceiverServerPlatformCopyProperty( gAirPlayReceiverServer, kCFObjectFlagDirect,
				CFSTR( kAirPlayProperty_OEMIcon ), NULL, NULL );
		}
		if( obj )
		{
			CFDictionarySetValue( info, CFSTR( kAirPlayProperty_OEMIcon ), obj );
			CFRelease( obj );
		}
		
#if( AIRPLAY_CONFIG_FILES )
		obj = NULL;
		*str = '\0';
		AirPlayGetConfigCString( CFSTR( kAirPlayProperty_OEMIconLabel ), str, sizeof( str ), NULL );
		if( *str != '\0' ) CFDictionarySetCString( info, CFSTR( kAirPlayProperty_OEMIconLabel ), str, kSizeCString );
		else
#endif
		{
			obj = AirPlayReceiverServerPlatformCopyProperty( gAirPlayReceiverServer, kCFObjectFlagDirect,
				CFSTR( kAirPlayProperty_OEMIconLabel ), NULL, NULL );
		}
		if( obj )
		{
			CFDictionarySetValue( info, CFSTR( kAirPlayProperty_OEMIconLabel ), obj );
			CFRelease( obj );
		}

#if( AIRPLAY_CONFIG_FILES )
		obj = CFDictionaryGetValue( gAirPlayReceiverServer->config, CFSTR( kAirPlayProperty_OEMIconVisible ) );
		CFRetainNullSafe( obj );
		if( !obj )
#endif
		{
			obj = AirPlayReceiverServerPlatformCopyProperty( gAirPlayReceiverServer, kCFObjectFlagDirect,
															CFSTR( kAirPlayProperty_OEMIconVisible ), NULL, NULL );
		}
		if( obj )
		{
			CFDictionarySetValue( info, CFSTR( kAirPlayProperty_OEMIconVisible ), obj );
			CFRelease( obj );
		}
	}
	
	// OS Info
		
#if( AIRPLAY_CONFIG_FILES )
	obj = CFDictionaryGetValue( gAirPlayReceiverServer->config, CFSTR( kAirPlayKey_OSInfo ) );
	if( CFIsType( obj, CFString ) ) CFRetainNullSafe( obj );
	else obj = NULL;
	if( !obj )
#endif
	{
		obj = AirPlayReceiverServerPlatformCopyProperty( gAirPlayReceiverServer, kCFObjectFlagDirect,
			CFSTR( kAirPlayKey_OSInfo ), NULL, NULL );
	}
	if( obj )
	{
		CFDictionarySetValue( info, CFSTR( kAirPlayKey_OSInfo ), obj );
		CFRelease( obj );
	}

	// RightHandDrive
	
#if( AIRPLAY_CONFIG_FILES )
	obj = CFDictionaryGetValue( gAirPlayReceiverServer->config, CFSTR( kAirPlayKey_RightHandDrive ) );
	CFRetainNullSafe( obj );
	if( !obj )
#endif
	{
		obj = AirPlayReceiverServerPlatformCopyProperty( gAirPlayReceiverServer, kCFObjectFlagDirect,
			CFSTR( kAirPlayKey_RightHandDrive ), NULL, NULL );
	}
	if( obj )
	{
		CFDictionarySetValue( info, CFSTR( kAirPlayKey_RightHandDrive ), obj );
		CFRelease( obj );
	}
	
	// SettingsIcon
	
	if( inSession )
	{
		#if( AIRPLAY_CONFIG_FILES )
		obj = NULL;
		*str = '\0';
		AirPlayGetConfigCString( CFSTR( kAirPlayProperty_SettingsIconPath ), str, sizeof( str ), NULL );
		if( *str != '\0' ) obj = CFDataCreateWithFilePath( str, NULL );
		if( !obj )
		#endif
		{
			obj = AirPlayReceiverServerPlatformCopyProperty( gAirPlayReceiverServer, kCFObjectFlagDirect,
				CFSTR( kAirPlayProperty_SettingsIcon ), NULL, NULL );
		}
		if( obj )
		{
			CFDictionarySetValue( info, CFSTR( kAirPlayProperty_SettingsIcon ), obj );
			CFRelease( obj );
		}
	}
	
	// SourceVersion
	
	CFDictionarySetValue( info, CFSTR( kAirPlayKey_SourceVersion ), CFSTR( kAirPlaySourceVersionStr ) );
	
	// StatusFlags
	
	u64 = AirPlayGetStatusFlags();
	if( u64 != 0 ) CFDictionarySetInt64( info, CFSTR( kAirPlayKey_StatusFlags ), u64 );
	
	// TXT records
	
	if( inProperties )
	{
		CFRange		range;
		
		range = CFRangeMake( 0, CFArrayGetCount( inProperties ) );
		if( CFArrayContainsValue( inProperties, range, CFSTR( kAirPlayKey_TXTAirPlay ) ) )
		{
			data = NULL;
			_UpdateBonjourAirPlay( gAirPlayReceiverServer, &data );
			if( data )
			{
				CFDictionarySetValue( info, CFSTR( kAirPlayKey_TXTAirPlay ), data );
				CFRelease( data );
			}
		}
		#if( AIRPLAY_RAOP_SERVER )
		if( CFArrayContainsValue( inProperties, range, CFSTR( kAirPlayKey_TXTRAOP ) ) )
		{
			data = NULL;
			_UpdateBonjourRAOP( gAirPlayReceiverServer, &data );
			if( data )
			{
				CFDictionarySetValue( info, CFSTR( kAirPlayKey_TXTRAOP ), data );
				CFRelease( data );
			}
		}
		#endif
	}
	
	result = info;
	info = NULL;
	err = kNoErr;
	
exit:
	CFReleaseNullSafe( info );
	if( outErr ) *outErr = err;
	return( result );
}

#if( AIRPLAY_CONFIG_FILES )
//===========================================================================================================================
//	AirPlayGetConfigCString
//===========================================================================================================================
//p8.mcu_ver.custom_ver

static int get_firmware_revision(char *inBuffer, size_t inMaxLen)
{
	char buf[128];
	unsigned int custom_ver = 0;
	char mcu_ver[32];
	int fd;
#ifdef FIRMWARE_CUSTOM_VERSION
	custom_ver = (unsigned int)atoi(FIRMWARE_CUSTOM_VERSION);
#endif

	fd = open("/tmp/mcu_ver", O_RDONLY);
	if( fd >= 0 )
	{
		memset((void *)mcu_ver, 0, sizeof(mcu_ver));
		read(fd, &mcu_ver, sizeof(mcu_ver));
		close(fd);
	}
	memset((void *)buf, 0, sizeof(buf));
	sprintf(buf, "p8.%s.%u", strlen(mcu_ver)>0? mcu_ver: "010", custom_ver);
	if(strlen(buf) > inMaxLen )
		return -1;
	strcpy(inBuffer, buf);
	return 0;
}

static char *
	AirPlayGetConfigCString( 
		CFStringRef			inKey,
		char *				inBuffer, 
		size_t				inMaxLen, 
		OSStatus *			outErr )
{
	char *				result = "";
	OSStatus			err;
	
	require_action( gAirPlayReceiverServer, exit, err = kNotInitializedErr );
	
//#if( defined( AIRPLAY_FIRMWARE_REVISION ) )
	if( CFEqual( inKey, CFSTR( kAirPlayKey_FirmwareRevision ) ) )
	{
		//strlcpy( inBuffer, AIRPLAY_FIRMWARE_REVISION, inMaxLen );
		if( inMaxLen > 0 && !get_firmware_revision(inBuffer, inMaxLen)) result = inBuffer;
		err = kNoErr;
		goto exit;
	}
//#endif
#if( defined( AIRPLAY_HARDWARE_REVISION ) )
	if( CFEqual( inKey, CFSTR( kAirPlayKey_HardwareRevision ) ) )
	{
		strlcpy( inBuffer, AIRPLAY_HARDWARE_REVISION, inMaxLen );
		if( inMaxLen > 0 ) result = inBuffer;
		err = kNoErr;
		goto exit;
	}
#endif
#if( defined( AIRPLAY_MANUFACTURER ) )
	if( CFEqual( inKey, CFSTR( kAirPlayKey_Manufacturer ) ) )
	{
		strlcpy( inBuffer, AIRPLAY_MANUFACTURER, inMaxLen );
		if( inMaxLen > 0 ) result = inBuffer;
		err = kNoErr;
		goto exit;
	}
#endif
#if( defined( AIRPLAY_OEM_ICON_PATH ) )
	if( CFEqual( inKey, CFSTR( kAirPlayProperty_OEMIconPath ) ) )
	{
		strlcpy( inBuffer, AIRPLAY_OEM_ICON_PATH, inMaxLen );
		if( inMaxLen > 0 ) result = inBuffer;
		err = kNoErr;
		goto exit;
	}
#endif
#if( defined( AIRPLAY_OEM_ICON_LABEL ) )
	if( CFEqual( inKey, CFSTR( kAirPlayProperty_OEMIconLabel ) ) )
	{
		strlcpy( inBuffer, AIRPLAY_OEM_ICON_LABEL, inMaxLen );
		if( inMaxLen > 0 ) result = inBuffer;
		err = kNoErr;
		goto exit;
	}
#endif
#if( defined( AIRPLAY_SETTINGS_ICON_PATH ) )
	if( CFEqual( inKey, CFSTR( kAirPlayProperty_SettingsIconPath ) ) )
	{
		strlcpy( inBuffer, AIRPLAY_SETTINGS_ICON_PATH, inMaxLen );
		if( inMaxLen > 0 ) result = inBuffer;
		err = kNoErr;
		goto exit;
	}
#endif
	
	require_action_quiet( gAirPlayReceiverServer->config, exit, err = kNotFoundErr );
	
	result = CFDictionaryGetCString( gAirPlayReceiverServer->config, inKey, inBuffer, inMaxLen, &err );
	require_noerr_quiet( err, exit );
	
exit:
	if( outErr )		*outErr = err;
	return( result );
}

//===========================================================================================================================
//	_ProcessLimitedUIElementsConfigArray
//===========================================================================================================================

static void _ProcessLimitedUIElementsConfigArray( const void *value, void *context )
{
	// If the config came from an INI file, the array element(s) will be dictionaries, otherwise they will be strings
	CFMutableArrayRef elements = (CFMutableArrayRef) context;
	if( CFIsType( (CFTypeRef) value, CFDictionary ) )
		CFDictionaryApplyFunction( (CFDictionaryRef) value, _ProcessLimitedUIElementsConfigArrayDictionaryEntry, context );
	else if( CFIsType( (CFTypeRef) value, CFString ) )
		CFArrayAppendValue( elements, value );
}

//===========================================================================================================================
//	_ProcessLimitedUIElementsConfigArrayDictionaryEntry
//===========================================================================================================================

static void _ProcessLimitedUIElementsConfigArrayDictionaryEntry( const void *key, const void *value, void *context )
{
	// If the config came from an INI file, the dictionary key will be the element and the value will be "0" or "1"
	CFMutableArrayRef elements = (CFMutableArrayRef) context;
	if( CFIsType( (CFTypeRef) value, CFString ) && CFStringGetIntValue( (CFStringRef) value) )
		CFArrayAppendValue( elements, key );
}

#endif

//===========================================================================================================================
//	AirPlayGetDeviceID
//===========================================================================================================================

uint64_t	AirPlayGetDeviceID( uint8_t *outDeviceID )
{
	memcpy( outDeviceID, gAirPlayReceiverServer->deviceID, 6 );
	return( ReadBig48( gAirPlayReceiverServer->deviceID ) );
}

//===========================================================================================================================
//	AirPlayGetDeviceName
//===========================================================================================================================

OSStatus	AirPlayGetDeviceName( char *inBuffer, size_t inMaxLen )
{
	OSStatus		err;
	
	require_action( inMaxLen > 0, exit, err = kSizeErr );
	
	*inBuffer = '\0';
	AirPlaySettings_GetCString( NULL, CFSTR( kAirPlayPrefKey_Name ), inBuffer, inMaxLen, NULL );
#if( AIRPLAY_CONFIG_FILES )
	if( *inBuffer == '\0' ) AirPlayGetConfigCString( CFSTR( kAirPlayKey_Name ), inBuffer, inMaxLen, NULL );
#endif
#if( defined( AIRPLAY_DEVICE_NAME ) )
	if( *inBuffer == '\0' ) strlcpy( inBuffer, AIRPLAY_DEVICE_NAME, inMaxLen );	// Use hard-coded default name if provided.
#endif
	if( *inBuffer == '\0' ) GetDeviceName( inBuffer, inMaxLen );				// If no custom name, try the system name.
	if( *inBuffer == '\0' ) strlcpy( inBuffer, "AirPlay", inMaxLen );			// If no system name, use a default.
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	AirPlayGetDeviceModel
//===========================================================================================================================

static OSStatus	AirPlayGetDeviceModel( char *inBuffer, size_t inMaxLen )
{
	OSStatus		err;
	
	require_action( inMaxLen > 0, exit, err = kSizeErr );
	
	*inBuffer = '\0';
#if( AIRPLAY_CONFIG_FILES )
	AirPlayGetConfigCString( CFSTR( kAirPlayKey_Model ), inBuffer, inMaxLen, NULL );
#endif
#if( defined( AIRPLAY_DEVICE_MODEL ) )
	if( *inBuffer == '\0' ) strlcpy( inBuffer, AIRPLAY_DEVICE_MODEL, inMaxLen );	// Use hard-coded model if provided.
#endif
	if( *inBuffer == '\0' ) GetDeviceModelString( inBuffer, inMaxLen );				// If no model, try the system name.
	if( *inBuffer == '\0' ) strlcpy( inBuffer, "AirPlayGeneric1,1", inMaxLen );		// If no model, use a default.
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	AirPlayGetFeatures
//===========================================================================================================================

AirPlayFeatures	AirPlayGetFeatures( void )
{
	AirPlayFeatures		features = 0;
	OSStatus			err;
	Boolean				b;
	
	// Mark as used to avoid special case for conditionalized cases.
	
	(void) err;
	(void) b;
	
	features |= kAirPlayFeature_Audio;
	features |= kAirPlayFeature_RedundantAudio;	
#if( AIRPLAY_MFI )
	features |= kAirPlayFeature_AudioAES_128_MFi_SAPv1;
#endif
	features |= kAirPlayFeature_AudioPCM;
	features |= kAirPlayFeature_AudioAppleLossless;
	features |= kAirPlayFeature_AudioUnencrypted;
	features |= kAirPlayFeature_UnifiedBonjour;
	
	features |= AirPlayReceiverServerPlatformGetInt64( gAirPlayReceiverServer, CFSTR( kAirPlayProperty_Features ), NULL, NULL );
	return( features );
}

//===========================================================================================================================
//	AirPlayGetMinimumClientOSBuildVersion
//===========================================================================================================================

OSStatus	AirPlayGetMinimumClientOSBuildVersion( char *inBuffer, size_t inMaxLen )
{
	OSStatus	err;
	
#if( AIRPLAY_CONFIG_FILES )
	AirPlayGetConfigCString( CFSTR( kAirPlayKey_ClientOSBuildVersionMin ), inBuffer, inMaxLen, &err );
#else
	(void) inBuffer;
	(void) inMaxLen;
	err = kNotFoundErr;
#endif
	
	return( err );
}

//===========================================================================================================================
//	AirPlayGetPairingPublicKeyID
//===========================================================================================================================

//===========================================================================================================================
//	AirPlayGetStatusFlags
//===========================================================================================================================

AirPlayStatusFlags	AirPlayGetStatusFlags( void )
{
	AirPlayStatusFlags		flags = 0;
	
#if( AIRPLAY_PASSWORDS )
	if( gAirPlayReceiverServer->pinMode )
	{
		flags |= kAirPlayStatusFlag_PINMode;
	}
	else if( *gAirPlayReceiverServer->playPassword != '\0' )
	{
		flags |= kAirPlayStatusFlag_PasswordRequired;
	}
#endif
	flags |= (AirPlayStatusFlags) AirPlayReceiverServerPlatformGetInt64( gAirPlayReceiverServer, 
		CFSTR( kAirPlayProperty_StatusFlags ), NULL, NULL );
	return( flags );
}

//===========================================================================================================================
//	AirPlayReceiverServerSetLogLevel
//===========================================================================================================================

void AirPlayReceiverServerSetLogLevel( void )
{
#if( DEBUG || TARGET_OS_DARWIN )
	LogControl(
               "?AirPlayReceiverCore:level=info"
               ",AirPlayScreenServerCore:level=trace"
               ",AirPlayReceiverSessionScreenCore:level=trace"
               ",AirTunesServerCore:level=info"
               );
	dlog_control( "?DebugServices.*:level=info" );
#endif
}

//===========================================================================================================================
//	AirPlayReceiverServerSetLogPath
//===========================================================================================================================

void AirPlayReceiverServerSetLogPath( void )
{
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	AirPlayReceiverServerGetTypeID
//===========================================================================================================================

EXPORT_GLOBAL
CFTypeID	AirPlayReceiverServerGetTypeID( void )
{
	dispatch_once_f( &gAirPlayReceiverServerInitOnce, NULL, _GetTypeID );
	return( gAirPlayReceiverServerTypeID );
}

//===========================================================================================================================
//	AirPlayReceiverServerCreate
//===========================================================================================================================

#if( AIRPLAY_CONFIG_FILES )
EXPORT_GLOBAL
#else
static
#endif
OSStatus	AirPlayReceiverServerCreateWithConfigFilePath( CFStringRef inConfigFilePath, AirPlayReceiverServerRef *outServer )
{
	OSStatus						err;
	AirPlayReceiverServerRef		me;
	size_t							extraLen;
	int								i;
	uint64_t						u64;

    // Print out POSIX release information
	aprs_ulog( kLogLevelNotice, "### AirPlay Audio Receiver POSIX Release Version: %s\n",  
                      kAirPlaySourceVersionStr kAirPlayAudioReceiverPosixReleaseVersionStr );
	
	dispatch_once_f( &gAirPlayReceiverInitOnce, NULL, _GlobalInitialize );
	
	extraLen = sizeof( *me ) - sizeof( me->base );
	me = (AirPlayReceiverServerRef) _CFRuntimeCreateInstance( NULL, AirPlayReceiverServerGetTypeID(), (CFIndex) extraLen, NULL );
	require_action( me, exit, err = kNoMemoryErr );
	memset( ( (uint8_t *) me ) + sizeof( me->base ), 0, extraLen );
	
	me->overscanOverride = -1; // Default to auto.
	me->timeoutDataSecs  = kAirPlayDataTimeoutSecs;
	
	me->queue = dispatch_get_main_queue();
	dispatch_retain( me->queue );
	
	for( i = 1; i < 10; ++i )
	{
		u64 = GetPrimaryMACAddress( me->deviceID, NULL );
		if( u64 != 0 ) break;
		sleep( 1 );
	}
	
#if( AIRPLAY_CONFIG_FILES )
	if( !inConfigFilePath )
	{
		me->configFilePath = strdup( AIRPLAY_CONFIG_FILE_PATH );
	}
	else
	{
		err = CFStringCopyUTF8CString( inConfigFilePath, &me->configFilePath );
		require_noerr( err, exit );
	}
	_InitializeConfig( me );
#else
	(void) inConfigFilePath;
#endif
	
	err = AirPlayReceiverServerPlatformInitialize( me );
	require_noerr( err, exit );
	
	*outServer = me;
	gAirPlayReceiverServer = me;
	me = NULL;
	err = kNoErr;
	
exit:
	if( me ) CFRelease( me );
	return( err );
}

EXPORT_GLOBAL
OSStatus	AirPlayReceiverServerCreate( AirPlayReceiverServerRef *outServer )
{
	return AirPlayReceiverServerCreateWithConfigFilePath( NULL, outServer );
}

//===========================================================================================================================
//	AirPlayReceiverServerSetDelegate
//===========================================================================================================================

EXPORT_GLOBAL
void	AirPlayReceiverServerSetDelegate( AirPlayReceiverServerRef inServer, const AirPlayReceiverServerDelegate *inDelegate )
{
	inServer->delegate = *inDelegate;
}

//===========================================================================================================================
//	AirPlayReceiverServerGetDispatchQueue
//===========================================================================================================================

EXPORT_GLOBAL
dispatch_queue_t	AirPlayReceiverServerGetDispatchQueue( AirPlayReceiverServerRef inServer )
{
	return( inServer->queue );
}

//===========================================================================================================================
//	AirPlayReceiverServerControl
//===========================================================================================================================

EXPORT_GLOBAL
OSStatus
	AirPlayReceiverServerControl( 
		CFTypeRef			inServer, 
		uint32_t			inFlags, 
		CFStringRef			inCommand, 
		CFTypeRef			inQualifier, 
		CFDictionaryRef		inParams, 
		CFDictionaryRef *	outParams )
{
	AirPlayReceiverServerRef const		server = (AirPlayReceiverServerRef) inServer;
	OSStatus							err;
	
	if( 0 ) {}
	
#if( AIRPLAY_DACP )
	// SendDACP
	
	else if( CFEqual( inCommand, CFSTR( kAirPlayCommand_SendDACP ) ) )
	{
		if( server->serversStarted )
		{
			char *		dacpCommand;
			
			dacpCommand = CFDictionaryCopyCString( inParams, CFSTR( kAirPlayKey_DACPCommand ), &err );
			require_noerr( err, exit );
			
			AirTunesServer_ScheduleDACPCommand( dacpCommand );
			free( dacpCommand );
		}
	}
#endif
	
	// UpdateBonjour
	
	else if( CFEqual( inCommand, CFSTR( kAirPlayCommand_UpdateBonjour ) ) )
	{
		_UpdateBonjour( server );
	}
	
#if( AIRPLAY_PASSWORDS )
	// UpdatePIN
	
	else if( CFEqual( inCommand, CFSTR( kAirPlayCommand_UpdatePIN ) ) )
	{
		if( server->started && server->pinMode )
		{
			aprs_ulog( kLogLevelVerbose, "Scheduling PIN change\n" );
			
			dispatch_source_forget( &server->pinTimer );
			server->pinTimer = dispatch_source_create( DISPATCH_SOURCE_TYPE_TIMER, 0, 0, server->queue );
			require_action( server->pinTimer, exit, err = kUnknownErr );
			
			CFRetain( server );
			dispatch_set_context( server->pinTimer, server );
			dispatch_source_set_event_handler_f( server->pinTimer, _UpdatePINTimerFired );
			dispatch_source_set_cancel_handler_f( server->pinTimer, _UpdatePINTimerCanceled );
			dispatch_source_set_timer( server->pinTimer, dispatch_time_seconds( 30 ), INT64_MAX, kNanosecondsPerSecond );
			dispatch_resume( server->pinTimer );
		}
	}
#endif
	
	// PrefsChanged
	
	else if( CFEqual( inCommand, CFSTR( kAirPlayEvent_PrefsChanged ) ) )
	{
		if( server->started )
		{
			aprs_ulog( kLogLevelNotice, "Prefs changed\n" );
			_UpdatePrefs( server );
		}
	}
	
	// StartServer / StopServer
	
	else if( CFEqual( inCommand, CFSTR( kAirPlayCommand_StartServer ) ) )
	{
		server->started = true;
		_UpdatePrefs( server );
	}
	else if( CFEqual( inCommand, CFSTR( kAirPlayCommand_StopServer ) ) )
	{
		if( server->started )
		{
			server->started = false;
			_StopServers( server );
		}
	}
	
	// SessionDied
	
	else if( CFEqual( inCommand, CFSTR( kAirPlayCommand_SessionDied ) ) )
	{
		AirTunesServer_FailureOccurred( kTimeoutErr );
	}
	
	// Other
	
	else
	{
		err = AirPlayReceiverServerPlatformControl( server, inFlags, inCommand, inQualifier, inParams, outParams );
		goto exit;
	}
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	AirPlayReceiverServerCopyProperty
//===========================================================================================================================

EXPORT_GLOBAL
CFTypeRef
	AirPlayReceiverServerCopyProperty( 
		CFTypeRef	inServer, 
		uint32_t	inFlags, 
		CFStringRef	inProperty, 
		CFTypeRef	inQualifier, 
		OSStatus *	outErr )
{
	AirPlayReceiverServerRef const		server = (AirPlayReceiverServerRef) inServer;
	OSStatus							err;
	CFTypeRef							value = NULL;
	
	if( 0 ) {}
	
	// Playing
	
	else if( CFEqual( inProperty, CFSTR( kAirPlayProperty_Playing ) ) )
	{
		value = server->playing ? kCFBooleanTrue : kCFBooleanFalse;
		CFRetain( value );
	}
	
	// Source version
	
	else if( CFEqual( inProperty, CFSTR( kAirPlayProperty_SourceVersion ) ) )
	{
		value = CFSTR( kAirPlaySourceVersionStr );
		CFRetain( value );
	}
	
	// Other
	
	else
	{
		value = AirPlayReceiverServerPlatformCopyProperty( server, inFlags, inProperty, inQualifier, &err );
		goto exit;
	}
	err = kNoErr;
	
exit:
	if( outErr ) *outErr = err;
	return( value );
}

//===========================================================================================================================
//	AirPlayReceiverServerSetProperty
//===========================================================================================================================

EXPORT_GLOBAL
OSStatus
	AirPlayReceiverServerSetProperty( 
		CFTypeRef	inServer, 
		uint32_t	inFlags, 
		CFStringRef	inProperty, 
		CFTypeRef	inQualifier, 
		CFTypeRef	inValue )
{
	AirPlayReceiverServerRef const		server = (AirPlayReceiverServerRef) inServer;
	OSStatus							err;
	
	if( 0 ) {}
	
	// DeviceID
	
	else if( CFEqual( inProperty, CFSTR( kAirPlayProperty_DeviceID ) ) )
	{
		CFGetData( inValue, server->deviceID, sizeof( server->deviceID ), NULL, NULL );
	}
	
	// InterfaceName
	
	else if( CFEqual( inProperty, CFSTR( kAirPlayProperty_InterfaceName ) ) )
	{
		CFGetCString( inValue, server->ifname, sizeof( server->ifname ) );
	}
	
	// Playing
	
	else if( CFEqual( inProperty, CFSTR( kAirPlayProperty_Playing ) ) )
	{
		server->playing = CFGetBoolean( inValue, NULL );
		
		// If we updated Bonjour while we were playing and had to defer it, do it now that we've stopped playing.
		
		if( !server->playing && server->started && server->serversStarted && server->bonjourRestartPending )
		{
			_RestartBonjour( server );
		}
	}
	
	// Other
	
	else
	{
		err = AirPlayReceiverServerPlatformSetProperty( server, inFlags, inProperty, inQualifier, inValue );
		goto exit;
	}
	err = kNoErr;
	
exit:
	return( err );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	_GetTypeID
//===========================================================================================================================

static void _GetTypeID( void *inContext )
{
	(void) inContext;
	
	gAirPlayReceiverServerTypeID = _CFRuntimeRegisterClass( &kAirPlayReceiverServerClass );
	check( gAirPlayReceiverServerTypeID != _kCFRuntimeNotATypeID );
}

//===========================================================================================================================
//	_GlobalInitialize
//
//	Perform one-time initialization of things global to the entire process.
//===========================================================================================================================

static void	_GlobalInitialize( void *inContext )
{
	(void) inContext;
	
	signal( SIGPIPE, SIG_IGN ); // Ignore SIGPIPE signals so we get EPIPE errors from APIs instead of a signal.
#if( AIRPLAY_MFI )
	APSMFiPlatform_Initialize(); // Initialize at app startup to cache cert to speed up first time connect.
#endif
	
	// Setup logging.
	
	AirPlayReceiverServerSetLogLevel();
	
#if  ( TARGET_OS_POSIX && DEBUG )
	DebugIPC_EnsureInitialized( NULL, NULL );
#endif
}

#if( AIRPLAY_CONFIG_FILES )
//===========================================================================================================================
//	_InitializeConfig
//===========================================================================================================================

static OSStatus	_InitializeConfig( AirPlayReceiverServerRef inServer )
{
	OSStatus			err;
	CFDataRef			data;
	CFDictionaryRef		config = NULL;
	CFArrayRef			array;
	
	(void) inServer;
	
	// Read the config file (if it exists). Try binary plist format first since it has a unique signature to detect a
	// valid file. If it's a binary plist then parse it as an INI file and convert it to a dictionary.
	
	data = CFDataCreateWithFilePath( inServer->configFilePath, NULL );
	require_action_quiet( data, exit, err = kNotFoundErr );
	
	config = (CFDictionaryRef) CFPropertyListCreateWithData( NULL, data, kCFPropertyListImmutable, NULL, NULL );
	if( !config )
	{
		config = CFDictionaryCreateWithINIBytes( CFDataGetBytePtr( data ), (size_t) CFDataGetLength( data ), 
			kINIFlag_MergeGlobals, CFSTR( kAirPlayKey_Name ), NULL );
	}
	CFRelease( data );
	if( config && !CFIsType( config, CFDictionary ) )
	{
		dlogassert( "Bad type for config file: %s", inServer->configFilePath );
		CFRelease( config );
		config = NULL;
	}
	
	// Retrieve vendor specific audio stream options

	if( config )
	{
		inServer->audioStreamOptions = CFDictionaryGetCFDictionary( config, CFSTR( kAirPlayKey_AudioStreamOptions ), NULL );
		if( !inServer->audioStreamOptions )
		{
			// INI named sections are read into an array.
			array = CFDictionaryGetCFArray( config, CFSTR( kAirPlayKey_AudioStreamOptions ), NULL );
			if( array ) inServer->audioStreamOptions = CFArrayGetCFDictionaryAtIndex( array, 0, NULL );
		}
		CFRetainNullSafe( inServer->audioStreamOptions );
	}
	
	// Save off the config dictionary for future use.
	
	inServer->config = config;
	config = NULL;
	err = kNoErr;
	
exit:
	// There is a significant amount of code that assumes there is always a config dictionary, so create an empty one in case of error.
	if( err ) inServer->config = CFDictionaryCreate( NULL, NULL, NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	
	CFReleaseNullSafe( config );
	return( err );
}
#endif // AIRPLAY_CONFIG_FILES

//===========================================================================================================================
//	_Finalize
//===========================================================================================================================

static void	_Finalize( CFTypeRef inCF )
{
	AirPlayReceiverServerRef const		me = (AirPlayReceiverServerRef) inCF;
	
	_StopServers( me );
	AirPlayReceiverServerPlatformFinalize( me );
	dispatch_forget( &me->queue );
	ForgetCF( &me->audioStreamOptions );
#if( AIRPLAY_CONFIG_FILES )
	ForgetMem( &me->configFilePath );
	ForgetCF( &me->config );
#endif
	gAirPlayReceiverServer = NULL;
}

#if( AIRPLAY_DACP )
//===========================================================================================================================
//	_HandleDACPStatus
//===========================================================================================================================

static void	_HandleDACPStatus( OSStatus inStatus, void *inContext )
{
	AirPlayReceiverServerRef const		server = (AirPlayReceiverServerRef) inContext;
	
	AirPlayReceiverServerControlAsyncF( server, CFSTR( kAirPlayEvent_DACPStatus ), NULL, NULL, NULL, NULL, 
		"{%kO=%i}", CFSTR( kAirPlayKey_Status ), inStatus );
}
#endif

#if( AIRPLAY_PASSWORDS )
//===========================================================================================================================
//	_UpdatePINTimerFired
//===========================================================================================================================

static void	_UpdatePINTimerFired( void *inArg )
{
	_UpdatePIN( inArg, false );
}

//===========================================================================================================================
//	_UpdatePIN
//===========================================================================================================================

static void	_UpdatePIN( AirPlayReceiverServerRef const inServer, Boolean inForceUpdate )
{
	dispatch_source_forget( &inServer->pinTimer );
	if( inServer->pinMode )
	{
			(void) inForceUpdate;
		
		if( *inServer->pinFixed != '\0' )
		{
			strlcpy( inServer->playPassword, inServer->pinFixed, sizeof( inServer->playPassword ) );
		}
		else
		{
			RandomString( kDecimalDigits, sizeof_string( kDecimalDigits ), 4, 4, inServer->playPassword );
		}
		aprs_dlog( kLogLevelNotice, "Updated PIN to %s\n", inServer->playPassword );
	}
	if( inServer->serversStarted ) _UpdateServers( inServer );
}

static void	_UpdatePINTimerCanceled( void *inArg )
{
	AirPlayReceiverServerRef const		server = (AirPlayReceiverServerRef) inArg;
	
	aprs_dlog( kLogLevelNotice, "PIN canceled\n" );
	CFRelease( server );
}
#endif

//===========================================================================================================================
//	_UpdatePrefs
//===========================================================================================================================

static void	_UpdatePrefs( AirPlayReceiverServerRef inServer )
{
	OSStatus		err;
	Boolean			b;
	int				i;
	char			cstr[ 64 ];
	Boolean			restartBonjour = false;
	Boolean			updateServers  = false;
	
	AirPlaySettings_Synchronize();
	
	// Enabled. If disabled, stop the servers and skip all the rest.
	
	b = AirPlaySettings_GetBoolean( NULL, CFSTR( kAirPlayPrefKey_Enabled ), &err );
	if( err ) b = true;
	if( !b )
	{
		_StopServers( inServer );
		goto exit;
	}
	
	// BonjourDisabled
	
	b = AirPlaySettings_GetBoolean( NULL, CFSTR( kAirPlayPrefKey_BonjourDisabled ), NULL );
	if( b != inServer->bonjourDisabled )
	{
		aprs_dlog( kLogLevelNotice, "Bonjour Disabled: %s -> %s\n", !b ? "yes" : "no", b ? "yes" : "no" );
		inServer->bonjourDisabled = b;
		_UpdateBonjour( inServer );
	}
	
	// DenyInterruptions
	
	b = AirPlaySettings_GetBoolean( NULL, CFSTR( kAirPlayPrefKey_DenyInterruptions ), NULL );
	if( b != inServer->denyInterruptions )
	{
		aprs_dlog( kLogLevelNotice, "Deny Interruptions: %s -> %s\n", !b ? "yes" : "no", b ? "yes" : "no" );
		inServer->denyInterruptions = b;
		updateServers = true;
	}
	
	// Name
	
	*cstr = '\0';
	AirPlayGetDeviceName( cstr, sizeof( cstr ) );
	if( strcmp( cstr, inServer->name ) != 0 )
	{
		aprs_ulog( kLogLevelNotice, "Name changed '%s' -> '%s'\n", inServer->name, cstr );
		strlcpy( inServer->name, cstr, sizeof( inServer->name ) );
		restartBonjour = true;
		updateServers  = true;
	}
	
	// OverscanOverride
	
	i = (int) AirPlaySettings_GetInt64( NULL, CFSTR( kAirPlayPrefKey_OverscanOverride ), &err );
	if( err ) i = -1;
	if( i != inServer->overscanOverride )
	{
		aprs_dlog( kLogLevelNotice, "Overscan override: %s -> %s\n", 
			( inServer->overscanOverride  < 0 ) ? "auto" :
			( inServer->overscanOverride == 0 ) ? "no"   :
												  "yes", 
			( i  < 0 ) ? "auto" :
			( i == 0 ) ? "no"   :
						 "yes" );
		inServer->overscanOverride = i;
	}

#if( AIRPLAY_PASSWORDS )
	// PINMode
	
	b = AirPlaySettings_GetBoolean( NULL, CFSTR( kAirPlayPrefKey_PINMode ), NULL );
	if( b != inServer->pinMode )
	{
		aprs_dlog( kLogLevelNotice, "PIN mode: %s -> %s\n", !b ? "yes" : "no", b ? "yes" : "no" );
		inServer->pinMode = b;
		
		// When transitioning from password mode to PIN mode, always force the PIN to update to avoid displaying the user
		// created password when inside the grace period. And if transitioning from no-security to PIN mode, this also guards
		// against other clients connecting without the use of a PIN within the grace period.
		
		_UpdatePIN( inServer, ( inServer->pinMode ) ? true : false );
		restartBonjour = true;
		updateServers  = true;
	}
	
	// PINFixed
	
	*inServer->pinFixed = '\0';
	if( inServer->pinMode )
	{
		AirPlaySettings_GetCString( NULL, CFSTR( kAirPlayPrefKey_PINFixed ), inServer->pinFixed, 
			sizeof( inServer->pinFixed ), NULL );
		if( *inServer->pinFixed != '\0' )
		{
			aprs_dlog( kLogLevelNotice, "PIN fixed: '%s'\n", inServer->pinFixed );
		}
	}
	
	// PlayPassword
	
	if( !inServer->pinMode )
	{
		*cstr = '\0';
		AirPlaySettings_GetCString( NULL, CFSTR( kAirPlayPrefKey_PlayPassword ), cstr, sizeof( cstr ), &err );
		if( strcmp( cstr, inServer->playPassword ) != 0 )
		{
			strlcpy( inServer->playPassword, cstr, sizeof( inServer->playPassword ) );
			aprs_dlog( kLogLevelNotice, "Password changed\n" );
			restartBonjour = true;
			updateServers  = true;
		}
	}
#endif
	
	// QoSDisabled
	
	b = AirPlaySettings_GetBoolean( NULL, CFSTR( kAirPlayPrefKey_QoSDisabled ), NULL );
	if( b != inServer->qosDisabled )
	{
		aprs_dlog( kLogLevelNotice, "QoS disabled: %s -> %s\n", !b ? "yes" : "no", b ? "yes" : "no" );
		inServer->qosDisabled = b;
		updateServers = true;
	}
	
	// TimeoutData
	
	inServer->timeoutDataSecs = (int) AirPlaySettings_GetInt64( NULL, CFSTR( kAirPlayPrefKey_TimeoutDataSecs ), &err );
	if( err || ( inServer->timeoutDataSecs <= 0 ) ) inServer->timeoutDataSecs = kAirPlayDataTimeoutSecs;
	
	// Tell the platform so it can re-read any prefs it might have.
	
	AirPlayReceiverServerPlatformControl( inServer, kCFObjectFlagDirect, CFSTR( kAirPlayCommand_UpdatePrefs ), NULL, NULL, NULL );
	
	// Finally, act on any settings changes.
	
	if( !inServer->serversStarted )
	{
		_StartServers( inServer );
	}
	else
	{
		if( restartBonjour ) _RestartBonjour( inServer );
		if( updateServers )  _UpdateServers( inServer );
	}
	
exit:
	return;
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	_BonjourRegistrationHandler
//===========================================================================================================================

static void DNSSD_API
	_BonjourRegistrationHandler( 
		DNSServiceRef		inRef, 
		DNSServiceFlags		inFlags, 
		DNSServiceErrorType	inError, 
		const char *		inName,
		const char *		inType,
		const char *		inDomain,
		void *				inContext )
{
	AirPlayReceiverServerRef const		server = (AirPlayReceiverServerRef) inContext;
	DNSServiceRef *						servicePtr  = NULL;
	const char *						serviceType = "?";
	
	(void) inFlags;
	
	if( inRef == server->bonjourAirPlay )
	{
		servicePtr  = &server->bonjourAirPlay;
		serviceType = kAirPlayBonjourServiceType;
	}
#if( AIRPLAY_RAOP_SERVER )
	else if( inRef == server->bonjourRAOP )
	{
		servicePtr  = &server->bonjourRAOP;
		serviceType = kRAOPBonjourServiceType;
	}
#endif
	
	if( inError == kDNSServiceErr_ServiceNotRunning )
	{
		aprs_ulog( kLogLevelNotice, "### Bonjour server crashed for %s\n", serviceType );
		if( servicePtr ) DNSServiceForget( servicePtr );
		_UpdateBonjour( server );
	}
	else if( inError )
	{
		aprs_ulog( kLogLevelNotice, "### Bonjour registration error for %s: %#m\n", serviceType, inError );
	}
	else
	{
		aprs_ulog( kLogLevelNotice, "Registered Bonjour %s.%s%s\n", inName, inType, inDomain );
	}
}

//===========================================================================================================================
//	_RestartBonjour
//===========================================================================================================================

static void	_RestartBonjour( AirPlayReceiverServerRef inServer )
{
	inServer->bonjourRestartPending = false;
	
	// Ignore if we've been disabled.
	
	if( !inServer->started )
	{
		aprs_dlog( kLogLevelNotice, "Ignoring Bonjour restart while disabled\n" );
		goto exit;
	}
	
	// Only restart bonjour if we're not playing because some clients may stop playing if they see us go away.
	// If we're playing, just mark the Bonjour restart as pending and we'll process it when we stop playing.
	
	if( inServer->playing )
	{
		aprs_dlog( kLogLevelNotice, "Deferring Bonjour restart until we've stopped playing\n" );
		inServer->bonjourRestartPending = true;
		goto exit;
	}
	
	// Ignore if Bonjour hasn't been started.
	
	if( !inServer->bonjourAirPlay )
	{
		aprs_dlog( kLogLevelNotice, "Ignoring Bonjour restart since Bonjour wasn't started\n" );
		goto exit;
	}
	
	// Some clients stop resolves after ~2 minutes to reduce multicast traffic so if we changed something in the 
	// TXT record, such as the password state, those clients wouldn't be able to detect that state change.
	// To work around this, completely remove the Bonjour service, wait 2 seconds to give time for Bonjour to 
	// flush out the removal then re-add the Bonjour service so client will re-resolve it.
	
	_StopBonjour( inServer, "restart" );
	
	aprs_dlog( kLogLevelNotice, "Waiting for 2 seconds for Bonjour to remove service\n" );
	sleep( 2 );
	
	aprs_dlog( kLogLevelNotice, "Starting Bonjour service for restart\n" );
	_UpdateBonjour( inServer );
	
exit:
	return;
}

//===========================================================================================================================
//	_RetryBonjour
//===========================================================================================================================

static void	_RetryBonjour( void *inArg )
{
	AirPlayReceiverServerRef const		server = (AirPlayReceiverServerRef) inArg;
	
	aprs_ulog( kLogLevelNotice, "Retrying Bonjour after failure\n" );
	dispatch_source_forget( &server->bonjourRetryTimer );
	server->bonjourStartTicks = 0;
	_UpdateBonjour( server );
}

//===========================================================================================================================
//	_StopBonjour
//===========================================================================================================================

static void	_StopBonjour( AirPlayReceiverServerRef inServer, const char *inReason )
{
	dispatch_source_forget( &inServer->bonjourRetryTimer );
	inServer->bonjourStartTicks = 0;
	
	if( inServer->bonjourAirPlay )
	{
		DNSServiceForget( &inServer->bonjourAirPlay );
		aprs_ulog( kLogLevelNotice, "Deregistered Bonjour %s for %s\n", kAirPlayBonjourServiceType, inReason );
	}
#if( AIRPLAY_RAOP_SERVER )
	if( inServer->bonjourRAOP )
	{
		DNSServiceForget( &inServer->bonjourRAOP );
		aprs_ulog( kLogLevelNotice, "Deregistered Bonjour %s for %s\n", kRAOPBonjourServiceType, inReason );
	}
#endif
}

//===========================================================================================================================
//	_UpdateBonjour
//
//	Update all Bonjour services.
//===========================================================================================================================

static void	_UpdateBonjour( AirPlayReceiverServerRef inServer )
{
	OSStatus				err, firstErr = kNoErr;
	dispatch_source_t		timer;
	Boolean					activated = true;
	uint64_t				ms;
	
	if( !inServer->serversStarted || inServer->bonjourDisabled || !activated )
	{
		if( !activated )
		{
			aprs_ulog( kLogLevelNotice, "Device has not been activated. Bonjour service registration disabled.\n" );
		}

		_StopBonjour( inServer, "disable" );
		return;
	}
	if( inServer->bonjourStartTicks == 0 ) inServer->bonjourStartTicks = UpTicks();
	
	err = _UpdateBonjourAirPlay( inServer, NULL );
	if( !firstErr ) firstErr = err;
	
#if( AIRPLAY_RAOP_SERVER )
	err = _UpdateBonjourRAOP( inServer, NULL );
	if( !firstErr ) firstErr = err;
#endif
	
	if( !firstErr )
	{
		dispatch_source_forget( &inServer->bonjourRetryTimer );
	}
	else if( !inServer->bonjourRetryTimer )
	{
		ms = UpTicksToMilliseconds( UpTicks() - inServer->bonjourStartTicks );
		ms = ( ms < 11113 ) ? ( 11113 - ms ) : 1; // Use 11113 to avoid being syntonic with 10 second re-launching.
		aprs_ulog( kLogLevelNotice, "### Bonjour failed, retrying in %llu ms: %#m\n", ms, firstErr );
		
		inServer->bonjourRetryTimer = timer = dispatch_source_create( DISPATCH_SOURCE_TYPE_TIMER, 0, 0, inServer->queue );
		check( timer );
		if( timer )
		{
			dispatch_set_context( timer, inServer );
			dispatch_source_set_event_handler_f( timer, _RetryBonjour );
			dispatch_source_set_timer( timer, dispatch_time_milliseconds( ms ), INT64_MAX, kNanosecondsPerSecond );
			dispatch_resume( timer );
		}
	}
}

//===========================================================================================================================
//	_UpdateBonjourAirPlay
//
//	Update Bonjour for the _airplay._tcp service.
//===========================================================================================================================

static OSStatus	_UpdateBonjourAirPlay( AirPlayReceiverServerRef inServer, CFDataRef *outTXTRecord )
{
	OSStatus			err;
	TXTRecordRef		txtRec;
	uint8_t				txtBuf[ 256 ];
	const uint8_t *		txtPtr;
	uint16_t			txtLen;
	char				cstr[ 256 ];
	const char *		ptr;
	AirPlayFeatures		features;
	int					n;
	uint32_t			u32;
	uint16_t			port;
	DNSServiceFlags		flags;
	const char *		domain;
	uint32_t			ifindex;
	
	TXTRecordCreate( &txtRec, (uint16_t) sizeof( txtBuf ), txtBuf );
	
	// DeviceID
	
	AirPlayGetDeviceID( inServer->deviceID );
	MACAddressToCString( inServer->deviceID, cstr );
	err = TXTRecordSetValue( &txtRec, kAirPlayTXTKey_DeviceID, (uint8_t) strlen( cstr ), cstr );
	require_noerr( err, exit );
	
	// Features
	
	features = AirPlayGetFeatures();
	u32 = (uint32_t)( ( features >> 32 ) & UINT32_C( 0xFFFFFFFF ) );
	if( u32 != 0 )
	{
		n = snprintf( cstr, sizeof( cstr ), "0x%X,0x%X", (uint32_t)( features & UINT32_C( 0xFFFFFFFF ) ), u32 );
	}
	else
	{
		n = snprintf( cstr, sizeof( cstr ), "0x%X", (uint32_t)( features & UINT32_C( 0xFFFFFFFF ) ) );
	}
	err = TXTRecordSetValue( &txtRec, kAirPlayTXTKey_Features, (uint8_t) n, cstr );
	require_noerr( err, exit );
	
	// Flags
	
	u32 = AirPlayGetStatusFlags();
	if( u32 != 0 )
	{
		n = snprintf( cstr, sizeof( cstr ), "0x%x", u32 );
		err = TXTRecordSetValue( &txtRec, kAirPlayTXTKey_Flags, (uint8_t) n, cstr );
		require_noerr( err, exit );
	}
	
	// FirmwareRevision
	
	*cstr = '\0';
#if( AIRPLAY_CONFIG_FILES )
	AirPlayGetConfigCString( CFSTR( kAirPlayKey_FirmwareRevision ), cstr, sizeof( cstr ), NULL );
	if( *cstr == '\0' )
#endif
	{
		AirPlayReceiverServerPlatformGetCString( gAirPlayReceiverServer, CFSTR( kAirPlayKey_FirmwareRevision ), NULL, 
			cstr, sizeof( cstr ), NULL );
	}
	if( *cstr != '\0' )
	{
		err = TXTRecordSetValue( &txtRec, kAirPlayTXTKey_FirmwareVersion, (uint8_t) strlen( cstr ), cstr );
		require_noerr( err, exit );
	}
	
	// Model
	
	*cstr = '\0';
	AirPlayGetDeviceModel( cstr, sizeof( cstr ) );
	if( *cstr != '\0' )
	{
		err = TXTRecordSetValue( &txtRec, kAirPlayTXTKey_Model, (uint8_t) strlen( cstr ), cstr );
		require_noerr( err, exit );
	}
	
#if( AIRPLAY_PASSWORDS )
	// Password
	
	if( !inServer->pinMode && inServer->playPassword && ( *inServer->playPassword != '\0' ) )
	{
		err = TXTRecordSetValue( &txtRec, kAirPlayTXTKey_Password, 1, "1" );
		require_noerr( err, exit );
	}
	
	// PIN Mode
	
	if( inServer->pinMode )
	{
		err = TXTRecordSetValue( &txtRec, kAirPlayTXTKey_PIN, 1, "1" );
		require_noerr( err, exit );
	}
#endif
	
	// Protocol version
	
	ptr = kAirPlayProtocolVersionStr;
	if( strcmp( ptr, "1.0" ) != 0 )
	{
		err = TXTRecordSetValue( &txtRec, kAirPlayTXTKey_ProtocolVersion, (uint8_t) strlen( ptr ), ptr );
		require_noerr( err, exit );
	}
	
	// Source Version
	
	ptr = kAirPlaySourceVersionStr kAirPlayAudioReceiverPosixReleaseVersionStr;
	err = TXTRecordSetValue( &txtRec, kAirPlayTXTKey_SourceVersion, (uint8_t) strlen( ptr ), ptr );
	require_noerr( err, exit );
	
	// If we're only building the TXT record then return it and exit without registering.
	
	txtPtr = TXTRecordGetBytesPtr( &txtRec );
	txtLen = TXTRecordGetLength( &txtRec );
	if( outTXTRecord )
	{
		CFDataRef		data;
		
		data = CFDataCreate( NULL, txtPtr, txtLen );
		require_action( data, exit, err = kNoMemoryErr );
		*outTXTRecord = data;
		goto exit;
	}
	
	// Update the service.
	
	if( inServer->bonjourAirPlay )
	{
		err = DNSServiceUpdateRecord( inServer->bonjourAirPlay, NULL, 0, txtLen, txtPtr, 0 );
		if( !err ) aprs_ulog( kLogLevelNotice, "Updated Bonjour TXT for %s\n", kAirPlayBonjourServiceType );
		else
		{
			// Update record failed or isn't supported so deregister to force a re-register below.
			
			aprs_ulog( kLogLevelNotice, "Bonjour TXT update failed for %s, deregistering so we can force a re-register %s\n",
					  kAirPlayBonjourServiceType );
			DNSServiceForget( &inServer->bonjourAirPlay );
		}
	}
	if( !inServer->bonjourAirPlay )
	{
		
		ptr = NULL;
			*cstr = '\0';
			AirPlayGetDeviceName( cstr, sizeof( cstr ) );
			ptr = cstr;
		
		flags = 0;
		domain = kAirPlayBonjourServiceDomain;
		
		port = (uint16_t) AirTunesServer_GetListenPort();
		require_action( port > 0, exit, err = kInternalErr );
		
		ifindex = kDNSServiceInterfaceIndexAny;
		if( *inServer->ifname != '\0' )
		{
			ifindex = if_nametoindex( inServer->ifname );
			require_action_quiet( ifindex != 0, exit, err = kNotFoundErr );
		}
		err = DNSServiceRegister( &inServer->bonjourAirPlay, flags, ifindex, ptr, kAirPlayBonjourServiceType, domain, NULL, 
			htons( port ), txtLen, txtPtr, _BonjourRegistrationHandler, inServer );
		require_noerr_quiet( err, exit );
		
		aprs_ulog( kLogLevelNotice, "Registering Bonjour %s port %d\n", kAirPlayBonjourServiceType, port );
	}
	
exit:
	TXTRecordDeallocate( &txtRec );
	return( err );
}

#if( AIRPLAY_RAOP_SERVER )
//===========================================================================================================================
//	_UpdateBonjourRAOP
//
//	Update Bonjour for the _raop._tcp server.
//===========================================================================================================================

static OSStatus	_UpdateBonjourRAOP( AirPlayReceiverServerRef inServer, CFDataRef *outTXTRecord )
{
	OSStatus			err;
	TXTRecordRef		txtRec;
	uint8_t				txtBuf[ 256 ];
	const uint8_t *		txtPtr;
	uint16_t			txtLen;
	char				str[ 256 ];
	const char *		ptr;
	size_t				len;
	uint32_t			u32;
	int					n;
	AirPlayFeatures		features;
	uint8_t				deviceID[ 6 ];
	const uint8_t *		a;
	uint16_t			port;
	uint32_t			flags;
	const char *		domain;
	uint32_t			ifindex;
	
	TXTRecordCreate( &txtRec, (uint16_t) sizeof( txtBuf ), txtBuf );
	
	// CompressionTypes
	
	u32  = kAirPlayCompressionType_PCM;
	u32 |= kAirPlayCompressionType_ALAC;
	BitListString_Make( u32, str, &len );
	err = TXTRecordSetValue( &txtRec, kRAOPTXTKey_CompressionTypes, (uint8_t) len, str );
	require_noerr( err, exit );
	
	// Digest Auth from RFC-2617 (i.e. use lowercase hex digits)
	
	err = TXTRecordSetValue( &txtRec, kRAOPTXTKey_RFC2617DigestAuth, sizeof_string( "true" ), "true" );
	require_noerr( err, exit );
	
	// Encryption Types
	
	u32  = kAirPlayEncryptionType_None;
#if( AIRPLAY_MFI )
	u32 |= kAirPlayEncryptionType_MFi_SAPv1;
#endif
	BitListString_Make( u32, str, &len );
	err = TXTRecordSetValue( &txtRec, kRAOPTXTKey_EncryptionTypes, (uint8_t) len, str );
	require_noerr( err, exit );
	
	// Features
	
	features = AirPlayGetFeatures();
	u32 = (uint32_t)( ( features >> 32 ) & UINT32_C( 0xFFFFFFFF ) );
	if( u32 != 0 )
	{
		len = (size_t) snprintf( str, sizeof( str ), "0x%X,0x%X", (uint32_t)( features & UINT32_C( 0xFFFFFFFF ) ), u32 );
	}
	else
	{
		len = (size_t) snprintf( str, sizeof( str ), "0x%X", (uint32_t)( features & UINT32_C( 0xFFFFFFFF ) ) );
	}
	err = TXTRecordSetValue( &txtRec, kRAOPTXTKey_Features, (uint8_t) len, str );
	require_noerr( err, exit );
	
	// FirmwareRevision
	
	*str = '\0';
#if( AIRPLAY_CONFIG_FILES )
	AirPlayGetConfigCString( CFSTR( kAirPlayKey_FirmwareRevision ), str, sizeof( str ), NULL );
	if( *str == '\0' )
#endif
	{
		AirPlayReceiverServerPlatformGetCString( gAirPlayReceiverServer, CFSTR( kAirPlayKey_FirmwareRevision ), NULL, 
			str, sizeof( str ), NULL );
	}
	if( *str != '\0' )
	{
		err = TXTRecordSetValue( &txtRec, kRAOPTXTKey_FirmwareSourceVersion, (uint8_t) strlen( str ), str );
		require_noerr( err, exit );
	}
	
	// MetaDataTypes -- Deprecated as of Mac OS X 10.9 and iOS 7.0 and later, but needed for older senders.
	
	u32 = 0;
	if( features & kAirPlayFeature_AudioMetaDataArtwork )	u32 |= kAirTunesMetaDataType_Artwork;
	if( features & kAirPlayFeature_AudioMetaDataProgress )	u32 |= kAirTunesMetaDataType_Progress;
	if( features & kAirPlayFeature_AudioMetaDataText )		u32 |= kAirTunesMetaDataType_Text;
	if( u32 != 0 )
	{
		BitListString_Make( u32, str, &len );
		err = TXTRecordSetValue( &txtRec, kRAOPTXTKey_MetaDataTypes, (uint8_t) len, str );
		require_noerr( err, exit );
	}
	
	// Model
	
	*str = '\0';
	AirPlayGetDeviceModel( str, sizeof( str ) );
	err = TXTRecordSetValue( &txtRec, kRAOPTXTKey_AppleModel, (uint8_t) strlen( str ), str );
	require_noerr( err, exit );
	
#if( AIRPLAY_PASSWORDS )
	// PasswordRequired
	
	if( !inServer->pinMode && ( *inServer->playPassword != '\0' ) )
	{
		err = TXTRecordSetValue( &txtRec, kRAOPTXTKey_Password, sizeof_string( "true" ), "true" );
		require_noerr( err, exit );
	}
#endif
	
	// StatusFlags
	
	u32 = AirPlayGetStatusFlags();
	if( u32 != 0 )
	{
		n = snprintf( str, sizeof( str ), "0x%x", u32 );
		err = TXTRecordSetValue( &txtRec, kRAOPTXTKey_StatusFlags, (uint8_t) n, str );
		require_noerr( err, exit );
	}
	
	// TransportTypes (audio)
	
	err = TXTRecordSetValue( &txtRec, kRAOPTXTKey_TransportTypes, 3, "UDP" );
	require_noerr( err, exit );
	
	// Version (protocol)
	
	n = snprintf( str, sizeof( str ), "%u", 0x00010001 ); // 1.1
	err = TXTRecordSetValue( &txtRec, kRAOPTXTKey_ProtocolVersion, (uint8_t) n, str );
	require_noerr( err, exit );
	
	// Version (source)
	
	ptr = kAirPlaySourceVersionStr kAirPlayAudioReceiverPosixReleaseVersionStr;
	err = TXTRecordSetValue( &txtRec, kRAOPTXTKey_SourceVersion, (uint8_t) strlen( ptr ), ptr );
	require_noerr( err, exit );
	
	// If we're only building the TXT record then return it and exit without registering.
	
	txtPtr = TXTRecordGetBytesPtr( &txtRec );
	txtLen = TXTRecordGetLength( &txtRec );
	if( outTXTRecord )
	{
		CFDataRef		data;
		
		data = CFDataCreate( NULL, txtPtr, txtLen );
		require_action( data, exit, err = kNoMemoryErr );
		*outTXTRecord = data;
		goto exit;
	}
	
	// Update the service.
	
	if( inServer->bonjourRAOP )
	{
		err = DNSServiceUpdateRecord( inServer->bonjourRAOP, NULL, 0, txtLen, txtPtr, 0 );
		if( !err ) aprs_ulog( kLogLevelNotice, "Updated Bonjour TXT for %s\n", kRAOPBonjourServiceType );
		else
		{
			// Update record failed or isn't supported so deregister to force a re-register below.
			
			DNSServiceForget( &inServer->bonjourRAOP );
		}
	}
	if( !inServer->bonjourRAOP )
	{
		// Publish the service. The name is in the format <mac address>@<speaker name> such as "001122AABBCC@My Device".
		
		AirPlayGetDeviceID( deviceID );
		a = deviceID;
		n = snprintf( str, sizeof( str ), "%02X%02X%02X%02X%02X%02X@", a[ 0 ], a[ 1 ], a[ 2 ], a[ 3 ], a[ 4 ], a[ 5 ] );
		err = AirPlayGetDeviceName( &str[ n ], sizeof( str ) - n );
		require_noerr( err, exit );
		
		flags = 0;
		domain = kRAOPBonjourServiceDomain;
		
		ifindex = kDNSServiceInterfaceIndexAny;
		if( *inServer->ifname != '\0' )
		{
			ifindex = if_nametoindex( inServer->ifname );
			require_action_quiet( ifindex != 0, exit, err = kNotFoundErr );
		}
		port = (uint16_t) AirTunesServer_GetListenPort();
		err = DNSServiceRegister( &inServer->bonjourRAOP, flags, ifindex, str, kRAOPBonjourServiceType, domain, NULL, 
			htons( port ), txtLen, txtPtr, _BonjourRegistrationHandler, inServer );
		require_noerr_quiet( err, exit );
		
		aprs_ulog( kLogLevelNotice, "Registering Bonjour %s.%s port %u\n", str, kRAOPBonjourServiceType, port );
	}
	
exit:
	TXTRecordDeallocate( &txtRec );
	return( err );
}
#endif // AIRPLAY_RAOP_SERVER

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	_StartServers
//===========================================================================================================================

static void	_StartServers( AirPlayReceiverServerRef inServer )
{
	OSStatus		err;
	
	DEBUG_USE_ONLY( err );
	
	if( inServer->serversStarted ) return;
	
	// Create the servers first.
	
	err = AirTunesServer_EnsureCreated();
	check_noerr( err );
#if( AIRPLAY_DACP )
	AirTunesServer_SetDACPCallBack( _HandleDACPStatus, inServer );
#endif
	
	// After all the servers are created, apply settings then start them.
	
	_UpdateServers( inServer );
	AirTunesServer_Start();
	inServer->serversStarted = true;
	_UpdateBonjour( inServer );
	
	AirPlayReceiverServerPlatformControl( inServer, kCFObjectFlagDirect, CFSTR( kAirPlayCommand_StartServer ), 
		NULL, NULL, NULL );
	
	aprs_ulog( kLogLevelNotice, "AirPlay servers started\n" );
}

//===========================================================================================================================
//	_StopServers
//===========================================================================================================================

static void	_StopServers( AirPlayReceiverServerRef inServer )
{
#if( AIRPLAY_PASSWORDS )
	dispatch_source_forget( &inServer->pinTimer );
#endif
	
	_StopBonjour( inServer, "stop" );
	if( inServer->serversStarted )
	{
		AirPlayReceiverServerPlatformControl( inServer, kCFObjectFlagDirect, CFSTR( kAirPlayCommand_StopServer ), 
			NULL, NULL, NULL );
	}
	AirTunesServer_EnsureDeleted();
	if( inServer->serversStarted )
	{
		aprs_ulog( kLogLevelNotice, "AirPlay servers stopped\n" );
		inServer->serversStarted = false;
	}
}

//===========================================================================================================================
//	_UpdateServers
//===========================================================================================================================

static void	_UpdateServers( AirPlayReceiverServerRef inServer )
{
	AirTunesServer_SetDenyInterruptions( inServer->denyInterruptions );
	if( inServer->serversStarted )
	{
		_UpdateBonjour( inServer );
	}
}
