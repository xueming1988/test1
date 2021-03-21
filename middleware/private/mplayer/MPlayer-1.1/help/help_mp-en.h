// $Revision: 34958 $
// MASTER FILE. Use this file as base for translations.
// Translated files should be sent to the mplayer-DOCS mailing list or
// to the help messages maintainer, see DOCS/tech/MAINTAINERS.
// The header of the translated file should contain credits and contact
// information. Before major releases we will notify all translators to update
// their files. Please do not simply translate and forget this, outdated
// translations quickly become worthless. To help us spot outdated files put a
// note like "sync'ed with help_mp-en.h XXX" in the header of the translation.
// Do NOT translate the above lines, just follow the instructions.

// ========================= MPlayer messages ===========================

// mplayer.c
#define MSGTR_Exiting "\nExiting...\n"
#define MSGTR_ExitingHow "\nExiting... (%s)\n"
#define MSGTR_Exit_quit "Quit"
#define MSGTR_Exit_eof "End of file"
#define MSGTR_Exit_error "Fatal error"
#define MSGTR_IntBySignal "\nMPlayer interrupted by signal %d in module: %s\n"
#define MSGTR_NoHomeDir "Cannot find HOME directory.\n"
#define MSGTR_GetpathProblem "get_path(\"config\") problem\n"
#define MSGTR_CreatingCfgFile "Creating config file: %s\n"
#define MSGTR_CantLoadFont "Cannot load bitmap font '%s'.\n"
#define MSGTR_CantLoadSub "Cannot load subtitles '%s'.\n"
#define MSGTR_DumpSelectedStreamMissing "dump: FATAL: Selected stream missing!\n"
#define MSGTR_CantOpenDumpfile "Cannot open dump file.\n"
#define MSGTR_CoreDumped "Core dumped ;)\n"
#define MSGTR_DumpBytesWrittenPercent "dump: %"PRIu64" bytes written (~%.1f%%)\r"
#define MSGTR_DumpBytesWritten "dump: %"PRIu64" bytes written\r"
#define MSGTR_DumpBytesWrittenTo "dump: %"PRIu64" bytes written to '%s'.\n"
#define MSGTR_FPSnotspecified "FPS not specified in the header or invalid, use the -fps option.\n"
#define MSGTR_TryForceAudioFmtStr "Trying to force audio codec driver family %s...\n"
#define MSGTR_CantFindAudioCodec "Cannot find codec for audio format 0x%X.\n"
#define MSGTR_TryForceVideoFmtStr "Trying to force video codec driver family %s...\n"
#define MSGTR_CantFindVideoCodec "Cannot find codec matching selected -vo and video format 0x%X.\n"
#define MSGTR_CannotInitVO "FATAL: Cannot initialize video driver.\n"
#define MSGTR_CannotInitAO "Could not open/initialize audio device -> no sound.\n"
#define MSGTR_StartPlaying "Starting playback...\n"
#define MSGTR_MemAllocFailed "Not enough memory.\n"
#define MSGTR_Unknown "Unknonw\n"

#define MSGTR_NoGui "MPlayer was compiled WITHOUT GUI support.\n"
#define MSGTR_GuiNeedsX "MPlayer GUI requires X11.\n"
#define MSGTR_Playing "\nPlaying %s.\n"
#define MSGTR_NoSound "Audio: no sound\n"
#define MSGTR_FPSforced "FPS forced to be %5.3f  (ftime: %5.3f).\n"
#define MSGTR_AvailableVideoOutputDrivers "Available video output drivers:\n"
#define MSGTR_AvailableAudioOutputDrivers "Available audio output drivers:\n"
#define MSGTR_AvailableAudioCodecs "Available audio codecs:\n"
#define MSGTR_AvailableVideoCodecs "Available video codecs:\n"
#define MSGTR_AvailableAudioFm "Available (compiled-in) audio codec families/drivers:\n"
#define MSGTR_AvailableVideoFm "Available (compiled-in) video codec families/drivers:\n"
#define MSGTR_AvailableFsType "Available fullscreen layer change modes:\n"
#define MSGTR_CannotReadVideoProperties "Video: Cannot read properties.\n"
#define MSGTR_NoStreamFound "No stream found.\n"
#define MSGTR_ErrorInitializingVODevice "Error opening/initializing the selected video_out (-vo) device.\n"
#define MSGTR_ForcedVideoCodec "Forced video codec: %s\n"
#define MSGTR_ForcedAudioCodec "Forced audio codec: %s\n"
#define MSGTR_Video_NoVideo "Video: no video\n"
#define MSGTR_NotInitializeVOPorVO "\nFATAL: Could not initialize video filters (-vf) or video output (-vo).\n"
#define MSGTR_Paused "  =====  PAUSE  =====" // no more than 23 characters (status line for audio files)
#define MSGTR_PlaylistLoadUnable "\nUnable to load playlist %s.\n"
#define MSGTR_Exit_SIGILL_RTCpuSel \
"- MPlayer crashed by an 'Illegal Instruction'.\n"\
"  It may be a bug in our new runtime CPU-detection code...\n"\
"  Please read DOCS/HTML/en/bugreports.html.\n"
#define MSGTR_Exit_SIGILL \
"- MPlayer crashed by an 'Illegal Instruction'.\n"\
"  It usually happens when you run it on a CPU different than the one it was\n"\
"  compiled/optimized for.\n"\
"  Verify this!\n"
#define MSGTR_Exit_SIGSEGV_SIGFPE \
"- MPlayer crashed by bad usage of CPU/FPU/RAM.\n"\
"  Recompile MPlayer with --enable-debug and make a 'gdb' backtrace and\n"\
"  disassembly. Details in DOCS/HTML/en/bugreports_what.html#bugreports_crash.\n"
#define MSGTR_Exit_SIGCRASH \
"- MPlayer crashed. This shouldn't happen.\n"\
"  It can be a bug in the MPlayer code _or_ in your drivers _or_ in your\n"\
"  gcc version. If you think it's MPlayer's fault, please read\n"\
"  DOCS/HTML/en/bugreports.html and follow the instructions there. We can't and\n"\
"  won't help unless you provide this information when reporting a possible bug.\n"
#define MSGTR_LoadingConfig "Loading config '%s'\n"
#define MSGTR_LoadingProtocolProfile "Loading protocol-related profile '%s'\n"
#define MSGTR_LoadingExtensionProfile "Loading extension-related profile '%s'\n"
#define MSGTR_AddedSubtitleFile "SUB: Added subtitle file (%d): %s\n"
#define MSGTR_RemovedSubtitleFile "SUB: Removed subtitle file (%d): %s\n"
#define MSGTR_ErrorOpeningOutputFile "Error opening file [%s] for writing!\n"
#define MSGTR_RTCDeviceNotOpenable "Failed to open %s: %s (it should be readable by the user.)\n"
#define MSGTR_LinuxRTCInitErrorIrqpSet "Linux RTC init error in ioctl (rtc_irqp_set %lu): %s\n"
#define MSGTR_IncreaseRTCMaxUserFreq "Try adding \"echo %lu > /proc/sys/dev/rtc/max-user-freq\" to your system startup scripts.\n"
#define MSGTR_LinuxRTCInitErrorPieOn "Linux RTC init error in ioctl (rtc_pie_on): %s\n"
#define MSGTR_UsingTimingType "Using %s timing.\n"
#define MSGTR_Getch2InitializedTwice "WARNING: getch2_init called twice!\n"
#define MSGTR_DumpstreamFdUnavailable "Cannot dump this stream - no file descriptor available.\n"
#define MSGTR_CantOpenLibmenuFilterWithThisRootMenu "Can't open libmenu video filter with root menu %s.\n"
#define MSGTR_AudioFilterChainPreinitError "Error at audio filter chain pre-init!\n"
#define MSGTR_LinuxRTCReadError "Linux RTC read error: %s\n"
#define MSGTR_SoftsleepUnderflow "Warning! Softsleep underflow!\n"
#define MSGTR_MasterQuit "Option -udp-slave: exiting because master exited\n"
#define MSGTR_InvalidIP "Option -udp-ip: invalid IP address\n"
#define MSGTR_Forking "Forking...\n"
#define MSGTR_Forked "Forked...\n"
#define MSGTR_CouldntFork "Couldn't fork\n"
#define MSGTR_FilenameTooLong "Filename is too long, can not load file or directory specific config files\n"
#define MSGTR_AudioDeviceStuck "Audio device got stuck!\n"
#define MSGTR_AudioOutputTruncated "Audio output truncated at end.\n"
#define MSGTR_PtsAfterFiltersMissing "pts after filters MISSING\n"
#define MSGTR_CommandLine "CommandLine:"


// codec-cfg.c
#define MSGTR_DuplicateFourcc "duplicated FourCC"
#define MSGTR_TooManyFourccs "too many FourCCs/formats..."
#define MSGTR_ParseError "parse error"
#define MSGTR_ParseErrorFIDNotNumber "parse error (format ID not a number?)"
#define MSGTR_ParseErrorFIDAliasNotNumber "parse error (format ID alias not a number?)"
#define MSGTR_DuplicateFID "duplicated format ID"
#define MSGTR_TooManyOut "too many out..."
#define MSGTR_InvalidCodecName "\ncodec(%s) name is not valid!\n"
#define MSGTR_CodecLacksFourcc "\ncodec(%s) does not have FourCC/format!\n"
#define MSGTR_CodecLacksDriver "\ncodec(%s) does not have a driver!\n"
#define MSGTR_CodecNeedsDLL "\ncodec(%s) needs a 'dll'!\n"
#define MSGTR_CodecNeedsOutfmt "\ncodec(%s) needs an 'outfmt'!\n"
#define MSGTR_CantAllocateComment "Can't allocate memory for comment. "
#define MSGTR_GetTokenMaxNotLessThanMAX_NR_TOKEN "get_token(): max >= MAX_MR_TOKEN!"
#define MSGTR_CantGetMemoryForLine "Can't get memory for 'line': %s\n"
#define MSGTR_CantReallocCodecsp "Can't realloc '*codecsp': %s\n"
#define MSGTR_CodecNameNotUnique "Codec name '%s' isn't unique."
#define MSGTR_CantStrdupName "Can't strdup -> 'name': %s\n"
#define MSGTR_CantStrdupInfo "Can't strdup -> 'info': %s\n"
#define MSGTR_CantStrdupDriver "Can't strdup -> 'driver': %s\n"
#define MSGTR_CantStrdupDLL "Can't strdup -> 'dll': %s"
#define MSGTR_AudioVideoCodecTotals "%d audio & %d video codecs\n"
#define MSGTR_CodecDefinitionIncorrect "Codec is not defined correctly."
#define MSGTR_OutdatedCodecsConf "This codecs.conf is too old and incompatible with this MPlayer release!"

// fifo.c
#define MSGTR_CannotMakePipe "Cannot make PIPE!\n"

// parser-mecmd.c, parser-mpcmd.c
#define MSGTR_NoFileGivenOnCommandLine "'--' indicates no more options, but no filename was given on the command line.\n"
#define MSGTR_TheLoopOptionMustBeAnInteger "The loop option must be an integer: %s\n"
#define MSGTR_UnknownOptionOnCommandLine "Unknown option on the command line: -%s\n"
#define MSGTR_ErrorParsingOptionOnCommandLine "Error parsing option on the command line: -%s\n"
#define MSGTR_InvalidPlayEntry "Invalid play entry %s\n"
#define MSGTR_NotAnMEncoderOption "-%s is not an MEncoder option\n"
#define MSGTR_NoFileGiven "No file given\n"

// m_config.c
#define MSGTR_SaveSlotTooOld "Save slot found from lvl %d is too old: %d !!!\n"
#define MSGTR_InvalidCfgfileOption "The %s option can't be used in a config file.\n"
#define MSGTR_InvalidCmdlineOption "The %s option can't be used on the command line.\n"
#define MSGTR_InvalidSuboption "Error: option '%s' has no suboption '%s'.\n"
#define MSGTR_MissingSuboptionParameter "Error: suboption '%s' of '%s' must have a parameter!\n"
#define MSGTR_MissingOptionParameter "Error: option '%s' must have a parameter!\n"
#define MSGTR_OptionListHeader "\n Name                 Type            Min        Max      Global  CL    Cfg\n\n"
#define MSGTR_TotalOptions "\nTotal: %d options\n"
#define MSGTR_ProfileInclusionTooDeep "WARNING: Profile inclusion too deep.\n"
#define MSGTR_NoProfileDefined "No profiles have been defined.\n"
#define MSGTR_AvailableProfiles "Available profiles:\n"
#define MSGTR_UnknownProfile "Unknown profile '%s'.\n"
#define MSGTR_Profile "Profile %s: %s\n"

// m_property.c
#define MSGTR_PropertyListHeader "\n Name                 Type            Min        Max\n\n"
#define MSGTR_TotalProperties "\nTotal: %d properties\n"

// loader/ldt_keeper.c
#define MSGTR_LOADER_DYLD_Warning "WARNING: Attempting to use DLL codecs but environment variable\n         DYLD_BIND_AT_LAUNCH not set. This will likely crash.\n"

// --- playlist
#define MSGTR_PLAYLIST_Path "Path"
#define MSGTR_PLAYLIST_Selected "Selected files"
#define MSGTR_PLAYLIST_Files "Files"
#define MSGTR_PLAYLIST_DirectoryTree "Directory tree"


// cfg.c
#define MSGTR_UnableToSaveOption "Unable to save option '%s'.\n"


// ======================= audio output drivers ========================

// audio_out.c
#define MSGTR_AO_ALSA9_1x_Removed "audio_out: alsa9 and alsa1x modules were removed, use -ao alsa instead.\n"
#define MSGTR_AO_NoSuchDriver "No such audio driver '%.*s'\n"
#define MSGTR_AO_FailedInit "Failed to initialize audio driver '%s'\n"


// ao_alsa.c
#define MSGTR_AO_ALSA_InvalidMixerIndexDefaultingToZero "[AO_ALSA] Invalid mixer index. Defaulting to 0.\n"
#define MSGTR_AO_ALSA_MixerOpenError "[AO_ALSA] Mixer open error: %s\n"
#define MSGTR_AO_ALSA_MixerAttachError "[AO_ALSA] Mixer attach %s error: %s\n"
#define MSGTR_AO_ALSA_MixerRegisterError "[AO_ALSA] Mixer register error: %s\n"
#define MSGTR_AO_ALSA_MixerLoadError "[AO_ALSA] Mixer load error: %s\n"
#define MSGTR_AO_ALSA_UnableToFindSimpleControl "[AO_ALSA] Unable to find simple control '%s',%i.\n"
#define MSGTR_AO_ALSA_ErrorSettingLeftChannel "[AO_ALSA] Error setting left channel, %s\n"
#define MSGTR_AO_ALSA_ErrorSettingRightChannel "[AO_ALSA] Error setting right channel, %s\n"
#define MSGTR_AO_ALSA_ChannelsNotSupported "[AO_ALSA] %d channels are not supported.\n"
#define MSGTR_AO_ALSA_OpenInNonblockModeFailed "[AO_ALSA] Open in nonblock-mode failed, trying to open in block-mode.\n"
#define MSGTR_AO_ALSA_PlaybackOpenError "[AO_ALSA] Playback open error: %s\n"
#define MSGTR_AO_ALSA_ErrorSetBlockMode "[AL_ALSA] Error setting block-mode %s.\n"
#define MSGTR_AO_ALSA_UnableToGetInitialParameters "[AO_ALSA] Unable to get initial parameters: %s\n"
#define MSGTR_AO_ALSA_UnableToSetAccessType "[AO_ALSA] Unable to set access type: %s\n"
#define MSGTR_AO_ALSA_FormatNotSupportedByHardware "[AO_ALSA] Format %s is not supported by hardware, trying default.\n"
#define MSGTR_AO_ALSA_UnableToSetFormat "[AO_ALSA] Unable to set format: %s\n"
#define MSGTR_AO_ALSA_UnableToSetChannels "[AO_ALSA] Unable to set channels: %s\n"
#define MSGTR_AO_ALSA_UnableToDisableResampling "[AO_ALSA] Unable to disable resampling: %s\n"
#define MSGTR_AO_ALSA_UnableToSetSamplerate2 "[AO_ALSA] Unable to set samplerate-2: %s\n"
#define MSGTR_AO_ALSA_UnableToSetBufferTimeNear "[AO_ALSA] Unable to set buffer time near: %s\n"
#define MSGTR_AO_ALSA_UnableToGetPeriodSize "[AO ALSA] Unable to get period size: %s\n"
#define MSGTR_AO_ALSA_UnableToSetPeriods "[AO_ALSA] Unable to set periods: %s\n"
#define MSGTR_AO_ALSA_UnableToSetHwParameters "[AO_ALSA] Unable to set hw-parameters: %s\n"
#define MSGTR_AO_ALSA_UnableToGetBufferSize "[AO_ALSA] Unable to get buffersize: %s\n"
#define MSGTR_AO_ALSA_UnableToGetSwParameters "[AO_ALSA] Unable to get sw-parameters: %s\n"
#define MSGTR_AO_ALSA_UnableToSetSwParameters "[AO_ALSA] Unable to set sw-parameters: %s\n"
#define MSGTR_AO_ALSA_UnableToGetBoundary "[AO_ALSA] Unable to get boundary: %s\n"
#define MSGTR_AO_ALSA_UnableToSetStartThreshold "[AO_ALSA] Unable to set start threshold: %s\n"
#define MSGTR_AO_ALSA_UnableToSetStopThreshold "[AO_ALSA] Unable to set stop threshold: %s\n"
#define MSGTR_AO_ALSA_UnableToSetSilenceSize "[AO_ALSA] Unable to set silence size: %s\n"
#define MSGTR_AO_ALSA_PcmCloseError "[AO_ALSA] pcm close error: %s\n"
#define MSGTR_AO_ALSA_NoHandlerDefined "[AO_ALSA] No handler defined!\n"
#define MSGTR_AO_ALSA_PcmPrepareError "[AO_ALSA] pcm prepare error: %s\n"
#define MSGTR_AO_ALSA_PcmPauseError "[AO_ALSA] pcm pause error: %s\n"
#define MSGTR_AO_ALSA_PcmDropError "[AO_ALSA] pcm drop error: %s\n"
#define MSGTR_AO_ALSA_PcmResumeError "[AO_ALSA] pcm resume error: %s\n"
#define MSGTR_AO_ALSA_DeviceConfigurationError "[AO_ALSA] Device configuration error."
#define MSGTR_AO_ALSA_PcmInSuspendModeTryingResume "[AO_ALSA] Pcm in suspend mode, trying to resume.\n"
#define MSGTR_AO_ALSA_WriteError "[AO_ALSA] Write error: %s\n"
#define MSGTR_AO_ALSA_TryingToResetSoundcard "[AO_ALSA] Trying to reset soundcard.\n"
#define MSGTR_AO_ALSA_CannotGetPcmStatus "[AO_ALSA] Cannot get pcm status: %s\n"

// ao_plugin.c
#define MSGTR_AO_PLUGIN_InvalidPlugin "[AO PLUGIN] invalid plugin: %s\n"


// ======================= audio filters ================================

// af_scaletempo.c
#define MSGTR_AF_ValueOutOfRange MSGTR_VO_ValueOutOfRange

// af_ladspa.c
#define MSGTR_AF_LADSPA_AvailableLabels "available labels in"
#define MSGTR_AF_LADSPA_WarnNoInputs "WARNING! This LADSPA plugin has no audio inputs.\n  The incoming audio signal will be lost."
#define MSGTR_AF_LADSPA_ErrMultiChannel "Multi-channel (>2) plugins are not supported (yet).\n  Use only mono and stereo plugins."
#define MSGTR_AF_LADSPA_ErrNoOutputs "This LADSPA plugin has no audio outputs."
#define MSGTR_AF_LADSPA_ErrInOutDiff "The number of audio inputs and audio outputs of the LADSPA plugin differ."
#define MSGTR_AF_LADSPA_ErrFailedToLoad "failed to load"
#define MSGTR_AF_LADSPA_ErrNoDescriptor "Couldn't find ladspa_descriptor() function in the specified library file."
#define MSGTR_AF_LADSPA_ErrLabelNotFound "Couldn't find label in plugin library."
#define MSGTR_AF_LADSPA_ErrNoSuboptions "No suboptions specified."
#define MSGTR_AF_LADSPA_ErrNoLibFile "No library file specified."
#define MSGTR_AF_LADSPA_ErrNoLabel "No filter label specified."
#define MSGTR_AF_LADSPA_ErrNotEnoughControls "Not enough controls specified on the command line."
#define MSGTR_AF_LADSPA_ErrControlBelow "%s: Input control #%d is below lower boundary of %0.4f.\n"
#define MSGTR_AF_LADSPA_ErrControlAbove "%s: Input control #%d is above upper boundary of %0.4f.\n"

// format.c
#define MSGTR_AF_FORMAT_UnknownFormat "unknown format "


// ========================== INPUT =========================================
// input.c
#define MSGTR_INPUT_INPUT_ErrCantRegister2ManyCmdFds "Too many command file descriptors, cannot register file descriptor %d.\n"
#define MSGTR_INPUT_INPUT_ErrCantRegister2ManyKeyFds "Too many key file descriptors, cannot register file descriptor %d.\n"
#define MSGTR_INPUT_INPUT_ErrArgMustBeInt "Command %s: argument %d isn't an integer.\n"
#define MSGTR_INPUT_INPUT_ErrArgMustBeFloat "Command %s: argument %d isn't a float.\n"
#define MSGTR_INPUT_INPUT_ErrUnterminatedArg "Command %s: argument %d is unterminated.\n"
#define MSGTR_INPUT_INPUT_ErrUnknownArg "Unknown argument %d\n"
#define MSGTR_INPUT_INPUT_Err2FewArgs "Command %s requires at least %d arguments, we found only %d so far.\n"
#define MSGTR_INPUT_INPUT_ErrReadingCmdFd "Error while reading command file descriptor %d: %s\n"
#define MSGTR_INPUT_INPUT_ErrCmdBufferFullDroppingContent "Command buffer of file descriptor %d is full: dropping content.\n"
#define MSGTR_INPUT_INPUT_ErrInvalidCommandForKey "Invalid command for bound key %s"
#define MSGTR_INPUT_INPUT_ErrSelect "Select error: %s\n"
#define MSGTR_INPUT_INPUT_ErrOnKeyInFd "Error on key input file descriptor %d\n"
#define MSGTR_INPUT_INPUT_ErrDeadKeyOnFd "Dead key input on file descriptor %d\n"
#define MSGTR_INPUT_INPUT_Err2ManyKeyDowns "Too many key down events at the same time\n"
#define MSGTR_INPUT_INPUT_ErrOnCmdFd "Error on command file descriptor %d\n"
#define MSGTR_INPUT_INPUT_ErrReadingInputConfig "Error while reading input config file %s: %s\n"
#define MSGTR_INPUT_INPUT_ErrUnknownKey "Unknown key '%s'\n"
#define MSGTR_INPUT_INPUT_ErrUnfinishedBinding "Unfinished binding %s\n"
#define MSGTR_INPUT_INPUT_ErrBuffer2SmallForKeyName "Buffer is too small for this key name: %s\n"
#define MSGTR_INPUT_INPUT_ErrNoCmdForKey "No command found for key %s"
#define MSGTR_INPUT_INPUT_ErrBuffer2SmallForCmd "Buffer is too small for command %s\n"
#define MSGTR_INPUT_INPUT_ErrWhyHere "What are we doing here?\n"
#define MSGTR_INPUT_INPUT_ErrCantInitJoystick "Can't init input joystick\n"
#define MSGTR_INPUT_INPUT_ErrCantOpenFile "Can't open %s: %s\n"
#define MSGTR_INPUT_INPUT_ErrCantInitAppleRemote "Can't init Apple Remote.\n"


// ========================== LIBMPDEMUX ===================================
// demuxer.c, demux_*.c
#define MSGTR_AudioStreamRedefined "WARNING: Audio stream header %d redefined.\n"
#define MSGTR_VideoStreamRedefined "WARNING: Video stream header %d redefined.\n"
#define MSGTR_TooManyAudioInBuffer "\nToo many audio packets in the buffer: (%d in %d bytes).\n"
#define MSGTR_TooManyVideoInBuffer "\nToo many video packets in the buffer: (%d in %d bytes).\n"
#define MSGTR_MaybeNI "Maybe you are playing a non-interleaved stream/file or the codec failed?\n" \
                      "For AVI files, try to force non-interleaved mode with the -ni option.\n"
#define MSGTR_WorkAroundBlockAlignHeaderBug "AVI: Working around CBR-MP3 nBlockAlign header bug!\n"
#define MSGTR_SwitchToNi "\nBadly interleaved AVI file detected - switching to -ni mode...\n"
#define MSGTR_InvalidAudioStreamNosound "AVI: invalid audio stream ID: %d - ignoring (nosound)\n"
#define MSGTR_InvalidAudioStreamUsingDefault "AVI: invalid video stream ID: %d - ignoring (using default)\n"
#define MSGTR_ON2AviFormat "ON2 AVI format"
#define MSGTR_Detected_XXX_FileFormat "%s file format detected.\n"
#define MSGTR_DetectedAudiofile "Audio file detected.\n"
#define MSGTR_InvalidMPEGES "Invalid MPEG-ES stream??? Contact the author, it may be a bug :(\n"
#define MSGTR_FormatNotRecognized "============ Sorry, this file format is not recognized/supported =============\n"\
                                  "=== If this file is an AVI, ASF or MPEG stream, please contact the author! ===\n"
#define MSGTR_SettingProcessPriority "Setting process priority: %s\n"
#define MSGTR_FilefmtFourccSizeFpsFtime "[V] filefmt:%d  fourcc:0x%X  size:%dx%d  fps:%5.3f  ftime:=%6.4f\n"
#define MSGTR_CannotInitializeMuxer "Cannot initialize muxer."
#define MSGTR_MissingVideoStream "No video stream found.\n"
#define MSGTR_MissingAudioStream "No audio stream found -> no sound.\n"
#define MSGTR_MissingVideoStreamBug "Missing video stream!? Contact the author, it may be a bug :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: File doesn't contain the selected audio or video stream.\n"

#define MSGTR_NI_Forced "Forced"
#define MSGTR_NI_Detected "Detected"
#define MSGTR_NI_Message "%s NON-INTERLEAVED AVI file format.\n"

#define MSGTR_UsingNINI "Using NON-INTERLEAVED broken AVI file format.\n"
#define MSGTR_CouldntDetFNo "Could not determine number of frames (for absolute seek).\n"
#define MSGTR_CantSeekRawAVI "Cannot seek in raw AVI streams. (Index required, try with the -idx switch.)\n"
#define MSGTR_CantSeekFile "Cannot seek in this file.\n"

#define MSGTR_MOVcomprhdr "MOV: Compressed headers support requires ZLIB!\n"
#define MSGTR_MOVvariableFourCC "MOV: WARNING: Variable FourCC detected!?\n"
#define MSGTR_MOVtooManyTrk "MOV: WARNING: too many tracks"
#define MSGTR_DetectedTV "TV detected! ;-)\n"
#define MSGTR_ErrorOpeningOGGDemuxer "Unable to open the Ogg demuxer.\n"
#define MSGTR_CannotOpenAudioStream "Cannot open audio stream: %s\n"
#define MSGTR_CannotOpenSubtitlesStream "Cannot open subtitle stream: %s\n"
#define MSGTR_OpeningAudioDemuxerFailed "Failed to open audio demuxer: %s\n"
#define MSGTR_OpeningSubtitlesDemuxerFailed "Failed to open subtitle demuxer: %s\n"
#define MSGTR_TVInputNotSeekable "TV input is not seekable! (Seeking will probably be for changing channels ;)\n"
#define MSGTR_DemuxerInfoChanged "Demuxer info %s changed to %s\n"
#define MSGTR_ClipInfo "Clip info:\n"

#define MSGTR_LeaveTelecineMode "\ndemux_mpg: 30000/1001fps NTSC content detected, switching framerate.\n"
#define MSGTR_EnterTelecineMode "\ndemux_mpg: 24000/1001fps progressive NTSC content detected, switching framerate.\n"

#define MSGTR_CacheFill "\rCache fill: %5.2f%% (%"PRId64" bytes)   "
#define MSGTR_NoBindFound "No bind found for key '%s'.\n"
#define MSGTR_FailedToOpen "Failed to open %s.\n"

#define MSGTR_VideoID "[%s] Video stream found, -vid %d\n"
#define MSGTR_AudioID "[%s] Audio stream found, -aid %d\n"
#define MSGTR_SubtitleID "[%s] Subtitle stream found, -sid %d\n"

// asfheader.c
#define MSGTR_MPDEMUX_ASFHDR_HeaderSizeOver1MB "FATAL: header size bigger than 1 MB (%d)!\nPlease contact MPlayer authors, and upload/send this file.\n"
#define MSGTR_MPDEMUX_ASFHDR_HeaderMallocFailed "Could not allocate %d bytes for header.\n"
#define MSGTR_MPDEMUX_ASFHDR_EOFWhileReadingHeader "EOF while reading ASF header, broken/incomplete file?\n"
#define MSGTR_MPDEMUX_ASFHDR_DVRWantsLibavformat "DVR will probably only work with libavformat, try -demuxer 35 if you have problems\n"
#define MSGTR_MPDEMUX_ASFHDR_NoDataChunkAfterHeader "No data chunk following header!\n"
#define MSGTR_MPDEMUX_ASFHDR_AudioVideoHeaderNotFound "ASF: no audio or video headers found - broken file?\n"
#define MSGTR_MPDEMUX_ASFHDR_InvalidLengthInASFHeader "Invalid length in ASF header!\n"
#define MSGTR_MPDEMUX_ASFHDR_DRMLicenseURL "DRM License URL: %s\n"
#define MSGTR_MPDEMUX_ASFHDR_DRMProtected "This file has been encumbered with DRM encryption, it will not play in MPlayer!\n"

// aviheader.c
#define MSGTR_MPDEMUX_AVIHDR_EmptyList "** empty list?!\n"
#define MSGTR_MPDEMUX_AVIHDR_WarnNotExtendedAVIHdr "** Warning: this is no extended AVI header..\n"
#define MSGTR_MPDEMUX_AVIHDR_BuildingODMLidx "AVI: ODML: Building ODML index (%d superindexchunks).\n"
#define MSGTR_MPDEMUX_AVIHDR_BrokenODMLfile "AVI: ODML: Broken (incomplete?) file detected. Will use traditional index.\n"
#define MSGTR_MPDEMUX_AVIHDR_CantReadIdxFile "Can't read index file %s: %s\n"
#define MSGTR_MPDEMUX_AVIHDR_NotValidMPidxFile "%s is not a valid MPlayer index file.\n"
#define MSGTR_MPDEMUX_AVIHDR_FailedMallocForIdxFile "Could not allocate memory for index data from %s.\n"
#define MSGTR_MPDEMUX_AVIHDR_PrematureEOF "premature end of index file %s\n"
#define MSGTR_MPDEMUX_AVIHDR_IdxFileLoaded "Loaded index file: %s\n"
#define MSGTR_MPDEMUX_AVIHDR_GeneratingIdx "Generating Index: %3lu %s     \r"
#define MSGTR_MPDEMUX_AVIHDR_IdxGeneratedForHowManyChunks "AVI: Generated index table for %d chunks!\n"
#define MSGTR_MPDEMUX_AVIHDR_Failed2WriteIdxFile "Couldn't write index file %s: %s\n"
#define MSGTR_MPDEMUX_AVIHDR_IdxFileSaved "Saved index file: %s\n"

// demux_audio.c
#define MSGTR_MPDEMUX_AUDIO_BadID3v2TagSize "Audio demuxer: bad ID3v2 tag size: larger than stream (%u).\n"
#define MSGTR_MPDEMUX_AUDIO_DamagedAppendedID3v2Tag "Audio demuxer: damaged appended ID3v2 tag detected.\n"
#define MSGTR_MPDEMUX_AUDIO_UnknownFormat "Audio demuxer: unknown format %d.\n"

// demux_demuxers.c
#define MSGTR_MPDEMUX_DEMUXERS_FillBufferError "fill_buffer error: bad demuxer: not vd, ad or sd.\n"

// demux_mkv.c
#define MSGTR_MPDEMUX_MKV_ZlibInitializationFailed "[mkv] zlib initialization failed.\n"
#define MSGTR_MPDEMUX_MKV_ZlibDecompressionFailed "[mkv] zlib decompression failed.\n"
#define MSGTR_MPDEMUX_MKV_LzoInitializationFailed "[mkv] lzo initialization failed.\n"
#define MSGTR_MPDEMUX_MKV_LzoDecompressionFailed "[mkv] lzo decompression failed.\n"
#define MSGTR_MPDEMUX_MKV_TrackEncrypted "[mkv] Track number %u has been encrypted  and decryption has not yet been\n[mkv] implemented. Skipping track.\n"
#define MSGTR_MPDEMUX_MKV_UnknownContentEncoding "[mkv] Unknown content encoding type for track %u. Skipping track.\n"
#define MSGTR_MPDEMUX_MKV_UnknownCompression "[mkv] Track %u has been compressed with an unknown/unsupported compression\n[mkv] algorithm (%u). Skipping track.\n"
#define MSGTR_MPDEMUX_MKV_ZlibCompressionUnsupported "[mkv] Track %u was compressed with zlib but imuzop has not been compiled\n[mkv] with support for zlib compression. Skipping track.\n"
#define MSGTR_MPDEMUX_MKV_TrackIDName "[mkv] Track ID %u: %s (%s) \"%s\", %s\n"
#define MSGTR_MPDEMUX_MKV_TrackID "[mkv] Track ID %u: %s (%s), %s\n"
#define MSGTR_MPDEMUX_MKV_UnknownCodecID "[mkv] Unknown/unsupported CodecID (%s) or missing/bad CodecPrivate\n[mkv] data (track %u).\n"
#define MSGTR_MPDEMUX_MKV_FlacTrackDoesNotContainValidHeaders "[mkv] FLAC track does not contain valid headers.\n"
#define MSGTR_MPDEMUX_MKV_UnknownAudioCodec "[mkv] Unknown/unsupported audio codec ID '%s' for track %u or missing/faulty\n[mkv] private codec data.\n"
#define MSGTR_MPDEMUX_MKV_SubtitleTypeNotSupported "[mkv] Subtitle type '%s' is not supported.\n"
#define MSGTR_MPDEMUX_MKV_WillPlayVideoTrack "[mkv] Will play video track %u.\n"
#define MSGTR_MPDEMUX_MKV_NoVideoTrackFound "[mkv] No video track found/wanted.\n"
#define MSGTR_MPDEMUX_MKV_NoAudioTrackFound "[mkv] No audio track found/wanted.\n"
#define MSGTR_MPDEMUX_MKV_WillDisplaySubtitleTrack "[mkv] Will display subtitle track %u.\n"
#define MSGTR_MPDEMUX_MKV_NoBlockDurationForSubtitleTrackFound "[mkv] Warning: No BlockDuration for subtitle track found.\n"
#define MSGTR_MPDEMUX_MKV_TooManySublines "[mkv] Warning: too many sublines to render, skipping.\n"
#define MSGTR_MPDEMUX_MKV_TooManySublinesSkippingAfterFirst "\n[mkv] Warning: too many sublines to render, skipping after first %i.\n"

// demux_nuv.c
#define MSGTR_MPDEMUX_NUV_NoVideoBlocksInFile "No video blocks in file.\n"

// demux_xmms.c
#define MSGTR_MPDEMUX_XMMS_FoundPlugin "Found plugin: %s (%s).\n"
#define MSGTR_MPDEMUX_XMMS_ClosingPlugin "Closing plugin: %s.\n"
#define MSGTR_MPDEMUX_XMMS_WaitForStart "Waiting for the XMMS plugin to start playback of '%s'...\n"



// ========================== LIBMPCODECS ===================================

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "Could not open codec.\n"
#define MSGTR_CantCloseCodec "Could not close codec.\n"

#define MSGTR_MissingDLLcodec "ERROR: Could not open required DirectShow codec %s.\n"
#define MSGTR_ACMiniterror "Could not load/initialize Win32/ACM audio codec (missing DLL file?).\n"
#define MSGTR_MissingLAVCcodec "Cannot find codec '%s' in libavcodec...\n"

#define MSGTR_MpegNoSequHdr "MPEG: FATAL: EOF while searching for sequence header.\n"
#define MSGTR_CannotReadMpegSequHdr "FATAL: Cannot read sequence header.\n"
#define MSGTR_CannotReadMpegSequHdrEx "FATAL: Cannot read sequence header extension.\n"
#define MSGTR_BadMpegSequHdr "MPEG: bad sequence header\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: bad sequence header extension\n"

#define MSGTR_ShMemAllocFail "Cannot allocate shared memory.\n"
#define MSGTR_CantAllocAudioBuf "Cannot allocate audio out buffer.\n"

#define MSGTR_UnknownAudio "Unknown/missing audio format -> no sound\n"

#define MSGTR_UsingExternalPP "[PP] Using external postprocessing filter, max q = %d.\n"
#define MSGTR_UsingCodecPP "[PP] Using codec's postprocessing, max q = %d.\n"
#define MSGTR_VideoCodecFamilyNotAvailableStr "Requested video codec family [%s] (vfm=%s) not available.\nEnable it at compilation.\n"
#define MSGTR_AudioCodecFamilyNotAvailableStr "Requested audio codec family [%s] (afm=%s) not available.\nEnable it at compilation.\n"
#define MSGTR_OpeningVideoDecoder "Opening video decoder: [%s] %s\n"
#define MSGTR_SelectedVideoCodec "Selected video codec: [%s] vfm: %s (%s)\n"
#define MSGTR_OpeningAudioDecoder "Opening audio decoder: [%s] %s\n"
#define MSGTR_SelectedAudioCodec "Selected audio codec: [%s] afm: %s (%s)\n"
#define MSGTR_VDecoderInitFailed "VDecoder init failed :(\n"
#define MSGTR_ADecoderInitFailed "ADecoder init failed :(\n"
#define MSGTR_ADecoderPreinitFailed "ADecoder preinit failed :(\n"


// vd.c
#define MSGTR_CodecDidNotSet "VDec: Codec did not set sh->disp_w and sh->disp_h, trying workaround.\n"
#define MSGTR_CouldNotFindColorspace "Could not find matching colorspace - retrying with -vf scale...\n"
#define MSGTR_MovieAspectIsSet "Movie-Aspect is %.2f:1 - prescaling to correct movie aspect.\n"
#define MSGTR_MovieAspectUndefined "Movie-Aspect is undefined - no prescaling applied.\n"

// vd_dshow.c, vd_dmo.c
#define MSGTR_DownloadCodecPackage "You need to upgrade/install the binary codecs package.\nGo to http://www.mplayerhq.hu/dload.html\n"

// libmpcodecs/vd_dmo.c vd_dshow.c vd_vfw.c
#define MSGTR_MPCODECS_CouldntAllocateImageForCinepakCodec "[VD_DMO] Couldn't allocate image for cinepak codec.\n"

// libmpcodecs/vd_ffmpeg.c
#define MSGTR_MPCODECS_XVMCAcceleratedCodec "[VD_FFMPEG] XVMC accelerated codec.\n"
#define MSGTR_MPCODECS_ArithmeticMeanOfQP "[VD_FFMPEG] Arithmetic mean of QP: %2.4f, Harmonic mean of QP: %2.4f\n"
#define MSGTR_MPCODECS_DRIFailure "[VD_FFMPEG] DRI failure.\n"
#define MSGTR_MPCODECS_CouldntAllocateImageForCodec "[VD_FFMPEG] Couldn't allocate image for codec.\n"
#define MSGTR_MPCODECS_XVMCAcceleratedMPEG2 "[VD_FFMPEG] XVMC-accelerated MPEG-2.\n"
#define MSGTR_MPCODECS_TryingPixfmt "[VD_FFMPEG] Trying pixfmt=%d.\n"
#define MSGTR_MPCODECS_McGetBufferShouldWorkOnlyWithXVMC "[VD_FFMPEG] The mc_get_buffer should work only with XVMC acceleration!!"
#define MSGTR_MPCODECS_UnexpectedInitVoError "[VD_FFMPEG] Unexpected init_vo error.\n"
#define MSGTR_MPCODECS_UnrecoverableErrorRenderBuffersNotTaken "[VD_FFMPEG] Unrecoverable error, render buffers not taken.\n"
#define MSGTR_MPCODECS_OnlyBuffersAllocatedByVoXvmcAllowed "[VD_FFMPEG] Only buffers allocated by vo_xvmc allowed.\n"


// vf.c
#define MSGTR_CouldNotFindVideoFilter "Couldn't find video filter '%s'.\n"
#define MSGTR_CouldNotOpenVideoFilter "Couldn't open video filter '%s'.\n"
#define MSGTR_OpeningVideoFilter "Opening video filter: "
#define MSGTR_CannotFindColorspace "Cannot find matching colorspace, even by inserting 'scale' :(\n"


// ================================== stream ====================================

// asf_mmst_streaming.c
#define MSGTR_MPDEMUX_MMST_WriteError "write error\n"
#define MSGTR_MPDEMUX_MMST_EOFAlert "\nAlert! EOF\n"
#define MSGTR_MPDEMUX_MMST_PreHeaderReadFailed "pre-header read failed\n"
#define MSGTR_MPDEMUX_MMST_InvalidHeaderSize "Invalid header size, giving up.\n"
#define MSGTR_MPDEMUX_MMST_HeaderDataReadFailed "Header data read failed.\n"
#define MSGTR_MPDEMUX_MMST_packet_lenReadFailed "packet_len read failed.\n"
#define MSGTR_MPDEMUX_MMST_InvalidRTSPPacketSize "Invalid RTSP packet size, giving up.\n"
#define MSGTR_MPDEMUX_MMST_CmdDataReadFailed "Command data read failed.\n"
#define MSGTR_MPDEMUX_MMST_HeaderObject "header object\n"
#define MSGTR_MPDEMUX_MMST_DataObject "data object\n"
#define MSGTR_MPDEMUX_MMST_FileObjectPacketLen "file object, packet length = %d (%d)\n"
#define MSGTR_MPDEMUX_MMST_StreamObjectStreamID "stream object, stream ID: %d\n"
#define MSGTR_MPDEMUX_MMST_2ManyStreamID "Too many IDs, stream skipped."
#define MSGTR_MPDEMUX_MMST_UnknownObject "unknown object\n"
#define MSGTR_MPDEMUX_MMST_MediaDataReadFailed "Media data read failed.\n"
#define MSGTR_MPDEMUX_MMST_MissingSignature "missing signature\n"
#define MSGTR_MPDEMUX_MMST_PatentedTechnologyJoke "Everything done. Thank you for downloading a media file containing proprietary and patented technology.\n"
#define MSGTR_MPDEMUX_MMST_UnknownCmd "unknown command %02x\n"
#define MSGTR_MPDEMUX_MMST_GetMediaPacketErr "get_media_packet error : %s\n"
#define MSGTR_MPDEMUX_MMST_Connected "Connected\n"

// asf_streaming.c
#define MSGTR_MPDEMUX_ASF_StreamChunkSize2Small "Ahhhh, stream_chunck size is too small: %d\n"
#define MSGTR_MPDEMUX_ASF_SizeConfirmMismatch "size_confirm mismatch!: %d %d\n"
#define MSGTR_MPDEMUX_ASF_WarnDropHeader "Warning: drop header ????\n"
#define MSGTR_MPDEMUX_ASF_ErrorParsingChunkHeader "Error while parsing chunk header\n"
#define MSGTR_MPDEMUX_ASF_NoHeaderAtFirstChunk "Didn't get a header as first chunk !!!!\n"
#define MSGTR_MPDEMUX_ASF_BufferMallocFailed "Error: Can't allocate %d bytes buffer.\n"
#define MSGTR_MPDEMUX_ASF_ErrReadingNetworkStream "Error while reading network stream.\n"
#define MSGTR_MPDEMUX_ASF_ErrChunk2Small "Error: Chunk is too small.\n"
#define MSGTR_MPDEMUX_ASF_ErrSubChunkNumberInvalid "Error: Subchunk number is invalid.\n"
#define MSGTR_MPDEMUX_ASF_Bandwidth2SmallCannotPlay "Bandwidth too small, file cannot be played!\n"
#define MSGTR_MPDEMUX_ASF_Bandwidth2SmallDeselectedAudio "Bandwidth too small, deselected audio stream.\n"
#define MSGTR_MPDEMUX_ASF_Bandwidth2SmallDeselectedVideo "Bandwidth too small, deselected video stream.\n"
#define MSGTR_MPDEMUX_ASF_InvalidLenInHeader "Invalid length in ASF header!\n"
#define MSGTR_MPDEMUX_ASF_ErrReadingChunkHeader "Error while reading chunk header.\n"
#define MSGTR_MPDEMUX_ASF_ErrChunkBiggerThanPacket "Error: chunk_size > packet_size\n"
#define MSGTR_MPDEMUX_ASF_ErrReadingChunk "Error while reading chunk.\n"
#define MSGTR_MPDEMUX_ASF_ASFRedirector "=====> ASF Redirector\n"
#define MSGTR_MPDEMUX_ASF_InvalidProxyURL "invalid proxy URL\n"
#define MSGTR_MPDEMUX_ASF_UnknownASFStreamType "unknown ASF stream type\n"
#define MSGTR_MPDEMUX_ASF_Failed2ParseHTTPResponse "Failed to parse HTTP response.\n"
#define MSGTR_MPDEMUX_ASF_ServerReturn "Server returned %d:%s\n"
#define MSGTR_MPDEMUX_ASF_ASFHTTPParseWarnCuttedPragma "ASF HTTP PARSE WARNING : Pragma %s cut from %zu bytes to %zu\n"
#define MSGTR_MPDEMUX_ASF_SocketWriteError "socket write error: %s\n"
#define MSGTR_MPDEMUX_ASF_HeaderParseFailed "Failed to parse header.\n"
#define MSGTR_MPDEMUX_ASF_NoStreamFound "No stream found.\n"
#define MSGTR_MPDEMUX_ASF_UnknownASFStreamingType "unknown ASF streaming type\n"
#define MSGTR_MPDEMUX_ASF_InfoStreamASFURL "STREAM_ASF, URL: %s\n"
#define MSGTR_MPDEMUX_ASF_StreamingFailed "Failed, exiting.\n"

// cache2.c
#define MSGTR_MPDEMUX_CACHE2_NonCacheableStream "\rThis stream is non-cacheable.\n"
#define MSGTR_MPDEMUX_CACHE2_ReadFileposDiffers "!!! read_filepos differs!!! Report this bug...\n"

// network.c
#define MSGTR_MPDEMUX_NW_UnknownAF "Unknown address family %d\n"
#define MSGTR_MPDEMUX_NW_ResolvingHostForAF "Resolving %s for %s...\n"
#define MSGTR_MPDEMUX_NW_CantResolv "Couldn't resolve name for %s: %s\n"
#define MSGTR_MPDEMUX_NW_ConnectingToServer "Connecting to server %s[%s]: %d...\n"
#define MSGTR_MPDEMUX_NW_CantConnect2Server "Failed to connect to server with %s\n"
#define MSGTR_MPDEMUX_NW_SelectFailed "Select failed.\n"
#define MSGTR_MPDEMUX_NW_ConnTimeout "connection timeout\n"
#define MSGTR_MPDEMUX_NW_GetSockOptFailed "getsockopt failed: %s\n"
#define MSGTR_MPDEMUX_NW_ConnectError "connect error: %s\n"
#define MSGTR_MPDEMUX_NW_InvalidProxySettingTryingWithout "Invalid proxy setting... Trying without proxy.\n"
#define MSGTR_MPDEMUX_NW_CantResolvTryingWithoutProxy "Could not resolve remote hostname for AF_INET. Trying without proxy.\n"
#define MSGTR_MPDEMUX_NW_ErrSendingHTTPRequest "Error while sending HTTP request: Didn't send all the request.\n"
#define MSGTR_MPDEMUX_NW_ReadFailed "Read failed.\n"
#define MSGTR_MPDEMUX_NW_Read0CouldBeEOF "http_read_response read 0 (i.e. EOF).\n"
#define MSGTR_MPDEMUX_NW_AuthFailed "Authentication failed. Please use the -user and -passwd options to provide your\n"\
"username/password for a list of URLs, or form an URL like:\n"\
"http://username:password@hostname/file\n"
#define MSGTR_MPDEMUX_NW_AuthRequiredFor "Authentication required for %s\n"
#define MSGTR_MPDEMUX_NW_AuthRequired "Authentication required.\n"
#define MSGTR_MPDEMUX_NW_NoPasswdProvidedTryingBlank "No password provided, trying blank password.\n"
#define MSGTR_MPDEMUX_NW_ErrServerReturned "Server returns %d: %s\n"
#define MSGTR_MPDEMUX_NW_CacheSizeSetTo "Cache size set to %d KBytes\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "CD-ROM Device '%s' not found.\n"
#define MSGTR_ErrTrackSelect "Error selecting VCD track."
#define MSGTR_ReadSTDIN "Reading from stdin...\n"
#define MSGTR_UnableOpenURL "Unable to open URL: %s\n"
#define MSGTR_ConnToServer "Connected to server: %s\n"
#define MSGTR_FileNotFound "File not found: '%s'\n"

#define MSGTR_SMBInitError "Cannot init the libsmbclient library: %d\n"
#define MSGTR_SMBFileNotFound "Could not open from LAN: '%s'\n"
#define MSGTR_SMBNotCompiled "MPlayer was not compiled with SMB reading support.\n"

#define MSGTR_CantOpenBluray "Couldn't open Blu-ray device: %s\n"
#define MSGTR_CantOpenDVD "Couldn't open DVD device: %s (%s)\n"

#define MSGTR_URLParsingFailed "URL parsing failed on url %s\n"
#define MSGTR_FailedSetStreamOption "Failed to set stream option %s=%s\n"
#define MSGTR_StreamNeedType "Streams need a type!\n"
#define MSGTR_StreamProtocolNULL "Stream type %s has protocols == NULL, it's a bug\n"
#define MSGTR_StreamCantHandleURL "No stream found to handle url %s\n"
#define MSGTR_StreamNULLFilename "open_output_stream(), NULL filename, report this bug\n"
#define MSGTR_StreamErrorWritingCapture "Error writing capture file: %s\n"
#define MSGTR_StreamSeekFailed "Seek failed\n"
#define MSGTR_StreamNotSeekable "Stream not seekable!\n"
#define MSGTR_StreamCannotSeekBackward "Cannot seek backward in linear streams!\n"


// stream_cue.c
#define MSGTR_MPDEMUX_CUEREAD_UnexpectedCuefileLine "[bincue] Unexpected cuefile line: %s\n"
#define MSGTR_MPDEMUX_CUEREAD_BinFilenameTested "[bincue] bin filename tested: %s\n"
#define MSGTR_MPDEMUX_CUEREAD_CannotFindBinFile "[bincue] Couldn't find the bin file - giving up.\n"
#define MSGTR_MPDEMUX_CUEREAD_UsingBinFile "[bincue] Using bin file %s.\n"
#define MSGTR_MPDEMUX_CUEREAD_UnknownModeForBinfile "[bincue] unknown mode for binfile. Should not happen. Aborting.\n"
#define MSGTR_MPDEMUX_CUEREAD_CannotOpenCueFile "[bincue] Cannot open %s.\n"
#define MSGTR_MPDEMUX_CUEREAD_ErrReadingFromCueFile "[bincue] Error reading from  %s\n"
#define MSGTR_MPDEMUX_CUEREAD_ErrGettingBinFileSize "[bincue] Error getting size of bin file.\n"
#define MSGTR_MPDEMUX_CUEREAD_InfoTrackFormat "track %02d:  format=%d  %02d:%02d:%02d\n"
#define MSGTR_MPDEMUX_CUEREAD_UnexpectedBinFileEOF "[bincue] unexpected end of bin file\n"
#define MSGTR_MPDEMUX_CUEREAD_CannotReadNBytesOfPayload "[bincue] Couldn't read %d bytes of payload.\n"
#define MSGTR_MPDEMUX_CUEREAD_CueStreamInfo_FilenameTrackTracksavail "CUE stream_open, filename=%s, track=%d, available tracks: %d -> %d\n"


// url.c
#define MSGTR_MPDEMUX_URL_StringAlreadyEscaped "String appears to be already escaped in url_escape %c%c1%c2\n"

// subtitles

