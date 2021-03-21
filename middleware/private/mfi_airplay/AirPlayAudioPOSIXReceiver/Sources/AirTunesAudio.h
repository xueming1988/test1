/*
	File:    AirTunesAudio.h
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
	
	Copyright (C) 2010-2014 Apple Inc. All Rights Reserved.
*/

#ifndef	__AirTunesAudio_h__
#define	__AirTunesAudio_h__

#if 0
#pragma mark == Debug Properties ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		Debug Properties
	@abstract	Properties for debugging the HAL plugin.
*/

#define kAirPlayPropertyDebug			0x61706462  // 'apdb' Qualifier: Request plist. Value: Response plist.
#define kAirPlayPropertyPrefs			0x61707066  // 'appf' Qualifier: String, Data: plist. Gets/sets AirPlay system prefs.
#define kAirPlayPropertyRemovePref		0x61707072  // 'appr' Qualifier: String, Data: None. Removes an AirPlay system pref.

#if 0
#pragma mark == Meta Data Properties ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		Meta Data Properties
	@abstract	Properties for communicating meta data to and from the HAL plugin.
*/

#define kAudioDevicePropertyArtwork					0x61727477 //! 'artw' [CFDictionary] Artwork meta data.
#define kAudioDevicePropertyMetaData				0x6D657461 //! 'meta' [CFDictionary] Meta data.
#define kAudioDevicePropertyProgress				0x70726F67 //! 'prog' [CFDictionary] Progress meta data.
	// For gets, output buffer is [CFDictionary *outMetaData] (always empty). Caller must CFRelease on success.
	// For sets, input  buffer is [CFDictionary *inMetaData].
	//
	// Note: These are separate properties for artwork, textual meta data, and progress because it may not be
	//       convenient for the upper layers to have these all at the same time. This avoids having to keep 
	//       all the data present for every set of the property. An alternative would be to support incremental
	//       meta data updates such that if a key is present, it replaces the current piece of meta data, but 
	//       if it's missing, it's not touched. Then for example, if a song is played that doesn't have album
	//       artwork, it would have an "artworkData" value of a 0-byte data to indicate "no artwork".
	
	#define kAudioMetaDataKey_PresentTime			"presentTime"		//! [CFNumber] Sample time to present meta data to user.
	
	#define kAudioMetaDataKey_Artist				"artist"			//! [CFString]
	#define kAudioMetaDataKey_Album					"album"				//! [CFString]
	#define kAudioMetaDataKey_Composer				"composer"			//! [CFString]
	#define kAudioMetaDataKey_Genre					"genre"				//! [CFString]
	#define kAudioMetaDataKey_Title					"title"				//! [CFString]
	#define kAudioMetaDataKey_TrackNumber			"trackNumber"		//! [CFNumber]
	#define kAudioMetaDataKey_TotalTracks			"totalTracks"		//! [CFNumber]
	#define kAudioMetaDataKey_DiscNumber			"discNumber"		//! [CFNumber]
	#define kAudioMetaDataKey_TotalDiscs			"totalDiscs"		//! [CFNumber]
	
	#define kAudioMetaDataKey_ArtworkData			"artworkData"		//! [CFData]   PNG, JPEG, etc. image data.
	#define kAudioMetaDataKey_ArtworkKind			"artworkKind"		//! [CFString] Front Cover, etc.
	#define kAudioMetaDataKey_ArtworkMIMEType		"artworkMIMEType"	//! [CFString] Defaults to PNG if not specified.
	
	#define kAudioMetaDataKey_ProgressStart			"progressStart"		//! [CFNumber] Start time of song in sample units.
	#define kAudioMetaDataKey_ProgressCurrent		"progressCurrent"	//! [CFNumber] Current time of song  in sample units.
	#define kAudioMetaDataKey_ProgressEnd			"progressEnd"		//! [CFNumber] End time of song  in sample units.

#if 0
#pragma mark == Data Source Properties ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		DataSource / DataSources
	@abstract	AirPlay-specific alternatives to CoreAudio's normal DataSource properties for customization.
*/

#define kAirPlayPropertyDataSource			0x61706473 // 'apds' Like kAudioDevicePropertyDataSource
#define kAirPlayPropertyDataSources			0x6170646C // 'apdl' Like kAudioDevicePropertyDataSources

#define kAirPlayPropertyEndpointInfo		0x61707369 // 'apsi'
	// Qualifier: uint64_t endpointID or 0/missing for selected endpoint.
	// Data Type: CFDictionary endpointInfo
	// For gets, gets endpoint info.
	// For sets, sets endpoint info.

#define kAirPlayPropertyEndpointInfos		0x61706569 // 'apei'
	// Qualifier: none.
	// Data Type: CFArrayRef of CFDictionary's.
	// For gets, Returns CFArray of endpoint info dictionaries.
	// For sets, doesn't nothing.

#define kAirPlayPropertySelectedEndpoint	0x61707365 // 'apse'
	// Qualifier: uint32_t screen.
	// Data Type: uint64_t endpointID
	// For gets, Returns selected endpoint ID.
	// For sets, Selects endpoint.

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		kAudioDevicePropertyDataSourceDiscoveryState
	@abstract	Controls discovery for an audio object.
	@discussion
	
	This allows the upper layer code to control when the audio object performs discovery of DataSource objects.
	For example, an audio object that discovers endpoints via Bonjour wouldn't want to keep queries active forever
	so the upper layer code would start discovery only when the endpoint picker UI is presented to the user.
*/

#define kAudioDevicePropertyDataSourceDiscoveryState		0x64736473  //! 'dsds' [uint32_t] Current state: 1=discovering. 0=stopped.
	
	#define kAudioDataSourceDiscoveryState_Discovering		1 //! For gets, this means it's currently discovering.
															  //! For sets, this tells it to start discovering.
	
	#define kAudioDataSourceDiscoveryState_Stopped			0 //! For gets, this means it's currently not discovering.
															  //! For sets, this tells it to stop discovering.

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		kAudioDevicePropertyDataSourceInfoForID
	@abstract	Gets and sets information about a DataSource object.
	@discussion
	
	This may be used to get details about a DataSource, such as its name. It may also be used to detect changes to a
	DataSource object, such as when the device encounters an error (e.g. bad password if it requires a password).
	Some information may also be set this way, such as setting the password for a password-protected DataSource.
*/

#define kAudioDevicePropertyDataSourceInfoForID				0x64736969 //! 'dsii'
	// For gets, qualifier is [uint32_t *inItemID], output buffer is [CFDictionary *outInfo]. Caller must CFRelease on success.
	// For sets, qualifier is [uint32_t *inItemID], input  buffer is [CFDictionary *inInfo].
	// For change events, the selector is the property, the scope is the item ID, and the element is 0.
	//
	// This property translates the given data source item ID into a CFDictionary containing information about 
	// the data source item. This could replace kAudioDevicePropertyDataSourceNameForIDCFString.
	
	#define kAudioDataSourceInfoKey_AuthStatus				"authStatus"			//! [CFNumber]  Status of most recent password auth.
	#define kAudioDataSourceInfoKey_CompressionTypes		"compressionTypes"		//! [CFNumber]  Supported compression types. See AirPlayCompressionType.
	#define kAudioDataSourceInfoKey_DataSourceID			"dataSourceID"			//! [CFNumber]  CoreAudio data source ID for the endpoint.
	#define kAudioDataSourceInfoKey_DeviceID				"deviceID"				//! [CFString]  Globally unique device ID (e.g. "00:11:22:33:44:55").
	#define kAudioDataSourceInfoKey_DNSName_AirPlay			"dnsNameAirPlay"		//! [CFString]  Bonjour/DNS name to connect to for photos/video.
	#define kAudioDataSourceInfoKey_DNSName_AirPlayP2POnly	"dnsNameAirPlayP2POnly"	//! [CFBoolean] True if Bonjour/DNS name for photos/video is P2P.
	#define kAudioDataSourceInfoKey_DNSName					"dnsName"				//! [CFString]  Bonjour/DNS name to connect to for audio/screen.
	#define kAudioDataSourceInfoKey_DNSNameP2POnly			"dnsNameP2POnly"		//! [CFBoolean] True if Bonjour/DNS name for audio/screen is P2P.
	#define kAudioDataSourceInfoKey_EncryptionTypes			"encryptionTypes"		//! [CFNumber]  Supported encryption types. See AirPlayEncryptionType.
	#define kAudioDataSourceInfoKey_Features				"features"				//! [CFNumber]  Bit mask of feature flags (see AirPlayFeatures).
	#define kAudioDataSourceInfoKey_ForceAuth				"forceAuth"				//! [CFBoolean]	If true, try to auth even if we don't have a password (for PINs).
	#define kAudioDataSourceInfoKey_InitialVolume			"initialVolume"			//! [CFNumber]  Linear volume 0.0-1.0 from the endpoint on connect.
	#define kAudioDataSourceInfoKey_MetaDataTypes			"metaDataTypes"			//! [CFNumber]  Supported meta data types. See AirTunesMetaDataType.
	#define kAudioDataSourceInfoKey_Model					"model"					//! [CFString]  Model of the device (e.g. "Device1,1").
	#define kAudioDataSourceInfoKey_Name					"name"					//! [CFString]  Friendly name of the device.
	#define kAudioDataSourceInfoKey_Password				"password"				//! [CFString]	Password to use to access the device.
	#define kAudioDataSourceInfoKey_PasswordRequired		"passwordRequired"		//! [CFBoolean] True if device requires a password to play to it.
	#define kAudioDataSourceInfoKey_PIN						"pin"					//! [CFString]  If using PIN mode, this is most recently entered PIN.
	#define kAudioDataSourceInfoKey_Present					"present"				//! [CFBoolean] True if the device is detected on the network.
	#define kAudioDataSourceInfoKey_PublicKey				"pk"					//! [CFData]    Public key of the device.
	#define kAudioDataSourceInfoKey_ScreenWidth				"width"					//! [CFNumber]  Pixel height of a display connected to the device (e.g. 1280).
	#define kAudioDataSourceInfoKey_ScreenHeight			"height"				//! [CFNumber]  Pixel width of a display connected to the device (e.g. 720).
	#define kAudioDataSourceInfoKey_ScreenOverscanned		"overscanned"			//! [CFBoolean] True if the remote TV is overscanned.
	#define kAudioDataSourceInfoKey_Selected				"selected"				//! [CFBoolean] True if the endpoint is selected.
	#define kAudioDataSourceInfoKey_Status					"status"				//! [CFNumber]  A non-zero value if there is an error.
	#define kAudioDataSourceInfoKey_SystemFlags				"systemFlags"			//! [CFNumber]  See AirPlayStatusFlags.
	#define kAudioDataSourceInfoKey_TransportType			"transportType"			//! [CFNumber:NetTransportType] Network transport endpoint is using.
	#define kAudioDataSourceInfoKey_UID						"uid"					//! [CFString]  UID string (e.g. "00:11:22:33:44:55-screen").
		#define kAudioUIDType_AirPlay							"airplay"			//! UID Suffix for AirPlay audio/video route entries.
		#define kAudioUIDType_ScreenPlay						"screen"			//! UID Suffix for ScreenPlay route entries.
		#define kAudioUIDType_CarMain							"CarMain"			//! UID Suffix for the car's main audio.
		#define kAudioUIDType_CarAlt							"CarAlt"			//! UID Suffix for the car's alt audio.
	#define kAudioDataSourceInfoKey_Version					"version"				//! [CFString]  Source version (e.g. "110.31").
	#define kAudioDataSourceInfoKey_VodkaVersion			"vodkaVersion"			//! [CFNumber]  FairPlay Vodka version. Missing means 0.

// Compatibility flags for external clients. See AirPlayStatusFlags.

#define kAirTunesSystemFlag_PINMode					( 1 << 3 ) // 0x08: PIN require to use AirPlay.
#define kAirTunesSystemFlag_CloudConnectivity		( 1 << 6 ) // 0x40: Recently able to connect to the cloud.

#if 0
#pragma mark == Misc Properties ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		kAirPlayPropertyP2PActive
	@abstract	Disables or enables P2P. Mainly used for AirDrop compatibility mode.
*/

#define kAirPlayPropertyP2PActive		0x61707061 // 'appa'
	// Qualifier: none.
	// Data Type: uint32_t
	// For gets, returns 1 if P2P is currently active.
	// For sets, doesn't nothing.

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		kAirPlayPropertyP2PDisabled
	@abstract	Disables or enables P2P. Mainly used for AirDrop compatibility mode.
*/

#define kAirPlayPropertyP2PDisabled		0x61707064 // 'appd'
	// Qualifier: none.
	// Data Type: uint32_t
	// For gets, returns 1 if P2P is disabled.
	// For sets, if 1, disables P2P for the next start of AirPlay. If 0, allows P2P.

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		kAirPlayPropertyScreenPID
	@abstract	Returns the PID of process where AirPlay Screen is running.
*/
// Warning: Don't change this value without discussing with the CoreAUC project.
#define kAirPlayPropertyScreenPID				0x61707370 // 'apsp'
	// Qualifier: none.
	// Data Type: uint32_t
	// For gets, returns the PID of process where AirPlay Screen is running.
	// For sets, does nothing.

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		kAirPlayPropertyScreenStarted
	@abstract	Starts or stops AirPlay screen.
*/
// Warning: Don't change this value without discussing with the CoreAUC project.
#define kAirPlayPropertyScreenStarted			0x76696465 // 'vide'
	// Qualifier: none.
	// Data Type: uint32_t
	// For gets, returns 1 if AirPlay screen is started, 0 otherwise.
	// For sets, if 1, starts AirPlay screen if not started. If 0, stops AirPlay screen if not stopped.

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		kAirPlayPropertyEventError
	@abstract	Changed notification posted when there is an error.
*/

#define kAirPlayPropertyEventError				0x61706572  // 'aper'
	// Qualifier: none.
	// Data Type: uint32_t
	// For gets, returns 0;
	// For sets, does nothing.
	// For change notification: scope is the error code, element is the data source ID.

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		kAirPlayPropertyEventStartStatus
	@abstract	Starts or stops AirPlay screen 
*/

#define kAirPlayPropertyEventStartStatus		0x61707373  // 'apss'
	// Qualifier: none.
	// Data Type: uint32_t
	// For gets, returns 0;
	// For sets, does nothing.
	// For change notification: scope is the error code, element is the data source ID.

#define kAirPlayPropertyScreenLatency			0x6170736C // 'apsl'
	// Qualifier: none.
	// Data Type: int32_t
	// For gets, returns the number of milliseconds of screen latency.
	// For sets, unsupported.

#if 0
#pragma mark == Stark Properties ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		kAudioDevicePropertyStarkBluetoothIdentifiers
	@abstract	Get the MAC addresses of the Stark device's Bluetooth endpoints.
*/

#define kAudioDevicePropertyStarkBluetoothIdentifiers		0x73746269 // 'stbi'
	// Scope:		kAudioObjectPropertyScopeGlobal
	// Element:		kAudioObjectPropertyElementMaster
	// Qualifier:	none.
	// Data Type:	CFArray of CFString MAC addresses (e.g. "00:11:22:AA:BB:CC").
	// For gets:	Returns an array of CFStrings. Caller must release when done.
	// For sets:	Unsupported.

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		kAudioDevicePropertyStarkStreamAudioType
	@abstract	Gets or sets audio type for the Stark stream(s) associated with the device.
*/

#define kAudioDevicePropertyStarkStreamAudioType		0x73746174 // 'stat'
	// Scope:		kAudioObjectPropertyScopeInput or kAudioObjectPropertyScopeOutput or kAudioObjectPropertyScopeGlobal.
	// Element:		kAudioObjectPropertyElementMaster
	// Qualifier:	none.
	// Data Type:	uint32_t

#define kAudioDevicePropertyStarkStreamAudioType_Invalid				0
#define kAudioDevicePropertyStarkStreamAudioType_Default				0x61746466 // 'atdf'
#define kAudioDevicePropertyStarkStreamAudioType_Media					0x61746D65 // 'atme'
#define kAudioDevicePropertyStarkStreamAudioType_Measurement			0x61746D73 // 'atms'
#define kAudioDevicePropertyStarkStreamAudioType_Telephony				0x61747465 // 'atte'
#define kAudioDevicePropertyStarkStreamAudioType_SpeechRecognition		0x61747372 // 'atsr'
#define kAudioDevicePropertyStarkStreamAudioType_SpokenAudio			0x61747361 // 'atsa'
#define kAudioDevicePropertyStarkStreamAudioType_Alert					0x6174616C // 'atal'

#define VADAudioTypeToAirPlayString( X )	( \
	( (X) == kAudioDevicePropertyStarkStreamAudioType_Default )				? kAirPlayAudioType_Default				: \
	( (X) == kAudioDevicePropertyStarkStreamAudioType_Media )				? kAirPlayAudioType_Media				: \
	( (X) == kAudioDevicePropertyStarkStreamAudioType_Measurement )			? kAirPlayAudioType_Measurement			: \
	( (X) == kAudioDevicePropertyStarkStreamAudioType_Telephony )			? kAirPlayAudioType_Telephony			: \
	( (X) == kAudioDevicePropertyStarkStreamAudioType_SpeechRecognition )	? kAirPlayAudioType_SpeechRecognition	: \
	( (X) == kAudioDevicePropertyStarkStreamAudioType_SpokenAudio )			? kAirPlayAudioType_SpokenAudio			: \
	( (X) == kAudioDevicePropertyStarkStreamAudioType_Alert )				? kAirPlayAudioType_Alert				: \
																			  "?" )

#define VADAudioTypeToAirPlayCFString( X )	( \
	( (X) == kAudioDevicePropertyStarkStreamAudioType_Default )				? CFSTR( kAirPlayAudioType_Default )			: \
	( (X) == kAudioDevicePropertyStarkStreamAudioType_Media )				? CFSTR( kAirPlayAudioType_Media )				: \
	( (X) == kAudioDevicePropertyStarkStreamAudioType_Measurement )			? CFSTR( kAirPlayAudioType_Measurement )		: \
	( (X) == kAudioDevicePropertyStarkStreamAudioType_Telephony )			? CFSTR( kAirPlayAudioType_Telephony )			: \
	( (X) == kAudioDevicePropertyStarkStreamAudioType_SpeechRecognition )	? CFSTR( kAirPlayAudioType_SpeechRecognition )	: \
	( (X) == kAudioDevicePropertyStarkStreamAudioType_SpokenAudio )			? CFSTR( kAirPlayAudioType_SpokenAudio )		: \
	( (X) == kAudioDevicePropertyStarkStreamAudioType_Alert )				? CFSTR( kAirPlayAudioType_Alert )				: \
																			  CFSTR( "?" ) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		kAudioDevicePropertyStarkStreamPriority
	@abstract	Get or set the priority of the Stark stream(s) associated with the device.
	@discussion	This priority is used when ducking the output stream.
*/

#define kAudioDevicePropertyStarkStreamPriority			0x73747370 // 'stsp'
	// Scope:		kAudioObjectPropertyScopeOutput.
	// Element:		kAudioObjectPropertyElementMaster
	// Qualifier:	none.
	// Data Type:	uint32_t

#define kAudioDevicePropertyStarkStreamPriority_High	0x73706869 // 'sphi'
#define kAudioDevicePropertyStarkStreamPriority_Low		0x73706C6F // 'splo'

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		kAudioDevicePropertyStarkStreamType
	@abstract	Gets the type of streams the device uses.
*/

#define kAudioDevicePropertyStarkStreamType				0x73747374 // 'stst'
	// Scope:		kAudioObjectPropertyScopeGlobal
	// Element:		kAudioObjectPropertyElementMaster
	// Qualifier:	none.
	// Data Type:	uint32_t
	// For gets:	Returns the type of streams the device uses (main audio, alternate audio, etc.).
	// For sets:	Unsupported.

#define kAudioDevicePropertyStarkStreamType_Main		0x73746D6E // 'stmn'
#define kAudioDevicePropertyStarkStreamType_Alternate	0x7374616C // 'stal'

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		kAudioDevicePropertyStarkTransportType
	@abstract	Reports the transport type (e.g. wired, wireless, etc.) that Stark is using.
*/

#define kAudioDevicePropertyStarkTransportType				0x73747474 // 'sttt'
	// Scope:		kAudioObjectPropertyScopeGlobal
	// Element:		kAudioObjectPropertyElementMaster
	// Qualifier:	none.
	// Data Type:	uint32_t
	// For gets:	Returns the type of streams the device uses (main audio, alternate audio, etc.).
	// For sets:	Unsupported.

#define kAudioDevicePropertyStarkTransportType_Wired		0x73747764 // 'stwd'
#define kAudioDevicePropertyStarkTransportType_Wireless		0x7374776C // 'stwl'

//---------------------------------------------------------------------------------------------------------------------------
/*!	@constant	kAudioDeviceTransportTypeStark
	@abstract	Transport type for Stark audio devices.
*/

#define kAudioDeviceTransportTypeStark		0x7374726B // 'strk'

#if 0
#pragma mark == Error Codes ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		AirPlay Errors Codes
	@abstract	Error codes returned by APIs, AuthStatus, etc.
*/

#define kAirPlayUnknownErr					-6700 //! Unknown error occurred.
#define kAirPlayOptionErr					-6701 //! Option was not acceptable.
#define kAirPlaySelectorErr					-6702 //! Selector passed in is invalid or unknown.
#define kAirPlayExecutionStateErr			-6703 //! Call made in the wrong execution state (e.g. called at interrupt time).
#define kAirPlayPathErr						-6704 //! Path is invalid, too long, or otherwise not usable.
#define kAirPlayParamErr					-6705 //! Parameter is incorrect, missing, or not appropriate.
#define kAirPlayParamCountErr				-6706 //! Incorrect or unsupported number of parameters.
#define kAirPlayCommandErr					-6707 //! Command invalid or not supported.
#define kAirPlayIDErr						-6708 //! Unknown, invalid, or inappropriate identifier.
#define kAirPlayStateErr					-6709 //! Not in appropriate state to perform operation.
#define kAirPlayRangeErr					-6710 //! Index is out of range or not valid.
#define kAirPlayRequestErr					-6711 //! Request was improperly formed or not appropriate.
#define kAirPlayResponseErr					-6712 //! Response was incorrect or out of sequence.
#define kAirPlayChecksumErr					-6713 //! Checksum does not match the actual data.
#define kAirPlayNotHandledErr				-6714 //! Operation was not handled (or not handled completely).
#define kAirPlayVersionErr					-6715 //! Version is not correct or not compatible.
#define kAirPlaySignatureErr				-6716 //! Signature did not match what was expected.
#define kAirPlayFormatErr					-6717 //! Unknown, invalid, or inappropriate file/data format.
#define kAirPlayNotInitializedErr			-6718 //! Action request before needed services were initialized.
#define kAirPlayAlreadyInitializedErr		-6719 //! Attempt made to initialize when already initialized.
#define kAirPlayNotInUseErr					-6720 //! Object not in use (e.g. cannot abort if not already in use).
#define kAirPlayAlreadyInUseErr				-6721 //! Object is in use (e.g. cannot reuse active param blocks).
#define kAirPlayTimeoutErr					-6722 //! Timeout occurred.
#define kAirPlayCanceledErr					-6723 //! Operation canceled (successful cancel).
#define kAirPlayAlreadyCanceledErr			-6724 //! Operation has already been canceled.
#define kAirPlayCannotCancelErr				-6725 //! Operation could not be canceled (maybe already done or invalid).
#define kAirPlayDeletedErr					-6726 //! Object has already been deleted.
#define kAirPlayNotFoundErr					-6727 //! Something was not found.
#define kAirPlayNoMemoryErr					-6728 //! Not enough memory was available to perform the operation.
#define kAirPlayNoResourcesErr				-6729 //! Resources unavailable to perform the operation.
#define kAirPlayDuplicateErr				-6730 //! Duplicate found or something is a duplicate.
#define kAirPlayImmutableErr				-6731 //! Entity is not changeable.
#define kAirPlayUnsupportedDataErr			-6732 //! Data is unknown or not supported.
#define kAirPlayIntegrityErr				-6733 //! Data is corrupt.
#define kAirPlayIncompatibleErr				-6734 //! Data is not compatible or it is in an incompatible format.
#define kAirPlayUnsupportedErr				-6735 //! Feature or option is not supported.
#define kAirPlayUnexpectedErr				-6736 //! Error occurred that was not expected.
#define kAirPlayValueErr					-6737 //! Value is not appropriate.
#define kAirPlayNotReadableErr				-6738 //! Could not read or reading is not allowed.
#define kAirPlayNotWritableErr				-6739 //! Could not write or writing is not allowed.
#define kAirPlayBadReferenceErr				-6740 //! An invalid or inappropriate reference was specified.
#define kAirPlayFlagErr						-6741 //! An invalid, inappropriate, or unsupported flag was specified.
#define kAirPlayMalformedErr				-6742 //! Something was not formed correctly.
#define kAirPlaySizeErr						-6743 //! Size was too big, too small, or not appropriate.
#define kAirPlayNameErr						-6744 //! Name was not correct, allowed, or appropriate.
#define kAirPlayNotPreparedErr				-6745 //! Device or service is not ready.
#define kAirPlayReadErr						-6746 //! Could not read.
#define kAirPlayWriteErr					-6747 //! Could not write.
#define kAirPlayMismatchErr					-6748 //! Something does not match.
#define kAirPlayDateErr						-6749 //! Date is invalid or out-of-range.
#define kAirPlayUnderrunErr					-6750 //! Less data than expected.
#define kAirPlayOverrunErr					-6751 //! More data than expected.
#define kAirPlayEndingErr					-6752 //! Connection, session, or something is ending.
#define kAirPlayConnectionErr				-6753 //! Connection failed or could not be established.
#define kAirPlayAuthenticationErr			-6754 //! Authentication failed or is not supported.
#define kAirPlayOpenErr						-6755 //! Could not open file, pipe, device, etc.
#define kAirPlayTypeErr						-6756 //! Incorrect or incompatible type (e.g. file, data, etc.).
#define kAirPlaySkipErr						-6757 //! Items should be or was skipped.
#define kAirPlayNoAckErr					-6758 //! No acknowledge.
#define kAirPlayCollisionErr				-6759 //! Collision occurred (e.g. two on bus at same time).
#define kAirPlayBackoffErr					-6760 //! Backoff in progress and operation intentionally failed.
#define kAirPlayNoAddressAckErr				-6761 //! No acknowledge of address.
#define kAirPlayInternalErr					-6762 //! An error internal to the implementation occurred.
#define kAirPlayNoSpaceErr					-6763 //! Not enough space to perform operation.
#define kAirPlayCountErr					-6764 //! Count is incorrect.
#define kAirPlayEndOfDataErr				-6765 //! Reached the end of the data (e.g. recv returned 0).
#define kAirPlayWouldBlockErr				-6766 //! Would need to block to continue (e.g. non-blocking read/write).
#define kAirPlayLookErr						-6767 //! Special case that needs to be looked at (e.g. interleaved data).
#define kAirPlaySecurityRequiredErr			-6768 //! Security is required for the operation (e.g. must use encryption).
#define kAirPlayOrderErr					-6769 //! Order is incorrect.
#define kAirPlayUpgradeErr					-6770 //! Must upgrade.
#define kAirPlayAsyncNoErr					-6771 //! Async operation successfully started and is now in progress.
#define kAirPlayDeprecatedErr				-6772 //! Operation or data is deprecated.
#define kAirPlayPermissionErr				-6773 //! Permission denied.

// Informational 1xx

#define kAirPlayHTTPStatus_Continue									200100 // "Continue"
#define kAirPlayHTTPStatus_SwitchingProtocols						200101 // "Switching Protocols"
#define kAirPlayHTTPStatus_Processing								200102 // "Processing" (WebDAV - RFC 2518)

// Successfull 2xx

#define kAirPlayHTTPStatus_OK										200200 // "OK"
#define kAirPlayHTTPStatus_Created									200201 // "Created"
#define kAirPlayHTTPStatus_Accepted									200202 // "Accepted"
#define kAirPlayHTTPStatus_NonAuthoritativeInfo						200203 // "Non-Authoritative Information"
#define kAirPlayHTTPStatus_NoContent								200204 // "No Content"
#define kAirPlayHTTPStatus_ResetContent								200205 // "Reset Content"
#define kAirPlayHTTPStatus_PartialContent							200206 // "Partial Content"
#define kAirPlayHTTPStatus_MultiStatus								200207 // "Multi-Status" (WebDAV)
#define kAirPlayHTTPStatus_IMUsed									200226 // "IM Used" (Delta Encoding, RFC 3229)
#define kAirPlayHTTPStatus_LowOnStorageSpace						200250 // "Low on Storage Space"

// Redirection 3xx

#define kAirPlayHTTPStatus_MultipleChoices							200300 // "Multiple Choices"
#define kAirPlayHTTPStatus_MovePermanently							200301 // "Moved Permanently"
#define kAirPlayHTTPStatus_Found									200302 // "Found"
#define kAirPlayHTTPStatus_SeeOther									200303 // "See Other"
#define kAirPlayHTTPStatus_NotModified								200304 // "Not Modified"
#define kAirPlayHTTPStatus_UseProxy									200305 // "Use Proxy"
#define kAirPlayHTTPStatus_SwitchProxy								200306 // "Switch Proxy" -- No longer used.
#define kAirPlayHTTPStatus_TemporaryRedirect						200307 // "Temporary Redirect"
#define kAirPlayHTTPStatus_GoingAway								200350 // "Going Away"
#define kAirPlayHTTPStatus_LoadBalancing							200351 // "Load Balancing"

// Client Error 4xx

#define kAirPlayHTTPStatus_BadRequest								200400 // "Bad Request"
#define kAirPlayHTTPStatus_Unauthorized								200401 // "Unauthorized"
#define kAirPlayHTTPStatus_PaymentRequired							200402 // "Payment Required"
#define kAirPlayHTTPStatus_Forbidden								200403 // "Forbidden"
#define kAirPlayHTTPStatus_NotFound									200404 // "Not Found"
#define kAirPlayHTTPStatus_MethodNotAllowed							200405 // "Method Not Allowed"
#define kAirPlayHTTPStatus_NotAcceptable							200406 // "Not Acceptable"
#define kAirPlayHTTPStatus_ProxyAuthenticationRequired				200407 // "Proxy Authentication Required"
#define kAirPlayHTTPStatus_RequestTimeout							200408 // "Request Timeout"
#define kAirPlayHTTPStatus_Conflict									200409 // "Conflict"
#define kAirPlayHTTPStatus_Gone										200410 // "Gone"
#define kAirPlayHTTPStatus_LengthRequired							200411 // "Length Required"
#define kAirPlayHTTPStatus_PreconditionFailed						200412 // "Precondition Failed"
#define kAirPlayHTTPStatus_RequestEntityTooLarge					200413 // "Request Entity Too Large"
#define kAirPlayHTTPStatus_RequestURITooLong						200414 // "Request-URI Too Long"
#define kAirPlayHTTPStatus_UnsupportedMediaType						200415 // "Unsupported Media Type"
#define kAirPlayHTTPStatus_RequestedRangeNotSatisfiable				200416 // "Requested Range Not Satisfiable"
#define kAirPlayHTTPStatus_ExpectationFailed						200417 // "Expectation Failed"
#define kAirPlayHTTPStatus_IamATeapot								200418 // "I'm a teapot" (April Fools joke)
#define kAirPlayHTTPStatus_UnprocessableEntity						200422 // "Unprocessable Entity" (WebDAV)
#define kAirPlayHTTPStatus_Locked									200423 // "Locked" (WebDAV)
#define kAirPlayHTTPStatus_FailedDependency							200424 // "Failed Dependency" (WebDAV)
#define kAirPlayHTTPStatus_UnorderedCollection						200425 // "Unordered Collection"
#define kAirPlayHTTPStatus_UpgradeRequired							200426 // "Upgrade Required"
#define kAirPlayHTTPStatus_RetryWith								200449 // "Retry With"
#define kAirPlayHTTPStatus_BlockedByParentalControls				200450 // "Blocked by Parental Controls"
#define kAirPlayHTTPStatus_ParameterNotUnderstood					200451 // "Parameter Not Understood"
#define kAirPlayHTTPStatus_ConferenceNotFound						200452 // "Conference Not Found"
#define kAirPlayHTTPStatus_NotEnoughBandwidth						200453 // "Not Enough Bandwidth"
#define kAirPlayHTTPStatus_SessionNotFound							200454 // "Session Not Found"
#define kAirPlayHTTPStatus_MethodNotValidInThisState				200455 // "Method Not Valid In This State"
#define kAirPlayHTTPStatus_HeaderFieldNotValid						200456 // "Header Field Not Valid"
#define kAirPlayHTTPStatus_InvalidRange								200457 // "Invalid Range"
#define kAirPlayHTTPStatus_ParameterIsReadOnly						200458 // "Parameter Is Read-Only"
#define kAirPlayHTTPStatus_AggregateOperationNotAllowed				200459 // "Aggregate Operation Not Allowed"
#define kAirPlayHTTPStatus_OnlyAggregateOperationAllowed			200460 // "Only Aggregate Operation Allowed"
#define kAirPlayHTTPStatus_UnsupportedTransport						200461 // "Unsupported Transport"
#define kAirPlayHTTPStatus_DestinationUnreachable					200462 // "Destination Unreachable"
#define kAirPlayHTTPStatus_DestinationProhibited					200463 // "Destination Prohibited"
#define kAirPlayHTTPStatus_DataTransportNotReadyYet					200464 // "Data Transport Not Ready Yet"
#define kAirPlayHTTPStatus_ConnectionAuthorizationRequired			200470 // "Connection Authorization Required"
#define kAirPlayHTTPStatus_ConnectionCredentialsNotAccepted			200471 // "Connection Credentials not accepted"
#define kAirPlayHTTPStatus_FailureToEstablishSecureConnection		200472 // "Failure to establish secure connection"

// Server Error 5xx

#define kAirPlayHTTPStatus_InternalServerError						200500 // "Internal Server Error"
#define kAirPlayHTTPStatus_NotImplemented							200501 // "Not Implemented"
#define kAirPlayHTTPStatus_BadGatway								200502 // "Bad Gateway"
#define kAirPlayHTTPStatus_ServiceUnavailable						200503 // "Service Unavailable"
#define kAirPlayHTTPStatus_GatewayTimeout							200504 // "Gateway Timeout"
#define kAirPlayHTTPStatus_VersionNotSupported						200505 // "Version Not Supported"
#define kAirPlayHTTPStatus_VariantAlsoNegotiates					200506 // "Variant Also Negotiates"
#define kAirPlayHTTPStatus_InsufficientStorage						200507 // "Insufficient Storage" (WebDAV)
#define kAirPlayHTTPStatus_BandwidthLimitExceeded					200509 // "Bandwidth Limit Exceeded"
#define kAirPlayHTTPStatus_NotExtended								200510 // "Not Extended"
#define kAirPlayHTTPStatus_OptionNotSupported						200551 // "Option Not Supported"

#if( !defined( AirPlayIsBusyError ) )
	#define AirPlayIsBusyError( X ) ( \
		( (X) == kAirPlayHTTPStatus_NotEnoughBandwidth ) || \
		( (X) == kHTTPStatus_NotEnoughBandwidth ) || \
		( (X) == kAlreadyInUseErr ) )
#endif

#if 0
#pragma mark == Misc ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		Misc Constants
	@abstract	Properties for controlling an audio device.
*/

#define kAudioDeviceTransportTypeAirTunes		0x61697274  // 'airt'

#define kAudioDevicePropertyFlush				0x666C7573 //! 'flus' [Float64] Sample time to flush up to.
	// Any audio samples up to the specified sample time must be immediately dropped.

#define kAudioDevicePropertyDataSourceQuiesce	0x71756965  // 'quie' Change posted to quiesce a DataSource. mElement=Item ID.

#if 0
#pragma mark == Notifications ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		Notifications
	@abstract	Notifications posted by AirTunes via the libnotify API.
*/

#define kAirTunesNotification_VolumeUp					"com.apple.AirTunes.DACP.volumeup"
#define kAirTunesNotification_VolumeDown				"com.apple.AirTunes.DACP.volumedown"
#define kAirTunesNotification_DeviceVolume				"com.apple.AirTunes.DACP.devicevolume" // [Float64 0.0-1.0] Current volume of endpoint.

#define kAirTunesNotification_NextItem					"com.apple.AirTunes.DACP.nextitem"
#define kAirTunesNotification_PrevItem					"com.apple.AirTunes.DACP.previtem"

#define kAirTunesNotification_Pause						"com.apple.AirTunes.DACP.pause"
#define kAirTunesNotification_Play						"com.apple.AirTunes.DACP.play"

#define kAirTunesNotification_MuteToggle				"com.apple.AirTunes.DACP.mutetoggle"
#define kAirTunesNotification_RepeatAdvance				"com.apple.AirTunes.DACP.repeatadv"
#define kAirTunesNotification_ShuffleToggle				"com.apple.AirTunes.DACP.shuffletoggle"

#define kAirTunesNotification_DevicePreventPlayback		"com.apple.AirTunes.DACP.device-prevent-playback" // [Integer] 1=Prevent, 0=Allow

#define kAirTunesNotification_EndpointStartError		"com.apple.AirTunes.endpointStartError" // [Integer] Error code.

#endif // __AirTunesAudio_h__
