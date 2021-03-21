#
#	Copyright (C) 2012-2015 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
#
#	Build options
#	-------------
#	debug		-- 1=Compile in debug code, asserts, etc. 0=Strip out debug code for a release build.
#	openssl		-- 1=Use OpenSSL for AES, SHA*, etc. 0=Compile in AES, SHA*, etc. code directly.
#   sdkptp      -- 1=Use SDK's PTP implementation. 0=Use external PRP implementation.
#	shared		-- 1=Build shared libraries 0=Build static libraries
#	sharedtoolchain -- 1= use shared toolchain for both AirPlay and CoreUtils 0=Use separate toolchains
#	stub		-- 1=Build AudioUtils/ScreenUtils/AudioConverter stub DLLs.
#	verbose		-- 1=Produce verbose output.
#

# Build tools
export TOP_DIR = $(BUILD_TOP_DIR)
include $(TOP_DIR)/Makefile.Inc
include $(TOP_DIR)/Rule.make

# Toolchain directory

TOOLROOT            =

# Path to toolchain sysroot, e.g. $(TOOLROOT)/linux-tools/

ROOT_DIR            =

# Path to toolchain binaries, usually $(ROOT_DIR)/usr/bin

BIN_DIR             =

# Prefix of cross compiler, e.g. arm-linux-gnueabihf-

CROSSPREFIX         = $(PREFIX)

# Toolchain

AR                  = $(BIN_DIR)$(CROSSPREFIX)ar
CC                  = $(BIN_DIR)$(CROSSPREFIX)gcc
CXX                 = $(BIN_DIR)$(CROSSPREFIX)g++
LD                  = $(BIN_DIR)$(CROSSPREFIX)gcc
NM                  = $(BIN_DIR)$(CROSSPREFIX)nm
RANLIB              = $(BIN_DIR)$(CROSSPREFIX)ranlib
STRIP               = $(BIN_DIR)$(CROSSPREFIX)strip

# By default, MapleSDK sets up the toolchain and exports it to AccessorySDK. 
# This behavior can be turned off by passing sharedtoolchain=0

sharedtoolchain     ?= 1
export sharedtoolchain

ifeq ($(sharedtoolchain), 1)
	export AR CC CXX LD NM RANLIB STRIP
endif

os                  ?= linux
stub                ?= 1
shared              ?= 0
sdkptp	            ?= 1

ifeq ($(debug),1)
	# Override "STRIP" to do nothing in debug builds.
	STRIP				= :
endif

ifneq ($(verbose),1)
	quiet				= @
endif

# Build options

ifeq ($(debug),1)
	config				= Debug
else
	config				= Release
endif

# Paths

SRCROOT					= $(CURDIR)/..
BUILDROOT				= $(SRCROOT)/build/$(config)-$(os)
OBJDIR					= $(BUILDROOT)/obj

COREUTILSROOT				?= $(SRCROOT)/AccessorySDK
MDNSROOT				?= $(SRCROOT)/../mDNSResponder

VPATH					+= $(SRCROOT)/Platform
VPATH					+= $(SRCROOT)/PlatformPOSIX
VPATH					+= $(SRCROOT)/Sources
VPATH					+= $(SRCROOT)/Support
VPATH					+= $(SRCROOT)/Support/AppleLossless
VPATH					+= $(SRCROOT)/Support/Aac
VPATH					+= $(SRCROOT)/Transport

ifeq ($(sdkptp),1)
	VPATH				+= $(SRCROOT)/Sources/ptp
	VPATH				+= $(SRCROOT)/Sources/ptp/Common
	VPATH				+= $(SRCROOT)/Sources/ptp/Messages
endif

# Includes

INCLUDES				+= -I$(SRCROOT)/Common
INCLUDES				+= -I$(SRCROOT)/Platform
INCLUDES				+= -I$(SRCROOT)/Sources
INCLUDES				+= -I$(SRCROOT)/Support
INCLUDES				+= -I$(SRCROOT)/Support/AppleLossless
INCLUDES				+= -I$(SRCROOT)/Support/Aac
INCLUDES				+= -I$(SRCROOT)/AccessorySDK/External/Ed25519
INCLUDES				+= -I$(SRCROOT)/AccessorySDK/External/Curve25519
INCLUDES				+= -I$(SRCROOT)/AccessorySDK/External/Ed25519
INCLUDES				+= -I$(SRCROOT)/AccessorySDK/External/GladmanAES
INCLUDES				+= -I$(SRCROOT)/AccessorySDK/External/LibTomMath
INCLUDES				+= -I$(SRCROOT)/AccessorySDK/External/Small25519
INCLUDES				+= -I$(SRCROOT)/AccessorySDK/External/SRP
INCLUDES				+= -I$(BUILD_OUT_DIR)/install/dist/release/include
INCLUDES				+= -I$(SRCROOT)/Transport
INCLUDES				+= -I$(BUILDROOT)
INCLUDES				+= -I$(BUILDROOT)/CoreUtils
INCLUDES				+= -I$(MDNSROOT)/mDNSShared
INCLUDES                                += -I$(RELEASE_DIR)/include -I$(COMMON_INC_DIR)

ifeq ($(sdkptp), 1)
	INCLUDES			+= -I$(SRCROOT)/Sources/ptp
	INCLUDES			+= -I$(SRCROOT)/Sources/ptp/Common
	INCLUDES			+= -I$(SRCROOT)/Sources/ptp/Messages
endif

# Link flags

LINKFLAGS				+= -L$(BUILDROOT)
LINKFLAGS				+= -L$(MDNSROOT)/mDNSPosix/build/prod

# Warning flags

COMMON_WARNINGS			+= -W
COMMON_WARNINGS			+= -Wall
COMMON_WARNINGS			+= -Wempty-body
COMMON_WARNINGS			+= -Werror
COMMON_WARNINGS			+= -Wextra
COMMON_WARNINGS			+= -Wformat
COMMON_WARNINGS			+= -Wmissing-braces
COMMON_WARNINGS			+= -Wmissing-field-initializers
COMMON_WARNINGS			+= -Wno-cast-align
COMMON_WARNINGS			+= -Wno-unused-result
COMMON_WARNINGS			+= -Wparentheses
COMMON_WARNINGS			+= -Wpointer-sign
COMMON_WARNINGS			+= -Wshadow
COMMON_WARNINGS			+= -Wsign-compare
COMMON_WARNINGS			+= -Wswitch
COMMON_WARNINGS			+= -Wuninitialized
COMMON_WARNINGS			+= -Wunknown-pragmas
COMMON_WARNINGS			+= -Wunused-function
COMMON_WARNINGS			+= -Wunused-label
COMMON_WARNINGS			+= -Wunused-parameter
COMMON_WARNINGS			+= -Wunused-value
COMMON_WARNINGS			+= -Wunused-variable

CWARNS					+= $(COMMON_WARNINGS)
CWARNS					+= -Wmissing-prototypes

CXXWARNS				+= $(COMMON_WARNINGS)
CXXWARNS				+= -Wnon-virtual-dtor
CXXWARNS				+= -Woverloaded-virtual

# Preprocessor flags

COMMONFLAGS				+= -D__STDC_LIMIT_MACROS=1
COMMONFLAGS				+= -D_GNU_SOURCE
COMMONFLAGS				+= -DCFL_BINARY_PLISTS=1
COMMONFLAGS				+= -DCFLITE_ENABLED=1
COMMONFLAGS				+= -DDEBUG_CF_OBJECTS_ENABLED=1
COMMONFLAGS				+= -DDEBUG_EXPORT_ERROR_STRINGS=1
COMMONFLAGS                             += -DLINKPLAY
ifeq ($(debug),1)
	COMMONFLAGS		+= -DDEBUG=1
else
	COMMONFLAGS			+= -DNDEBUG
endif
ifneq ($(openssl),1)
	COMMONFLAGS			+= -DAES_UTILS_HAS_GLADMAN_GCM=0
	COMMONFLAGS			+= -DAES_UTILS_USE_GLADMAN_AES=1
	COMMONFLAGS			+= -DTARGET_HAS_MD5_UTILS=1
	COMMONFLAGS			+= -DTARGET_HAS_SHA_UTILS=1
	COMMONFLAGS			+= -DTARGET_NO_OPENSSL=1
	COMMONFLAGS			+= -DUSE_VIA_ACE_IF_PRESENT=0
endif
COMMONFLAGS			+= -DFRAMEWORK_STYLE_INCLUDES=0
COMMONFLAGS			+= -DAIRPLAY_MFI=1
COMMONFLAGS			+= -DAIRPLAY_SCREEN=0
COMMONFLAGS			+= -DAIRPLAY_CONFIG_FILES=0
COMMONFLAGS			+= -DAIRPLAY_GENERAL_AUDIO=0
COMMONFLAGS			+= -DAIRPLAY_P2P_SERVER=0
COMMONFLAGS			+= -DAIRPLAY_AAC_ELD=0
ifeq ($(sdkptp), 1)
	COMMONFLAGS		+= -DPTP_USE_DEDICATED_LOGGING_THREAD=1
endif

# Compiler flags

ifeq ($(debug),1)
	CFLAGS			+= $(INCLUDES) $(COMMONFLAGS) $(CWARNS) -g -O0 -std=c99
	CXXFLAGS		+= $(INCLUDES) $(COMMONFLAGS) $(CXXWARNS) -g -O0
else
	CFLAGS			+= $(INCLUDES) $(COMMONFLAGS) $(CWARNS) -g -Os -std=c99
	CXXFLAGS		+= $(INCLUDES) $(COMMONFLAGS) $(CXXWARNS) -g -Os
endif

# Targets

ifeq ($(shared),1)
	TARGETS				+= $(BUILDROOT)/libAirPlaySupport.so
	TARGETS				+= $(BUILDROOT)/libAirPlay.so
	TARGETS				+= $(BUILDROOT)/libCoreUtils.so
else
	TARGETS				+= $(BUILDROOT)/libAirPlay.a
	TARGETS				+= $(BUILDROOT)/libCoreUtils.a
endif

ifeq ($(stub),1)
	TARGETS 			+= $(BUILDROOT)/airplaydemo
	TARGETS 			+= $(BUILDROOT)/airplayutil
endif


# AirPlay Core
AirPlayCore_OBJS		+= $(OBJDIR)/AirPlayAllocator.o
AirPlayCore_OBJS		+= $(OBJDIR)/AirPlayHomeIntegrationUtilities.o
AirPlayCore_OBJS		+= $(OBJDIR)/AirPlayNetworkAddress.o
AirPlayCore_OBJS		+= $(OBJDIR)/AirPlayReceiverCommand.o
AirPlayCore_OBJS		+= $(OBJDIR)/AirPlayReceiverMetrics.o
AirPlayCore_OBJS		+= $(OBJDIR)/AirPlayReceiverPOSIX.o
AirPlayCore_OBJS		+= $(OBJDIR)/AirPlayReceiverServer.o
AirPlayCore_OBJS		+= $(OBJDIR)/AirPlayReceiverLogRequest.o
AirPlayCore_OBJS		+= $(OBJDIR)/AirPlayReceiverSession.o
AirPlayCore_OBJS		+= $(OBJDIR)/AirPlayReceiverSkewCalculator.o
AirPlayCore_OBJS		+= $(OBJDIR)/AirPlaySessionManager.o
AirPlayCore_OBJS		+= $(OBJDIR)/AirPlayUtils.o
AirPlayCore_OBJS		+= $(OBJDIR)/AirTunesClock.o
AirPlayCore_OBJS		+= $(OBJDIR)/AirTunesDACP.o
AirPlayCore_OBJS		+= $(OBJDIR)/CRamstadASRC.o

ifeq ($(sdkptp), 1)
	AirPlayCore_OBJS	+= $(OBJDIR)/AirTunesSDKPTPClock.o
	AirPlayCore_OBJS	+= $(OBJDIR)/ptpClockContext.o
	AirPlayCore_OBJS	+= $(OBJDIR)/ptpClockDataset.o
	AirPlayCore_OBJS	+= $(OBJDIR)/ptpClockIdentity.o
	AirPlayCore_OBJS	+= $(OBJDIR)/ptpClockQuality.o
	AirPlayCore_OBJS	+= $(OBJDIR)/ptpCondition.o
	AirPlayCore_OBJS	+= $(OBJDIR)/ptpForeignClock.o
	AirPlayCore_OBJS	+= $(OBJDIR)/ptpInterface.o
	AirPlayCore_OBJS	+= $(OBJDIR)/ptpLinkLayerAddress.o
	AirPlayCore_OBJS	+= $(OBJDIR)/ptpList.o
	AirPlayCore_OBJS	+= $(OBJDIR)/ptpLog.o
	AirPlayCore_OBJS	+= $(OBJDIR)/ptpMacAddress.o
	AirPlayCore_OBJS	+= $(OBJDIR)/ptpMemoryPool.o
	AirPlayCore_OBJS	+= $(OBJDIR)/ptpMsgAnnounce.o
	AirPlayCore_OBJS	+= $(OBJDIR)/ptpMsgCommon.o
	AirPlayCore_OBJS	+= $(OBJDIR)/ptpMsgDelayReq.o
	AirPlayCore_OBJS	+= $(OBJDIR)/ptpMsgDelayResp.o
	AirPlayCore_OBJS	+= $(OBJDIR)/ptpMsgFollowUp.o
	AirPlayCore_OBJS	+= $(OBJDIR)/ptpMsgSignalling.o
	AirPlayCore_OBJS	+= $(OBJDIR)/ptpMsgSync.o
	AirPlayCore_OBJS	+= $(OBJDIR)/ptpMutex.o
    AirPlayCore_OBJS    += $(OBJDIR)/ptpNetworkAccessContext.o
	AirPlayCore_OBJS	+= $(OBJDIR)/ptpNetworkInterface.o
	AirPlayCore_OBJS	+= $(OBJDIR)/ptpNetworkPort.o
	AirPlayCore_OBJS	+= $(OBJDIR)/ptpPeripheralDelay.o
	AirPlayCore_OBJS	+= $(OBJDIR)/ptpPortIdentity.o
	AirPlayCore_OBJS	+= $(OBJDIR)/ptpRefCountedObject.o
	AirPlayCore_OBJS	+= $(OBJDIR)/ptpThread.o
	AirPlayCore_OBJS	+= $(OBJDIR)/ptpThreadSafeCallback.o
	AirPlayCore_OBJS	+= $(OBJDIR)/ptpTimerQueue.o
	AirPlayCore_OBJS	+= $(OBJDIR)/ptpTimestamp.o
	AirPlayCore_OBJS	+= $(OBJDIR)/ptpUtils.o
else
	AirPlayCore_OBJS	+= $(OBJDIR)/AirTunesOpenAvnuPTPClock.o
endif

# AirPlay

AirPlay_OBJS		+= $(AirPlayCore_OBJS)

# AirPlay Support
AirPlaySupport_OBJS		+= $(OBJDIR)/AudioUtilsDLL.so.o
AirPlaySupport_OBJS		+= $(OBJDIR)/AudioConverterDLL.so.o

# AirPlayUtil

AirPlayUtil_OBJS		+= $(OBJDIR)/airplayutil.o

# AirPlayDemo

AirPlayDemo_OBJS		+= $(OBJDIR)/AirPlayMainStub.o
AirPlayDemo_OBJS		+= $(OBJDIR)/Platform_linkplay.o
AirPlayDemo_OBJS		+= $(OBJDIR)/AudioConverterStub.o
AirPlayDemo_OBJS		+= $(OBJDIR)/AudioUtilsStub.o
AirPlayDemo_OBJS		+= $(OBJDIR)/AudioPlay.o
AirPlayDemo_OBJS		+= $(OBJDIR)/AudioSupport.o
AirPlayDemo_OBJS		+= $(OBJDIR)/ag_dec.o
AirPlayDemo_OBJS		+= $(OBJDIR)/AppleLosslessDecoder.o
AirPlayDemo_OBJS		+= $(OBJDIR)/BitUtilities.o
AirPlayDemo_OBJS		+= $(OBJDIR)/dp_dec.o
AirPlayDemo_OBJS		+= $(OBJDIR)/matrix_dec.o

AirPlayDemo_OBJS		+= $(OBJDIR)/aac.o

# Audio / Screen

Audio_OBJS				+= $(OBJDIR)/AudioUtilsStub.so.o
Screen_OBJS				+= $(OBJDIR)/ScreenUtilsStub.so.o
AudioConverter_OBJS		+= $(OBJDIR)/AudioConverterStub.so.o

# Libraries

LINKFLAGS				+= -lc
ifeq ($(openssl),1)
LINKFLAGS			+= -L $(BUILD_OUT_DIR)/install/dist/release/lib -lcrypto
endif
LINKFLAGS				+= -ldns_sd
LINKFLAGS				+= -lm -lrt 
LINKFLAGS				+= $(LDFLAGS) -L$(RELEASE_DIR)/lib/ -L$(BUILD_ROMFS_DIR)/lib -lasound 

ifeq ($(BUILD_PLATFORM), INGENIC)
LINKFLAGS		+= -lrt -L$(BUILD_PLATFORM_DIR)/lib  -lnvram -lwmdb -lmvmsg -lgcc_s   
else ifeq ($(BUILD_PLATFORM), AMLOGIC)
LINKFLAGS		+= -L$(BUILD_PLATFORM_DIR)/lib  -lnvram -lwmdb -lmvmsg -lgcc_s -ljson-c -lsamplerate -lwl_util 
else ifeq ($(BUILD_PLATFORM), QUALCOMM)
LINKFLAGS		+= -L$(BUILD_PLATFORM_DIR)/lib  -lnvram -lwmdb
else 
LINKFLAGS		+= -lnvram
endif

LINKFLAGS		+= -lfaad

# Pretty printing

INTERACTIVE := $(shell [ -t 0 ] && echo 1)
ifdef INTERACTIVE
	ColorMagenta=\033[0;35m
	ColorCyan=\033[0;36m
	ColorEnd=\033[0m
endif

# Rules

.PHONY : all
all: $(TARGETS)

$(BUILDROOT)/airplaydemo: $(AirPlayDemo_OBJS) $(BUILDROOT)/libAirPlay.a $(BUILDROOT)/libCoreUtils.a
	@echo -e "Linking ($(os)-$(config)) $(ColorMagenta)$(notdir $@)$(ColorEnd)"
	$(quiet)$(CC) $(LINKFLAGS) -lrt -lpthread -o $@ $^
	#$(quiet)$(STRIP) $@
	@echo -e "$(ColorCyan)=== BUILD COMPLETE: $(notdir $@) ($(os)-$(config))$(ColorEnd)"
 
$(BUILDROOT)/airplayutil: $(AirPlayUtil_OBJS) $(BUILDROOT)/libAirPlay.a $(BUILDROOT)/libCoreUtils.a
	@echo -e "Linking ($(os)-$(config)) $(ColorMagenta)$(notdir $@)$(ColorEnd)"
	$(quiet)$(CC) $(LINKFLAGS) -lpthread -o $@ $^
	#$(quiet)$(STRIP) $@
	@echo -e "$(ColorCyan)=== BUILD COMPLETE: $(notdir $@) ($(os)-$(config))$(ColorEnd)"

# Library rules.

$(BUILDROOT)/libAirPlay.so: $(AirPlay_OBJS) $(BUILDROOT)/libAirPlaySupport.so
	@echo -e "Linking ($(os)-$(config)) $(ColorMagenta)$(notdir $@)$(ColorEnd)"
	$(quiet)$(CC) -shared -Wl,-soname,libAirPlay.so -o $@ $(LINKFLAGS) -lAirPlaySupport $^
	$(quiet)$(STRIP) $@
	@echo -e  "$(ColorCyan)=== BUILD COMPLETE: $(notdir $@) ($(os)-$(config))$(ColorEnd)"

$(BUILDROOT)/libAirPlay.a: $(AirPlay_OBJS)
	@echo -e "Linking ($(os)-$(config)) $(ColorMagenta)$(notdir $@)$(ColorEnd)"
	$(quiet)$(AR) rcs $@ $^
	@echo -e "$(ColorCyan)=== BUILD COMPLETE: $(notdir $@) ($(os)-$(config))$(ColorEnd)"

$(BUILDROOT)/libAudioStream.so: $(Audio_OBJS) $(BUILDROOT)/libAirPlaySupport.so
	@echo -e "Linking ($(os)-$(config)) $(ColorMagenta)$(notdir $@)$(ColorEnd)"
	$(quiet)$(CC) -shared -Wl,-soname,libAudioStream.so -o $@ $(LINKFLAGS) -lAirPlaySupport $^
	$(quiet)$(STRIP) $@
	@echo -e "$(ColorCyan)=== BUILD COMPLETE: $(notdir $@) ($(os)-$(config))$(ColorEnd)"

$(BUILDROOT)/libAirPlaySupport.so: $(AirPlaySupport_OBJS)
	@echo -e "Linking ($(os)-$(config)) $(ColorMagenta)$(notdir $@)$(ColorEnd)"
	$(quiet)$(CC) -shared -Wl,-soname,libAirPlaySupport.so -o $@ $(LINKFLAGS) $^
	$(quiet)$(STRIP) $@
	@echo -e "$(ColorCyan)=== BUILD COMPLETE: $(notdir $@) ($(os)-$(config))$(ColorEnd)"

$(BUILDROOT)/libScreenStream.so: $(Screen_OBJS) $(BUILDROOT)/libAirPlaySupport.so
	@echo -e "Linking ($(os)-$(config)) $(ColorMagenta)$(notdir $@)$(ColorEnd)"
	$(quiet)$(CC) -shared -Wl,-soname,libScreenStream.so -o $@ $(LINKFLAGS) -lAirPlaySupport $^
	$(quiet)$(STRIP) $@
	@echo -e "$(ColorCyan)=== BUILD COMPLETE: $(notdir $@) ($(os)-$(config))$(ColorEnd)"

$(BUILDROOT)/libAudioConverter.so: $(AudioConverter_OBJS) $(BUILDROOT)/libAirPlaySupport.so
	@echo -e "Linking ($(os)-$(config)) $(ColorMagenta)$(notdir $@)$(ColorEnd)"
	$(quiet)$(CC) -shared -Wl,-soname,libAudioConverter.so -o $@ $(LINKFLAGS) -lAirPlaySupport $^
	$(quiet)$(STRIP) $@
	@echo -e "$(ColorCyan)=== BUILD COMPLETE: $(notdir $@) ($(os)-$(config))$(ColorEnd)"
	
.PHONY : clean
clean:
	-rm -rf $(SRCROOT)/build/

$(AirPlay_OBJS):		| $(BUILDROOT)/obj $(BUILDROOT)/CoreUtils
$(AirPlayDemo_OBJS):	| $(BUILDROOT)/obj $(BUILDROOT)/CoreUtils
$(AirPlaySupport_OBJS):	| $(BUILDROOT)/obj $(BUILDROOT)/CoreUtils
$(AirPlayUtil_OBJS):	| $(BUILDROOT)/obj $(BUILDROOT)/CoreUtils
$(Audio_OBJS):			| $(BUILDROOT)/obj $(BUILDROOT)/CoreUtils
$(Screen_OBJS):			| $(BUILDROOT)/obj $(BUILDROOT)/CoreUtils
$(AudioConverter_OBJS):	| $(BUILDROOT)/obj $(BUILDROOT)/CoreUtils

$(BUILDROOT)/obj:
	$(quiet)mkdir -p $@

# General rules

$(OBJDIR)/%.o: %.c
	@echo -e "Compiling ($(os)-$(config)-direct) $(ColorMagenta)$(notdir $<)$(ColorEnd)"
	$(quiet)$(CC) -c $(CPPFLAGS) $(CFLAGS) -o $@ $<

$(OBJDIR)/%.o: %.cpp
	@echo -e "Compiling ($(os)-$(config)-direct) $(ColorMagenta)$(notdir $<)$(ColorEnd)"
	$(quiet)$(CXX) -c $(CXXFLAGS) -o $@ $<

$(OBJDIR)/%.so.o: %.c
	@echo -e "Compiling ($(os)-$(config)-shared) $(ColorMagenta)$(notdir $<)$(ColorEnd)"
	$(quiet)$(CC) -c -fPIC $(CPPFLAGS) $(CFLAGS) -o $@ $<

$(OBJDIR)/%.so.o: %.cpp
	@echo -e "Compiling ($(os)-$(config)-shared) $(ColorMagenta)$(notdir $<)$(ColorEnd)"
	$(quiet)$(CXX) -c -fPIC $(CXXFLAGS) -o $@ $<

# Core Utils

arm					= 1
ifeq ($(openssl),1)
	libtommath		= 0
else
	libtommath		= 1
endif	
mfi					= 1

export arch arm debug libtommath mfi openssl os shared
export BUILDROOT
ifeq ($(debug),1)
	export STRIP
endif

.PHONY : $(BUILDROOT)/libCoreUtils.so
$(BUILDROOT)/libCoreUtils.so:
	@echo -e "Making ($(os)-$(config)) $(ColorMagenta)$(notdir $@)$(ColorEnd)"
	$(quiet)$(MAKE) -C $(COREUTILSROOT)/PlatformPOSIX -e CoreUtils
	@echo -e "$(ColorCyan)=== MAKE COMPLETE: $(notdir $@) ($(os)-$(config))$(ColorEnd)"

.PHONY : $(BUILDROOT)/libCoreUtils.a
$(BUILDROOT)/libCoreUtils.a:
	@echo -e "Making ($(os)-$(config)) $(ColorMagenta)$(notdir $@)$(ColorEnd)"
	$(quiet)$(MAKE) -C $(COREUTILSROOT)/PlatformPOSIX -e CoreUtilsStatic
	@echo -e "$(ColorCyan)=== MAKE COMPLETE: $(notdir $@) ($(os)-$(config))$(ColorEnd)"
	
.PHONY : $(BUILDROOT)/CoreUtils
$(BUILDROOT)/CoreUtils:
	@echo -e "Copying CoreUtils headers"
	$(quiet)mkdir -p $@
	$(quiet)rsync -ap "$(COREUTILSROOT)/Support/"*.h "$(BUILDROOT)/CoreUtils"
	$(quiet)rsync -ap "$(COREUTILSROOT)/External/GladmanAES/"*.h "$(BUILDROOT)/CoreUtils"
	@echo -e "$(ColorCyan)=== COPY COMPLETE: $(notdir $@) ($(os)-$(config))$(ColorEnd)"

# Bonjour 

.PHONY : bonjour clean_bonjour
bonjour clean_bonjour:
	@echo -e "Making ($(os)-$(config)) $(ColorMagenta)$(notdir $@)$(ColorEnd)"
	$(quiet)$(MAKE) -C $(COREUTILSROOT)/PlatformPOSIX -e $@
	@echo -e "$(ColorCyan)=== MAKE COMPLETE: $(notdir $@) ($(os)-$(config))$(ColorEnd)"
