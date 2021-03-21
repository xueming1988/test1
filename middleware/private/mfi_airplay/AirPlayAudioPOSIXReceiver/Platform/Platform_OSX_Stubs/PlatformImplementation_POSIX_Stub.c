/*
	File:    PlatformImplementation_POSIX_Stub.c
	Package: AirPlayAudioPOSIXReceiver
	Version: AirPlayAudioPOSIXReceiver-190.9.p6
	
	Disclaimer: IMPORTANT: This Apple software is supplied to you, by Apple Inc. ("Apple"), in your
	capacity as a current, and in good standing, Licensee in the MFi Licensing Program. Use of this
	Apple software is governed by and subject to the terms and conditions of your MFi License,
	including, but not limited to, the restrictions specified in the provision entitled ?ùPublic
	Software?? and is further subject to your agreement to the following additional terms, and your
	agreement that the use, installation, modification or redistribution of this Apple software
	constitutes acceptance of these additional terms. If you do not agree with these additional terms,
	please do not use, install, modify or redistribute this Apple software.
	
	Subject to all of these terms and in?consideration of your agreement to abide by them, Apple grants
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
	fixes or enhancements to Apple in connection with this software (?úFeedback??, you hereby grant to
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
//#include <openssl/rand.h> //For RAND_bytes function, modified by Sean
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <sys/ioctl.h>
#include <net/if.h> 
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>

#include "CommonServices.h"     // For OSStatus
#include "TickUtils.h"
#include "RandomNumberUtils.h"
#include "NetUtils.h"
#include "SystemUtils.h"
#include "CFUtils.h"
#include "CFCompat.h"
#include "AirPlaySettings.h"

#define INFO_DEVICENAME_FILE_PATH	"/tmp/airplay_name"
#define INFO_PASSWORD_FILE_PATH		"/tmp/airplay_password"
#define INFO_DEVICENAME_DEFAULT		"iMuzo WiFi Speaker"
#define INFO_PASSWORD_DEFAULT "123456"
#define INFO_MODULENAME_FILE_PATH	"/tmp/module_info"

#ifdef PLATFORM_MT7688
#define INFO_MODULENAME_DEFAULT		"Wiimu-A31"
#elif defined(PLATFORM_MT7620)
#define INFO_MODULENAME_DEFAULT		"Wiimu-A21"
#elif defined(PLATFORM_INGENIC)
#define INFO_MODULENAME_DEFAULT		"Wiimu-A76"
#endif

//===========================================================================================================================
//	UpTicks
//===========================================================================================================================

uint64_t    UpTicks( void )
{
	uint64_t			nanos = 0;
			
    /* 
     * PLATFORM_TO_DO:
     * 
     *  - Call platform's get tick value function (ideally nano second accurate) here. 
     *  - Return it as a 64bit value.
     */
   
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  nanos = (uint64_t) ( ((uint64_t)now.tv_sec)*1000000ULL + ((uint64_t)(now.tv_nsec+500ULL)/1000ULL));
  return (nanos);

}



//===========================================================================================================================
//	UpTicksPerSecond
//===========================================================================================================================

uint64_t    UpTicksPerSecond( void )
{
    /* 
     * PLATFORM_TO_DO:
     *
     *   - Return appropriate value for the platform being ported to.
     */
   
    uint64_t tickPerSec = 1000000ULL; // 1us
    return(tickPerSec);
   
}



//===========================================================================================================================
//	RandomBytes
//===========================================================================================================================

OSStatus	RandomBytes( void * inBuffer, size_t inByteCount )
{
   
	struct timeval tm;
	uint32_t seedValue=0;
  uint32_t i =0;
	char tmp=0;
	char *tmpBuf=(void*)inBuffer;
	
	for(i=0; i<inByteCount; i++) {
		gettimeofday(&tm, NULL);
		seedValue = (uint32_t)(tm.tv_sec + tm.tv_usec);
		srand(seedValue); 
		tmp = (char)(rand()%256);
		memset(tmpBuf+i, tmp, sizeof(char));
	}
	if( inByteCount > 0 ) {
		//printf("[APPLE]RandomBytes() : %s \n", tmpBuf);  
	}
    
	return( kNoErr );
}



//===========================================================================================================================
//	GetPrimaryMACAddressPlatform
//===========================================================================================================================


OSStatus	GetPrimaryMACAddressPlatform( uint8_t outMAC[ 6 ] )
{
	OSStatus        err = kNotFoundErr;   
    
  struct ifreq ifr;
    struct ifconf ifc;
    char buf[1024];
    int success = 0;

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock == -1) { /* handle error*/ };

    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    if (ioctl(sock, SIOCGIFCONF, &ifc) == -1) { /* handle error */ }

    struct ifreq* it = ifc.ifc_req;
    const struct ifreq* const end = it + (ifc.ifc_len / sizeof(struct ifreq));

    for (; it != end; ++it) {
        strcpy(ifr.ifr_name, it->ifr_name);
        if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0) {
            if (! (ifr.ifr_flags & IFF_LOOPBACK)) { // don't count loopback
                if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0) {
                    success = 1;
                    break;
                }
            }
        }
        else { /* handle error */ }
    }

    

    if (success) memcpy(outMAC, ifr.ifr_hwaddr.sa_data, 6);
    else
    	printf("macaddr fail!!\n");

    /* 
     * PLATFORM_TO_DO:
     *
     *   - call platform's get interface list function
     *   - For the primary interfacem, do the memcpy below
     */
        
    err = kNoErr;
	
	return( err );
}



//===========================================================================================================================
//	GetInterfaceMACAddress
//===========================================================================================================================

OSStatus	GetInterfaceMACAddress( const char *inInterfaceName, uint8_t *outMACAddress )
{
	OSStatus		err = kUnknownErr;
 

  (void) inInterfaceName;
	
    /* 
     * PLATFORM_TO_DO:
     *
     *   - call platform's interface function to find the incoming interface name's mac address
     *   - Do memcpy below
     */
  int s;
  struct ifreq buffer;
	

	s = socket(PF_INET, SOCK_DGRAM, 0);

	memset(&buffer, 0x00, sizeof(buffer));
	
	strcpy(buffer.ifr_name, inInterfaceName);

	ioctl(s, SIOCGIFHWADDR, &buffer);

	close(s);
	
	memcpy( outMACAddress, buffer.ifr_hwaddr.sa_data, 6 );

	
  
    err = kNoErr;
	
	return( err );
}



//===========================================================================================================================
//	GetDeviceName
//===========================================================================================================================

char *	GetDeviceName( char *inBuf, size_t inMaxLen )
{
	FILE *fp=NULL;
	char buf[512];
	if( inMaxLen > 0 )
	{
        /* 
         * PLATFORM_TO_DO:
         *
         *   - Copy the accessory's name to the specified buffer.
         */
	         if(access( INFO_DEVICENAME_FILE_PATH , F_OK) == 0) 
		{
		    	fp = fopen(INFO_DEVICENAME_FILE_PATH, "r+");
		    	if (fp == NULL){
		    	  	strncpy( inBuf, INFO_DEVICENAME_DEFAULT, inMaxLen);			
				return( inBuf );
			}else{
				memset((void *)buf, 0, sizeof(buf));
				if(fgets(buf, sizeof buf, fp)){
					sprintf(inBuf, "%s", buf);
				}
				fclose(fp);
				if( strlen(buf) <= 0 )
					strncpy( inBuf, INFO_DEVICENAME_DEFAULT, inMaxLen);	
				return( inBuf );
			}
		}else{
			strncpy( inBuf, INFO_DEVICENAME_DEFAULT, inMaxLen);			
			return( inBuf );
		}	
	}
	return( NULL );
}



//===========================================================================================================================
//	GetDeviceModelString
//===========================================================================================================================

char *	GetDeviceModelString( char *inBuf, size_t inMaxLen )
{
	FILE *fp=NULL;
	char buf[512];

    if ( (inBuf == NULL) || (inMaxLen <= 1) ) return( "" );

    /* 
     * PLATFORM_TO_DO:
     *
     *   - Copy the accessory's model string to the specified buffer.
     */
     if(access( INFO_MODULENAME_FILE_PATH , F_OK) == 0) 
	{
		fp = fopen(INFO_MODULENAME_FILE_PATH, "r+");
		if (fp == NULL){
			strncpy( inBuf, INFO_MODULENAME_DEFAULT, inMaxLen);			
			return( inBuf );
		}else{
			if(fgets(buf, sizeof buf, fp)){
				sprintf(inBuf, "%s", buf);
			}
			fclose(fp);
			return( inBuf );
		}
	}else{
			strncpy( inBuf, INFO_MODULENAME_DEFAULT, inMaxLen);			
			return( inBuf );
	}	

	return( NULL );
}


//===========================================================================================================================
//	GetPlatformMaxSocketBufferSize
//===========================================================================================================================

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
	//printf("==%s(%d) = %d KB\n", __FUNCTION__, direction, value/1024);
	return( value );
}


//===========================================================================================================================
//	CFPreferencesCopyAppValue_compat
//===========================================================================================================================

CFPropertyListRef   CFPreferencesCopyAppValue_compat( CFStringRef inKey, CFStringRef inAppID )
{
    CFPropertyListRef    result = NULL;
    char *                key = NULL;
    char Password[128]={0};
    FILE *fp;
    
    (void) inAppID;
    
    key = CFCopyCString( inKey, NULL );
    require( key, exit );
        
    if (0) {}

    // Password

    else if ( CFEqual(inKey, CFSTR(kAirPlayPrefKey_PlayPassword)) )
    {

        /* 
         * PLATFORM_TO_DO:
         *
         *   - return password here.
         */
    		if(access( INFO_PASSWORD_FILE_PATH , F_OK) == 0) 
    		{
    				fp = fopen(INFO_PASSWORD_FILE_PATH, "r+");
    				if (fp == NULL){
    	  				printf("Error Reading File\n");
    					//set to default password
    					result = CFStringCreateWithCString( NULL, INFO_PASSWORD_DEFAULT, kCFStringEncodingUTF8 );
						if( key ) free( key );
						return result;
    				}  
    				fscanf(fp,"%s",Password);
    				printf("password %s\n",Password);
					fclose(fp);
    				result = CFStringCreateWithCString( NULL, Password, kCFStringEncodingUTF8 );
    				if( key ) free( key );	
    				return( result );
				}
    			
        // result = CFStringCreateWithCString( NULL, "password", kCFStringEncodingUTF8 );
    }

    // Unknown

    else
    {
    	printf("=============== !!!! %s [%s] =================\n", __FUNCTION__, key);
        // ignore unknown keys
    }

exit:
	//	result = CFStringCreateWithCString( NULL, INFO_PASSWORD_DEFAULT, kCFStringEncodingUTF8 ); //default password
    if( key )       free( key );	
    return( result );
}



//===========================================================================================================================
//	CFPreferencesSetAppValue_compat
//===========================================================================================================================

void    CFPreferencesSetAppValue_compat( CFStringRef inKey, CFPropertyListRef inValue, CFStringRef inAppID )
{
    char *        key = NULL;
    char *        value = NULL;
    
    (void) inAppID;
    char cmd[256];

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
        /* 
         * PLATFORM_TO_DO:
         *
         *   - Save or reset password here.
         */

        if ( value == NULL )
        {
            // reset password here 
            printf("%s clear airplay password\n", __FUNCTION__);            
            memset((void *)&cmd, 0, sizeof(cmd));
			sprintf(cmd, "nvram_set 2860 AIRPLAY_PASSWORD&");
			system(cmd);
        }
        else
        {
            // save specified passwod "value" here 
            printf("%s save airplay password[%s]\n", __FUNCTION__, value);
            memset((void *)&cmd, 0, sizeof(cmd));
			sprintf(cmd, "nvram_set 2860 AIRPLAY_PASSWORD \"%s\"&", value);
			system(cmd);
        }
    }

    // Unknown

    else
    {
    	printf("=============== !!!! %s [%s] =================\n", __FUNCTION__, key);
        // ignore unknown keys
    }

exit:
    if( key )       free( key );
    if( value )     free( value );
}



//===========================================================================================================================
//	CFPreferencesAppSynchronize_compat
//===========================================================================================================================

Boolean CFPreferencesAppSynchronize_compat( CFStringRef inAppID )
{
    (void) inAppID;
            
        /* 
         * PLATFORM_TO_DO:
         *
         *   - Reload any preference data from persistent storage if needed.
         */
			printf("=============== !!!! %s %d =================\n", __FUNCTION__, __LINE__);

    return( true );
}
