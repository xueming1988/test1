/*
    File:    PlatformImplementationNetBSD.c
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
    
    Copyright (C) 2012-2013 Apple Inc. All Rights Reserved.
*/

#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/types.h>
#include <ifaddrs.h>

#include "CommonServices.h"     // For OSStatus
#include "TickUtils.h"
#include "RandomNumberUtils.h"
#include "NetUtils.h"
#include "SystemUtils.h"
#include "CFUtils.h"
#include "CFCompat.h"
#include "AirPlaySettings.h"
#include "AirPlayReceiverServer.h"

#include "PlatformInternal.h"

#define WIFI_INTERFACE_NAME     "wlan0"


uint64_t    UpTicks( void )
{
	uint64_t			nanos;
	struct timespec		ts;
    
    ts.tv_sec  = 0;
    ts.tv_nsec = 0;
    clock_gettime( CLOCK_MONOTONIC, &ts );
    nanos = ts.tv_sec;
    nanos *= 1000000000;
    nanos += ts.tv_nsec;
    return( nanos );
}

uint64_t    UpTicksPerSecond( void )
{
    return(1000000000ULL);
}


//===========================================================================================================================
//	RandomBytes
//===========================================================================================================================

OSStatus	RandomBytes( void *inBuffer, size_t inByteCount )
{
	static int		sRandomFD = -1;
	uint8_t *		dst;
	ssize_t			n;
    
    while( sRandomFD < 0 )
    {
        sRandomFD = open( "/dev/urandom", O_RDONLY );
        if( sRandomFD < 0 ) { perror( "open urandom error: "); sleep( 1 ); continue; }
        break;
    }
    dst = (uint8_t *) inBuffer;
    while( inByteCount > 0 )
    {
        n = read( sRandomFD, dst, inByteCount );
        if( n < 0 ) { perror( "read urandom error: "); sleep( 1 ); continue; }
        dst += n;
        inByteCount -= n;
    }
    return( kNoErr );
}

OSStatus	GetPrimaryMACAddressPlatform( uint8_t outMAC[ 6 ] )
{
	OSStatus					err = kNotFoundErr;
    int                         ret;
	struct ifaddrs *			iaList;
	const struct ifaddrs *		ia;
    
    iaList = NULL;

    /* Use wifi interface mac address if available */
    if ( kNoErr == GetInterfaceMACAddress( WIFI_INTERFACE_NAME, outMAC ))
    {
        printf("%s(): Using WiFi MAC=%02x:%02x:%02x:%02x:%02x:%02x\n", __FUNCTION__,
		    outMAC[0], outMAC[1], outMAC[2], outMAC[3], outMAC[4], outMAC[5]);

        goto exit;
    }

    /* no wifi mac address found. So find an interface avaialble */
    ret = getifaddrs( &iaList );
    if ( ret == -1)     goto exit;
    
    for( ia = iaList; ia; ia = ia->ifa_next )
    {
        const struct sockaddr_dl *      sdl;
        
        if( !( ia->ifa_flags & IFF_UP ) )           continue; // Skip inactive.
        if( !ia->ifa_addr )                         continue; // Skip no addr.
        if( ia->ifa_addr->sa_family != AF_LINK )    continue; // Skip non-AF_LINK.
        sdl = (const struct sockaddr_dl *) ia->ifa_addr;
        if( sdl->sdl_alen != 6 )                    continue; // Skip wrong length.
        
        memcpy( outMAC, LLADDR( sdl ), 6 );
        break;
    }
    if ( ia == NULL ) goto exit;

    err = kNoErr;
    
exit:
    if( iaList ) freeifaddrs( iaList );
    return( err );
}

OSStatus    GetInterfaceMACAddress( const char *inInterfaceName, uint8_t *outMACAddress )
{
    OSStatus                err = kUnknownErr;
    int                     ret;
    struct ifaddrs *        iflist = NULL;
    struct ifaddrs *        ifa;
    
    ret = getifaddrs( &iflist );
    if ( ret == -1)     goto exit;
    
    for( ifa = iflist; ifa; ifa = ifa->ifa_next )
    {
        if( ( ifa->ifa_addr->sa_family == AF_LINK ) && ( strcmp( ifa->ifa_name, inInterfaceName ) == 0 ) )
        {
            const struct sockaddr_dl * const        sdl = (const struct sockaddr_dl *) ifa->ifa_addr;
            
            if( sdl->sdl_alen == 6 )
            {
                memcpy( outMACAddress, &sdl->sdl_data[ sdl->sdl_nlen ], 6 );
                err = kNoErr;
                goto exit;
            }
        }
    }
    
exit:
    if( iflist ) freeifaddrs( iflist );
    return( err );
}

char *	GetDeviceName( char *inBuf, size_t inMaxLen )
{
    char *name;

    if (( inMaxLen <= 0 ) || ( inBuf == NULL )) return( "" );

            inBuf[ 0 ] = '\0';
    name = getFriendlyName();

    if ( name )
    {
        strncpy( inBuf, name, inMaxLen);
        inBuf[ inMaxLen - 1 ] = '\0';
    }

        return( inBuf );
}

char *	GetDeviceModelString( char *inBuf, size_t inMaxLen )
{
    if ( (inBuf == NULL) || (inMaxLen <= 1) ) return( "" );

    strncpy( inBuf, "PosixPi", inMaxLen - 1 );
    inBuf[inMaxLen - 1] = '\0';

    return( inBuf );
}

int GetPlatformMaxSocketBufferSize( int direction )
{
	const char *		path;
	FILE *				file;
	int					n;
    int                 value = 0;
    
    value = 0;
    path = ( direction ) ? "/proc/sys/net/core/rmem_max" : "/proc/sys/net/core/wmem_max";
    file = fopen( path, "r" );
    check( file );
    if( file )
    {
        n = fscanf( file, "%u", &value );
        if( n != 1 ) value = 0;
        fclose( file );
    }
	if( value <= 0 ) value = 256 * 1024;	// Default to 256 KB.

    return( value );
}

CFPropertyListRef   CFPreferencesCopyAppValue_compat( CFStringRef inKey, CFStringRef inAppID )
{
    CFPropertyListRef    result = NULL;
    char *                key = NULL;
    
    (void) inAppID;
    
    key = CFCopyCString( inKey, NULL );
    require( key, exit );
        
    if (0) {}

    // Password

    else if ( CFEqual(inKey, CFSTR(kAirPlayPrefKey_PlayPassword)) )
    {
        char *passwd = getPlayPassword();

        // return password here
        if ( passwd )
        {
            result = CFStringCreateWithCString( NULL, passwd, kCFStringEncodingUTF8 );
        }
    }

    // Unknown

    else
    {
        // ignore unknown keys
    }

exit:
    if( key )       free( key );

    return( result );
}



void    CFPreferencesSetAppValue_compat( CFStringRef inKey, CFPropertyListRef inValue, CFStringRef inAppID )
{
    char *        key = NULL;
    char *        value = NULL;
    
    (void) inAppID;
    
    key = CFCopyCString( inKey, NULL );
    require( key, exit );
    
    if ( inValue != NULL )
    {
        value = CFCopyCString( inValue, NULL );
        require( value, exit );
    }


    if (0) {}


    // Password

    else if ( CFEqual(inKey, CFSTR(kAirPlayPrefKey_PlayPassword)) )
    {
        if ( value == NULL )
        {
            // reset password here 
        }
        else
        {
            // save specified passwod "value" here 
        }
    }

    // Unknown

    else
    {
        // ignore unknown keys
    }

exit:
    if( key )       free( key );
    if( value )     free( value );
}



Boolean CFPreferencesAppSynchronize_compat( CFStringRef inAppID )
{
    (void) inAppID;
            
    return( true );
}


