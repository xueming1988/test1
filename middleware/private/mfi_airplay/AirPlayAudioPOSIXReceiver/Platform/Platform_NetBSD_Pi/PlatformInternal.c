/*
	File:    PlatformInternal.c
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

#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <errno.h>

#include "CommonServices.h"     // For OSStatus
#include "AirPlayReceiverServer.h"
#include "AirPlayReceiverPOSIX.h"

#include "PlatformInternal.h"


#define PASSWORD_NAME_CONFIG_FILE   "/root/wac_config"
#define PASSWORD_KEY                "password"
#define FRIENDLY_NAME_KEY           "name"
#define LINE_SIZE                   128

#define DELETE_PASSWORD_CMDLINE     "sed -i '/" PASSWORD_KEY "/d' " PASSWORD_NAME_CONFIG_FILE
#define APPEND_PASSWORD_CMDLINE     "echo " PASSWORD_KEY "=%s >> " PASSWORD_NAME_CONFIG_FILE

#define DELETE_NAME_CMDLINE         "sed -i '/" FRIENDLY_NAME_KEY "/d' " PASSWORD_NAME_CONFIG_FILE
#define APPEND_NAME_CMDLINE         "echo " FRIENDLY_NAME_KEY "=%s >> " PASSWORD_NAME_CONFIG_FILE

/* 
 * variables
 */
static char *               gPlayPassword = NULL;
static char *               gFriendlyName = NULL;
static char *               gSongTitle = NULL;
static char *               gSongAlbum = NULL;
static char *               gSongArtist = NULL;
static char *               gSongComposer = NULL;
static char *               gSongElapsed = NULL;
static char *               gSongDuration = NULL;

static AirPlayFeatures      gMetadataFeatures = kAirPlayFeature_AudioMetaDataArtwork | 
                                                kAirPlayFeature_AudioMetaDataText |  
                                                kAirPlayFeature_AudioMetaDataProgress; 


static char *retrieveKeyValueFromFile( char *fileName, char *inKey )
{
    FILE *pf;
    char *value;
    char line[ LINE_SIZE ];

    if ((fileName == NULL) || (inKey == NULL)) return( NULL );

    // open file for reading and execute our command.
    pf = fopen( PASSWORD_NAME_CONFIG_FILE, "r" );

    if(!pf)
    {
        // fprintf(stderr, "Could not open %s for %s\n", PASSWORD_NAME_CONFIG_FILE, inKey);
        return( NULL );
    }

    value = NULL;

    while (1)
    {
        char *key, *ret;

        // Grab line from file
        ret = fgets(line, LINE_SIZE , pf);

        if ( ret == NULL )          break;      /* EOF or an error */
        if ( strlen(line) == 1 )    continue;   /* empty line */

        /* dump tokens */
        key = strtok( line, "=\n");

        if ( key == NULL ) break;

        if ( strncmp( key, inKey, sizeof( PASSWORD_KEY )) == 0 )
        {
            value = strtok( NULL, " \n");
            break;
        }
    }

    fclose(pf);

    if (value) value = strndup( value, strlen(value) );

    return( value );
}

static void appendKeyValueToFile( char *inFileName, char *inKey, char *inValue )
{
    FILE *pf;

    if ((inFileName == NULL) || (inKey == NULL)) return;

    // open file for reading and execute our command.
    pf = fopen( inFileName, "a+" );

    if(!pf)
    {
        fprintf(stderr, "Could not open %s for %s\n", inFileName, inKey);
        return;
    }

    fprintf(pf, "%10s = %s\n", inKey, inValue ? inValue : "");

    // flush user space buffer and then sync everything to disk for this file
    // fflush( pf);
    // fsync( fileno(pf) );

    fclose(pf);

    return;
}

char *getPlayPassword( void )
{
    /* get the password specified in the config file */
    gPlayPassword = retrieveKeyValueFromFile( PASSWORD_NAME_CONFIG_FILE, PASSWORD_KEY );
    return( gPlayPassword );
}

static void setPlayPassword( char *newPassword )
{
    char line[ LINE_SIZE ];

    if (gPlayPassword) free( gPlayPassword );

    if ( newPassword )
    {
        gPlayPassword = strdup(newPassword);
        printf("New Password=%s\n", gPlayPassword ? gPlayPassword : "NULL");
    }
    else
    {
        gPlayPassword = NULL;
        printf("Password Reset\n");
    }

    /* delete any existing password line in the config file */
    snprintf(line, LINE_SIZE, DELETE_PASSWORD_CMDLINE);
    printf("Delete Command: %s\n", line);
    system(line);

    /* append a new password line with the new password to the config file */
    snprintf(line, LINE_SIZE, APPEND_PASSWORD_CMDLINE, gPlayPassword ? gPlayPassword : "");
    printf("Append Command: %s\n", line);
    system(line);

    return;
}


void handlePasswordChange( char *newPassword)
{
    /* save the new password */
    setPlayPassword( newPassword );

    /* notify airplay about password change */
    AirPlayReceiverServerPostEvent( AirPlayReceiverServerGetRef(), CFSTR( kAirPlayEvent_PrefsChanged ), NULL, NULL );

    return;
}


char *getFriendlyName( void )
{
    char line[ LINE_SIZE ];

    /* get the name specified in the config file */
    gFriendlyName = retrieveKeyValueFromFile( PASSWORD_NAME_CONFIG_FILE, FRIENDLY_NAME_KEY );

    if (gFriendlyName == NULL)
    {
        /* no name specified in the config file. So use hostname */
        line[0] = '\0';
        gethostname( line, LINE_SIZE );

        gFriendlyName = strndup(line, LINE_SIZE - 1);
    }

    /* save the name as the hostname as well */
    if (gFriendlyName)  sethostname( gFriendlyName, strlen( gFriendlyName ));

    return( gFriendlyName );
}

static void setFriendlyName( char *newName )
{
    char line[ LINE_SIZE ];

    if (newName == NULL) return;
    if (gFriendlyName) free( gFriendlyName );

    gFriendlyName = strdup(newName);

    if (gFriendlyName == NULL) return;

    /* save the new name as the hostname as well */
    sethostname( newName, strlen( gFriendlyName ));
    printf("New Name=%s\n", gFriendlyName);

    /* delete any existing name line in the config file */
    snprintf(line, LINE_SIZE, DELETE_NAME_CMDLINE);
    printf("Delete Command: %s\n", line);
    system(line);

    /* append a new name line with the new name to the config file */
    snprintf(line, LINE_SIZE, APPEND_NAME_CMDLINE, gFriendlyName);
    printf("Append Command: %s\n", line);
    system(line);

    return;
}

void handleFriendlyNameChange( char *newName)
{
    if ( newName )
    {
        /* new name specified. set new host name */
        setFriendlyName( newName );

        /* notify airplay about name change */
        AirPlayReceiverServerPostEvent( AirPlayReceiverServerGetRef(), CFSTR( kAirPlayEvent_PrefsChanged ), NULL, NULL );
    }
    else
    {
        printf("Name Request Failed: No name specified\n");
    }

    return;
}


static void setMetadataFeature( AirPlayFeatures metadataSupported )
{
    gMetadataFeatures |= metadataSupported;

    return;
}

void setMetadataFeatures( const char*  mdSupported )
{
    if (mdSupported == NULL) return;

    gMetadataFeatures = 0;  // reset the default now that call to set metadata is made

    // -md= -- Metadata support (a=artwork t=text p=progress) any combination. Ex. -md=ta

    if (strchr(mdSupported, 'a')) 
    {
        setMetadataFeature( kAirPlayFeature_AudioMetaDataArtwork );
        printf("*** Metadata ArtWork Supported  ***\n");
    }

    if (strchr(mdSupported, 't')) 
    {
        setMetadataFeature( kAirPlayFeature_AudioMetaDataText );
        printf("*** Metadata Text Supported     ***\n");
    }

    if (strchr(mdSupported, 'p')) 
    {
        setMetadataFeature( kAirPlayFeature_AudioMetaDataProgress );
        printf("*** Metadata Progress Supported ***\n");
    }

    if ( gMetadataFeatures == 0 ) printf("*** Metadata None Supported ***\n");

    return;
}

AirPlayFeatures getMetadataFeatures( void )
{
    return( gMetadataFeatures );
}

void handleMetadataChange( const char*  mdType, const char *mdValue )
{
    if (mdType == NULL) return;

    if( 0 ) {}

    // title
    else if ( strcmp(TITLE_KEY, mdType) == 0 )
    {
        if ( gSongTitle ) free ( gSongTitle );
        gSongTitle = mdValue ? strdup( mdValue ) : NULL;
    }

    // artist
    else if ( strcmp(ARTIST_KEY, mdType) == 0 )
    {
        if ( gSongArtist ) free ( gSongArtist );
        gSongArtist = mdValue ? strdup( mdValue ) : NULL;
    }

    // album
    else if ( strcmp(ALBUM_KEY, mdType) == 0 )
    {
        if ( gSongAlbum ) free ( gSongAlbum );
        gSongAlbum = mdValue ? strdup( mdValue ) : NULL;
    }

    // composer
    else if ( strcmp(COMPOSER_KEY, mdType) == 0 )
    {
        if ( gSongComposer ) free ( gSongComposer );
        gSongComposer = mdValue ? strdup( mdValue ) : NULL;
    }

    // elapsed time
    else if ( strcmp(ELAPSED_KEY, mdType) == 0 )
    {
        if ( gSongElapsed ) free ( gSongElapsed );
        gSongElapsed = mdValue ? strdup( mdValue ) : NULL;
    }

    // duration
    else if ( strcmp(DURATION_KEY, mdType) == 0 )
    {
        if ( gSongDuration ) free ( gSongDuration );
        gSongDuration = mdValue ? strdup( mdValue ) : NULL;
    }


    /*
     * update the new metadata in the song metadata file
     */
    system( "echo > " METADATA_FILE_NAME );  // reset the file
    appendKeyValueToFile( METADATA_FILE_NAME, TITLE_KEY, gSongTitle);
    appendKeyValueToFile( METADATA_FILE_NAME, ARTIST_KEY, gSongArtist);
    appendKeyValueToFile( METADATA_FILE_NAME, ALBUM_KEY, gSongAlbum);
    appendKeyValueToFile( METADATA_FILE_NAME, COMPOSER_KEY, gSongComposer);
    appendKeyValueToFile( METADATA_FILE_NAME, ELAPSED_KEY, gSongElapsed);
    appendKeyValueToFile( METADATA_FILE_NAME, DURATION_KEY, gSongDuration);

    return;
}


