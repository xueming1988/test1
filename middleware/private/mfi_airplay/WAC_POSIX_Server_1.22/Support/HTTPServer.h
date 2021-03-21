/*
    File:    HTTPServer.h
    Package: WACServer
    Version: WAC_POSIX_Server_1.22
    
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
    
    Copyright (C) 2013 Apple Inc. All Rights Reserved.
*/

/*!
     @header HTTP Server
      This header contains function prototypes called by the WAC
      engine. This file contains a simplistic HTTP server for accepting
      connections from the WAC client devices.
 */

#include "Common.h"

#include <sys/socket.h>
#include <netdb.h>

#include "URLUtils.h"
#include "HTTPUtils.h"


typedef enum
{
    eStateUnconfigured  = 0,
    eStateNotStarted    = 1,
    eStateRunning       = 2,
    eStateStopped       = 3,
    eStateError         = 4

} HTTPServerState_t;

#define HTTPServerStateToString( X ) ( \
    ( (X) == eStateUnconfigured )   ? "Unconfigured"    : \
    ( (X) == eStateNotStarted )     ? "Not started"     : \
    ( (X) == eStateRunning )        ? "Running"         : \
    ( (X) == eStateStopped )        ? "Stopped"         : \
    ( (X) == eStateError )          ? "Error"           : \
    "?" )

typedef struct
{
    const char          *name;
    uint32_t            uuid;
    HTTPServerState_t   state;
    int                 acceptSocket;
    int                 connectedSocket;
#if HAVE_IPV6	
    struct sockaddr_in6 sockAddr;
#else
    struct sockaddr_in sockAddr;
#endif
    void                (*callback)( HTTPHeader_t *, bool, void * );
    pthread_t           thread;
    int                 threadCommandPipe[ 2 ];
    void                *callerPayload;

} HTTPServer_t;


OSStatus HTTPServerCreate( const char *name, void *callback, void *callerPayload, HTTPServer_t *outServer );

OSStatus HTTPServerStart( HTTPServer_t *inServer );

OSStatus HTTPServerStop( HTTPServer_t *inServer );

OSStatus HTTPServerSend( HTTPServer_t *inServer, const uint8_t *inBuf, size_t inBufLen );

void HTTPServerPrint( HTTPServer_t *inServer );
