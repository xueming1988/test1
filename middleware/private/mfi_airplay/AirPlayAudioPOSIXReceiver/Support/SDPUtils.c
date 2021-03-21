/*
	File:    SDPUtils.c
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
	
	Copyright (C) 2007-2013 Apple Inc. All Rights Reserved.
*/

#include <ctype.h>

#include "APSCommonServices.h"
#include "APSDebugServices.h"
#include "StringUtils.h"

#include "SDPUtils.h"

//===========================================================================================================================
//	SDPFindAttribute
//===========================================================================================================================

OSStatus
	SDPFindAttribute( 
		const char *	inSrc, 
		const char *	inEnd, 
		const char *	inAttribute, 
		const char **	outValuePtr, 
		size_t *		outValueLen, 
		const char **	outNextPtr )
{
	OSStatus			err;
	char				type;
	const char *		valuePtr;
	size_t				valueLen;
	
	while( ( err = SDPGetNext( inSrc, inEnd, &type, &valuePtr, &valueLen, &inSrc ) ) == kNoErr )
	{
		if( type == 'a' )
		{
			const char *		ptr;
			const char *		end;
			
			ptr = valuePtr;
			end = ptr + valueLen;
			while( ( ptr < end ) && ( *ptr != ':' ) ) ++ptr;
			if( ( ptr < end ) && ( strncmpx( valuePtr, (size_t)( ptr - valuePtr ), inAttribute ) == 0 ) )
			{
				ptr += 1;
				*outValuePtr = ptr;
				*outValueLen = (size_t)( end - ptr );
				break;
			}
		}
	}
	if( outNextPtr ) *outNextPtr = inSrc;
	return( err );
}

//===========================================================================================================================
//	SDPFindMediaSection
//===========================================================================================================================

OSStatus
	SDPFindMediaSection( 
		const char *	inSrc, 
		const char *	inEnd, 
		const char **	outMediaSectionPtr, 
		const char **	outMediaSectionEnd, 
		const char **	outMediaValuePtr, 
		size_t *		outMediaValueLen, 
		const char **	outNextPtr )
{
	OSStatus			err;
	char				type;
	const char *		valuePtr;
	size_t				valueLen;
	const char *		mediaSectionPtr;
	const char *		mediaValuePtr;
	size_t				mediaValueLen;
	
	// Find a media section.
	
	mediaSectionPtr = NULL;
	mediaValuePtr   = NULL;
	mediaValueLen   = 0;
	while( SDPGetNext( inSrc, inEnd, &type, &valuePtr, &valueLen, &inSrc ) == kNoErr )
	{
		if( type == 'm' )
		{
			mediaSectionPtr	= valuePtr - 2; // Skip back to "m=".
			mediaValuePtr	= valuePtr;
			mediaValueLen	= valueLen;
			break;
		}
	}
	require_action_quiet( mediaSectionPtr, exit, err = kNotFoundErr );
	
	// Find the end of the media section.
	
	while( SDPGetNext( inSrc, inEnd, &type, &valuePtr, &valueLen, &inSrc ) == kNoErr )
	{
		if( type == 'm' )
		{
			inSrc = valuePtr - 2; // Skip back to "m=" of the next section.
			break;
		}
	}
	
	if( outMediaSectionPtr )	*outMediaSectionPtr	= mediaSectionPtr;
	if( outMediaSectionEnd )	*outMediaSectionEnd	= inSrc;
	if( outMediaValuePtr )		*outMediaValuePtr	= mediaValuePtr;
	if( outMediaValueLen )		*outMediaValueLen	= mediaValueLen;
	if( outNextPtr )			*outNextPtr			= inSrc;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	SDPFindMediaSectionEx
//===========================================================================================================================

OSStatus
	SDPFindMediaSectionEx( 
		const char *	inSrc, 
		const char *	inEnd, 
		const char **	outMediaTypePtr, 
		size_t *		outMediaTypeLen, 
		const char **	outPortPtr, 
		size_t *		outPortLen, 
		const char **	outProtocolPtr, 
		size_t *		outProtocolLen, 
		const char **	outFormatPtr, 
		size_t *		outFormatLen, 
		const char **	outSubSectionsPtr, 
		size_t *		outSubSectionsLen, 
		const char **	outSrc )
{
	OSStatus			err;
	char				type;
	const char *		valuePtr;
	size_t				valueLen;
	const char *		mediaValuePtr;
	size_t				mediaValueLen;
	const char *		subSectionPtr;
	const char *		ptr;
	const char *		end;
	const char *		token;
	
	// Find a media section. Media sections start with "m=" and end with another media section or the end of data.
	// The following is an example of two media sections, "audio" and "video".
	//
	//		m=audio 21010 RTP/AVP 5\r\n
	//		c=IN IP4 224.0.1.11/127\r\n
	//		a=ptime:40\r\n
	//		m=video 61010 RTP/AVP 31\r\n
	//		c=IN IP4 224.0.1.12/127\r\n
	
	while( ( err = SDPGetNext( inSrc, inEnd, &type, &mediaValuePtr, &mediaValueLen, &inSrc ) ) == kNoErr )
	{
		if( type == 'm' )
		{
			break;
		}
	}
	require_noerr_quiet( err, exit );
	subSectionPtr = inSrc;
	
	// Find the end of the media section. Media sections end with another media section or the end of data.
	
	while( SDPGetNext( inSrc, inEnd, &type, &valuePtr, &valueLen, &inSrc ) == kNoErr )
	{
		if( type == 'm' )
		{
			inSrc = valuePtr - 2; // Skip back to "m=" of the next section.
			break;
		}
	}
	
	if( outSubSectionsPtr ) *outSubSectionsPtr	= subSectionPtr;
	if( outSubSectionsLen )	*outSubSectionsLen	= (size_t)( inSrc - subSectionPtr );
	if( outSrc )			*outSrc				= inSrc;
	
	// Parse details out of the media line (after the m= prefix). It has the following format:
	//
	//		<media> <port> <proto> <fmt> ...
	//
	//		"video 49170/2 RTP/AVP 31"
	
	ptr = mediaValuePtr;
	end = ptr + mediaValueLen;
	
	// Media Type
	
	while( ( ptr < end ) && isspace_safe( *ptr ) ) ++ptr;
	token = ptr;
	while( ( ptr < end ) && !isspace_safe( *ptr ) ) ++ptr;
	if( outMediaTypePtr ) *outMediaTypePtr = token;
	if( outMediaTypeLen ) *outMediaTypeLen = (size_t)( ptr - token );
	
	// Port
	
	while( ( ptr < end ) && isspace_safe( *ptr ) ) ++ptr;
	token = ptr;
	while( ( ptr < end ) && !isspace_safe( *ptr ) ) ++ptr;
	if( outPortPtr ) *outPortPtr = token;
	if( outPortLen ) *outPortLen = (size_t)( ptr - token );
	
	// Protocol
	
	while( ( ptr < end ) && isspace_safe( *ptr ) ) ++ptr;
	token = ptr;
	while( ( ptr < end ) && !isspace_safe( *ptr ) ) ++ptr;
	if( outProtocolPtr ) *outProtocolPtr = token;
	if( outProtocolLen ) *outProtocolLen = (size_t)( ptr - token );
	
	// Format
	
	while( ( ptr < end ) && isspace_safe( *ptr ) ) ++ptr;
	while( ( ptr < end ) && isspace_safe( end[ -1 ] ) ) --end;
	if( outFormatPtr ) *outFormatPtr = ptr;
	if( outFormatLen ) *outFormatLen = (size_t)( end - ptr );
	
exit:
	return( err );
}

//===========================================================================================================================
//	SDPFindParameter
//===========================================================================================================================

OSStatus
	SDPFindParameter( 
		const char *	inSrc, 
		const char *	inEnd, 
		const char *	inName, 
		const char **	outValuePtr, 
		size_t *		outValueLen, 
		const char **	outNextPtr )
{
	const char *		namePtr;
	size_t				nameLen;
	const char *		valuePtr;
	size_t				valueLen;
	
	while( SDPGetNextParameter( inSrc, inEnd, &namePtr, &nameLen, &valuePtr, &valueLen, &inSrc ) == kNoErr )
	{
		if( strncmpx( namePtr, nameLen, inName ) == 0 )
		{
			if( outValuePtr ) *outValuePtr = valuePtr;
			if( outValueLen ) *outValueLen = valueLen;
			if( outNextPtr )  *outNextPtr  = inSrc;
			return( kNoErr );
		}
	}
	if( outNextPtr ) *outNextPtr = inSrc;
	return( kNotFoundErr );
}

//===========================================================================================================================
//	SDPFindSessionSection
//===========================================================================================================================

OSStatus
	SDPFindSessionSection( 
		const char *	inSrc, 
		const char *	inEnd, 
		const char **	outSectionPtr, 
		const char **	outSectionEnd, 
		const char **	outNextPtr )
{
	const char *		sessionPtr;
	char				type;
	const char *		valuePtr;
	size_t				valueLen;
	
	// SDP session sections start at the beginning of the text and go until the next section (or the end of text).
	
	sessionPtr = inSrc;
	while( SDPGetNext( inSrc, inEnd, &type, &valuePtr, &valueLen, &inSrc ) == kNoErr )
	{
		if( type == 'm' )
		{
			inSrc = valuePtr - 2; // Skip to just before "m=" of the next section.
			break;
		}
	}
	if( outSectionPtr ) *outSectionPtr	= sessionPtr;
	if( outSectionEnd ) *outSectionEnd	= inSrc;
	if( outNextPtr )	*outNextPtr		= inSrc;
	return( kNoErr );
}

//===========================================================================================================================
//	SDPFindAttribute
//===========================================================================================================================

OSStatus
	SDPFindType( 
		const char *	inSrc, 
		const char *	inEnd, 
		char			inType, 
		const char **	outValuePtr, 
		size_t *		outValueLen, 
		const char **	outNextPtr )
{
	OSStatus			err;
	char				type;
	const char *		valuePtr;
	size_t				valueLen;
	
	while( ( err = SDPGetNext( inSrc, inEnd, &type, &valuePtr, &valueLen, &inSrc ) ) == kNoErr )
	{
		if( type == inType )
		{
			if( outValuePtr ) *outValuePtr = valuePtr;
			if( outValueLen ) *outValueLen = valueLen;
			break;
		}
	}
	if( outNextPtr ) *outNextPtr = inSrc;
	return( err );
}

//===========================================================================================================================
//	SDPGetNext
//===========================================================================================================================

OSStatus
	SDPGetNext( 
		const char *	inSrc, 
		const char *	inEnd, 
		char *			outType, 
		const char **	outValuePtr, 
		size_t *		outValueLen, 
		const char **	outNextPtr )
{
	OSStatus			err;
	char				type;
	const char *		val;
	size_t				len;
	char				c;
	
	require_action_quiet( inSrc < inEnd, exit, err = kNotFoundErr );
	
	// Parse a <type>=<value> line (e.g. "v=0\r\n").
	
	require_action( ( inEnd - inSrc ) >= 2, exit, err = kSizeErr );
	type = inSrc[ 0 ];
	require_action( inSrc[ 1 ] == '=', exit, err = kMalformedErr );
	inSrc += 2;
	
	for( val = inSrc; ( inSrc < inEnd ) && ( ( c = *inSrc ) != '\r' ) && ( c != '\n' ); ++inSrc ) {}
	len = (size_t)( inSrc - val );
		
	while( ( inSrc < inEnd ) && ( ( ( c = *inSrc ) == '\r' ) || ( c == '\n' ) ) ) ++inSrc;
	
	// Return results.
	
	*outType		= type;
	*outValuePtr	= val;
	*outValueLen	= len;
	*outNextPtr		= inSrc;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	SDPGetNextAttribute
//===========================================================================================================================

OSStatus
	SDPGetNextAttribute( 
		const char *	inSrc, 
		const char *	inEnd, 
		const char **	outNamePtr, 
		size_t *		outNameLen, 
		const char **	outValuePtr, 
		size_t *		outValueLen, 
		const char **	outNextPtr )
{
	OSStatus			err;
	char				type;
	const char *		src;
	const char *		ptr;
	const char *		end;
	size_t				len;
	
	while( ( err = SDPGetNext( inSrc, inEnd, &type, &src, &len, &inSrc ) ) == kNoErr )
	{
		if( type == 'a' )
		{
			break;
		}
	}
	require_noerr_quiet( err, exit );
	
	ptr = src;
	end = src + len;
	while( ( src < end ) && ( *src != ':' ) ) ++src;
	
	if( outNamePtr )  *outNamePtr  = ptr;
	if( outNameLen )  *outNameLen  = (size_t)( src - ptr );
	
	if( src < end ) ++src;
	if( outValuePtr ) *outValuePtr = src;
	if( outValueLen ) *outValueLen = (size_t)( end - src );
	
exit:
	if( outNextPtr ) *outNextPtr = inSrc;
	return( err );
}

//===========================================================================================================================
//	SDPGetNextParameter
//===========================================================================================================================

OSStatus
	SDPGetNextParameter( 
		const char *	inSrc, 
		const char *	inEnd, 
		const char **	outNamePtr, 
		size_t *		outNameLen, 
		const char **	outValuePtr, 
		size_t *		outValueLen, 
		const char **	outNextPtr )
{
	const char *		ptr;
	
	while( ( inSrc < inEnd ) && isspace_safe( *inSrc ) ) ++inSrc;
	if( inSrc == inEnd ) return( kNotFoundErr );
	
	for( ptr = inSrc; ( inSrc < inEnd ) && ( *inSrc != '=' ); ++inSrc ) {}
	if( outNamePtr ) *outNamePtr = ptr;
	if( outNameLen ) *outNameLen = (size_t)( inSrc - ptr );
	if( inSrc < inEnd ) ++inSrc;
	
	for( ptr = inSrc; ( inSrc < inEnd ) && ( *inSrc != ';' ); ++inSrc ) {}
	if( outValuePtr ) *outValuePtr = ptr;
	if( outValueLen ) *outValueLen = (size_t)( inSrc - ptr );
	
	if( outNextPtr ) *outNextPtr = ( inSrc < inEnd ) ? ( inSrc + 1 ) : inSrc;
	return( kNoErr );
}

//===========================================================================================================================
//	SDPScanFAttribute
//===========================================================================================================================

int
	SDPScanFAttribute( 
		const char *	inSrc, 
		const char *	inEnd, 
		const char **	outSrc, 
		const char *	inAttribute, 
		const char *	inFormat, 
		... )
{
	int			n;
	va_list		args;
	
	va_start( args, inFormat );
	n = SDPScanFAttributeV( inSrc, inEnd, outSrc, inAttribute, inFormat, args );
	va_end( args );
	return( n );
}

//===========================================================================================================================
//	SDPScanFAttributeV
//===========================================================================================================================

int
	SDPScanFAttributeV( 
		const char *	inSrc, 
		const char *	inEnd, 
		const char **	outSrc, 
		const char *	inAttribute, 
		const char *	inFormat, 
		va_list			inArgs )
{
	int					n;
	OSStatus			err;
	const char *		valuePtr;
	size_t				valueLen;
	
	n = 0;
	err = SDPFindAttribute( inSrc, inEnd, inAttribute, &valuePtr, &valueLen, outSrc );
	require_noerr_quiet( err, exit );
	
	n = VSNScanF( valuePtr, valueLen, inFormat, inArgs );
	
exit:
	return( n );
}

#if 0
#pragma mark -
#pragma mark == Debugging ==
#endif

