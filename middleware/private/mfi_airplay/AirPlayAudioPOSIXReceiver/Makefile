#
#	File:    Makefile
#	Package: AirPlayAudioPOSIXReceiver
#	Version: AirPlay_Audio_POSIX_Receiver_211.1.p8
#	
#	Disclaimer: IMPORTANT: This Apple software is supplied to you, by Apple Inc. ("Apple"), in your
#	capacity as a current, and in good standing, Licensee in the MFi Licensing Program. Use of this
#	Apple software is governed by and subject to the terms and conditions of your MFi License,
#	including, but not limited to, the restrictions specified in the provision entitled ”Public
#	Software”, and is further subject to your agreement to the following additional terms, and your
#	agreement that the use, installation, modification or redistribution of this Apple software
#	constitutes acceptance of these additional terms. If you do not agree with these additional terms,
#	please do not use, install, modify or redistribute this Apple software.
#	
#	Subject to all of these terms and in consideration of your agreement to abide by them, Apple grants
#	you, for as long as you are a current and in good-standing MFi Licensee, a personal, non-exclusive
#	license, under Apple's copyrights in this original Apple software (the "Apple Software"), to use,
#	reproduce, and modify the Apple Software in source form, and to use, reproduce, modify, and
#	redistribute the Apple Software, with or without modifications, in binary form. While you may not
#	redistribute the Apple Software in source form, should you redistribute the Apple Software in binary
#	form, you must retain this notice and the following text and disclaimers in all such redistributions
#	of the Apple Software. Neither the name, trademarks, service marks, or logos of Apple Inc. may be
#	used to endorse or promote products derived from the Apple Software without specific prior written
#	permission from Apple. Except as expressly stated in this notice, no other rights or licenses,
#	express or implied, are granted by Apple herein, including but not limited to any patent rights that
#	may be infringed by your derivative works or by other works in which the Apple Software may be
#	incorporated.
#	
#	Unless you explicitly state otherwise, if you provide any ideas, suggestions, recommendations, bug
#	fixes or enhancements to Apple in connection with this software (“Feedback”), you hereby grant to
#	Apple a non-exclusive, fully paid-up, perpetual, irrevocable, worldwide license to make, use,
#	reproduce, incorporate, modify, display, perform, sell, make or have made derivative works of,
#	distribute (directly or indirectly) and sublicense, such Feedback in connection with Apple products
#	and services. Providing this Feedback is voluntary, but if you do provide Feedback to Apple, you
#	acknowledge and agree that Apple may exercise the license granted above without the payment of
#	royalties or further consideration to Participant.
#	
#	The Apple Software is provided by Apple on an "AS IS" basis. APPLE MAKES NO WARRANTIES, EXPRESS OR
#	IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY
#	AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR
#	IN COMBINATION WITH YOUR PRODUCTS.
#	
#	IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL DAMAGES
#	(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
#	PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION
#	AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT
#	(INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE
#	POSSIBILITY OF SUCH DAMAGE.
#	
#	Copyright (C) 2012-2014 Apple Inc. All Rights Reserved.
#
#	Build Targets
#	-------------
#				-- 	Build AirPlay Library [default]
#	clean		-- 	Clean AirPlay 
#	help		-- 	Help info	
#
#
#	Build options
#	-------------
#	openssl		-- 	1=Use OpenSSL for AES, SHA*, etc. 
#					0=Compile in AES, SHA*, etc. code directly.
#
#	debug       -- 	1=Compile in debug code, asserts, etc. 
#					0=Strip out debug code for a release build.
#
#	static      -- 	1=Create static library	[default]
#					0=Create dynamic library
#

ifneq ($(platform_makefile), )
include $(platform_makefile)
endif

##################################################################
#
# Platform independent stuff
#
##################################################################


#
# VPaths
#
VPATH			= 
VPATH			+= $(SRCROOT)/External/AppleLossless
VPATH			+= $(SRCROOT)/External/Curve25519
VPATH			+= $(SRCROOT)/External/Ed25519
ifneq ($(openssl),1)
VPATH			+= $(SRCROOT)/External/GladmanAES
endif
VPATH			+= $(SRCROOT)/Sources
VPATH			+= $(SRCROOT)/Support

#
# Include Paths
#
INCLUDES		= 
INCLUDES		+= -I$(SRCROOT)/External
INCLUDES		+= -I$(SRCROOT)/External/AppleLossless
INCLUDES		+= -I$(SRCROOT)/External/Curve25519
ifneq ($(openssl),1)
INCLUDES		+= -I$(SRCROOT)/External/GladmanAES
endif
INCLUDES		+= -I$(SRCROOT)/Sources
INCLUDES		+= -I$(SRCROOT)/Support

# Append includes from platform
INCLUDES		+= $(PLATFORM_INCLUDES)


#
# Object File List
#

# AirPlay
OBJS			= 
OBJS			+= AirPlayReceiverPOSIX.o
OBJS			+= AirPlayReceiverServer.o
OBJS			+= AirPlayReceiverSession.o
OBJS			+= AirPlaySettings.o
OBJS			+= AirPlayUtils.o
OBJS			+= AirTunesClock.o
OBJS			+= AirTunesDACP.o
OBJS			+= AirTunesServer.o
OBJS			+= AirPlayMain.o

# Apple Lossless
OBJS			+= ag_dec.o
OBJS			+= AppleLosslessDecoder.o
OBJS			+= BitUtilities.o
OBJS			+= dp_dec.o
OBJS			+= matrix_dec.o

ifneq ($(openssl),1)
# GladmanAES
OBJS			+= aes_modes.o
OBJS			+= aescrypt.o
OBJS			+= aeskey.o
OBJS			+= aestab.o
OBJS			+= gcm.o
OBJS			+= gf128mul.o
endif

# Support
OBJS			+= AESUtils.o
OBJS			+= AsyncConnection.o
OBJS			+= AudioConverterLite.o
OBJS			+= AudioUtils.o
OBJS			+= Base64Utils.o
OBJS			+= CFCompat.o
OBJS			+= CFLite.o
OBJS			+= CFLiteBinaryPlist.o
OBJS			+= CFLiteRunLoopSelect.o
OBJS			+= CFUtils.o
OBJS			+= CommandLineUtils.o
OBJS			+= curve25519-donna.o
OBJS			+= DataBufferUtils.o
OBJS			+= DebugIPCUtils.o
OBJS			+= APSDebugServices.o
OBJS			+= DispatchLite.o
OBJS			+= APSDMAP.o
OBJS			+= APSDMAPUtils.o
OBJS			+= HTTPClient.o
OBJS			+= HTTPMessage.o
OBJS			+= HTTPNetUtils.o
OBJS			+= HTTPServer.o
OBJS			+= HTTPUtils.o
OBJS			+= MathUtils.o
OBJS			+= MD5Utils.o
OBJS			+= APSMFiSAP.o
OBJS			+= MiscUtils.o
OBJS			+= NetPerf.o
OBJS			+= NetUtils.o
OBJS			+= NTPUtils.o
OBJS			+= RandomNumberUtils.o
OBJS			+= SDPUtils.o
OBJS			+= SHAUtils.o
OBJS			+= StringUtils.o
OBJS			+= SystemUtils.o
OBJS			+= ThreadUtils.o
OBJS			+= TickUtils.o
OBJS			+= TimeUtils.o
OBJS			+= URLUtils.o
OBJS			+= utfconv.o
OBJS			+= UUIDUtils.o
OBJS			+= ifaddrs.o


# ar flags 
LINKFLAGS		= 
LINKFLAGS		+= $(PLATFORM_LINK_FLAGS)


# Flags
COMMONFLAGS		= 
COMMONFLAGS		+= -D__STDC_LIMIT_MACROS=1
COMMONFLAGS		+= -D_GNU_SOURCE
COMMONFLAGS		+= -DAIRPLAY_ALAC=0
COMMONFLAGS		+= -DAUDIO_CONVERTER_ALAC=0
COMMONFLAGS		+= -DAIRPLAY_MFI=1
COMMONFLAGS		+= -DCFCOMPAT_NOTIFICATIONS_ENABLED=0
COMMONFLAGS		+= -DCFL_BINARY_PLISTS=1
COMMONFLAGS		+= -DCFL_XML=0
COMMONFLAGS		+= -DCFLITE_ENABLED=1
COMMONFLAGS		+= -DDEBUG_CF_OBJECTS_ENABLED=1
COMMONFLAGS		+= -DDEBUG_EXPORT_ERROR_STRINGS=1

ifeq ($(debug),1)
COMMONFLAGS		+= -DDEBUG=1 -g
else
COMMONFLAGS		+= -DNDEBUG -Os
endif

ifneq ($(openssl),1)
COMMONFLAGS		+= -DAES_UTILS_HAS_GLADMAN_GCM=1
COMMONFLAGS		+= -DAES_UTILS_USE_GLADMAN_AES=1
COMMONFLAGS		+= -DTARGET_HAS_MD5_UTILS=1
COMMONFLAGS		+= -DTARGET_HAS_SHA_UTILS=1
COMMONFLAGS		+= -DTARGET_NO_OPENSSL=1
COMMONFLAGS		+= -DUSE_VIA_ACE_IF_PRESENT=0
endif


COMMONFLAGS		+= -DAIRPLAY_AUDIO_INPUT=0
COMMONFLAGS		+= -DAIRPLAY_WEB_SERVER=1


# Add Platform specific flags 
COMMONFLAGS		+= $(PLATFORM_HEADERS)
COMMONFLAGS		+= $(PLATFORM_COMMONFLAGS)
#COMMONFLAGS		+= -DAIRPLAY_FIRMWARE_REVISION=$(PLATFORM_FIRMWARE_VERSION)
COMMONFLAGS		+= -DFIRMWARE_CUSTOM_VERSION=$(FIRMWARE_CUSTOM_VERSION)

ifeq ($(PLATFORM_SKEW_COMPENSATION_ENABLE), 1)
COMMONFLAGS		+= -DPLATFORM_SKEW_COMPENSATION_ENABLE
endif

CWARNS			+= $(PLATFORM_C_WARNINGS)
CXXWARNS		+= $(PLATFORM_CPP_WARNINGS)




CFLAGS			+= $(INCLUDES) $(COMMONFLAGS) $(CWARNS)
CFLAGS			+= -std=gnu99
CXXFLAGS		+= $(INCLUDES) $(COMMONFLAGS) $(CXXWARNS)

ifeq ($(static),0)
LIBNAME	= libAirPlay.so 
else
LIBNAME	= libAirPlay.a 
endif

OBJDIR	:= $(realpath $(PLATFORM_OBJDIR))
OBJFILES := $(addprefix $(OBJDIR)/,$(OBJS))


####################################################################
#
# Rules
#
####################################################################

.PHONY : all
all: airplaylib

#
# Target to build airplay library target.
#
airplaylib: check $(OBJDIR)/$(LIBNAME)
	@echo "--------------"
	@echo "BUILD COMPLETE: $(notdir $@)"
	@echo "--------------"


#
# Target to build airplay library binary.
#
$(OBJDIR)/$(LIBNAME): $(OBJFILES)
	@echo "====> Creating AirPlay Library $(notdir $@)"
ifeq ($(static),0)
	$(CC) -shared -o $@ $(filter-out check,$^)
else
	$(AR) rcs $@ $(filter-out check,$^)
endif


$(OBJFILES): | $(OBJDIR)

$(OBJDIR):
	test -d $(OBJDIR) && $(error Error: Specified OBJDIR=$(OBJDIR) does not exist. Exiting...)

$(OBJDIR)/%.o: %.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) -o $@ $<

$(OBJDIR)/%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) -o $@ $<



#
# Target to cleanup 
#
.PHONY : clean
clean: check
	-rm -f $(OBJDIR)/* 


#
# Target to check that the caller of this makefile has passed in the needed information
#
.PHONY: check
check:
ifeq ($(SRCROOT), )
	# Stop and print error - caller has not specified the required makefile variables.
	$(error Error: Caller needs to specify necessary makefile variables. See example in Platform/Platform_OSX_Stubs/)
endif
ifeq ($(OBJDIR), )
	# Stop and print error - caller has not specified the required Object Directory
	$(error Error: Caller needs to specify object directory. See example in Platform/Platform_OSX_Stubs/)
endif
# ifeq ($(platform_makefile),)
#	Stop and print error if the caller has not included the platform specific makefile
#	$(error Caller needs to specify platform specific include makefile. See example in Platform/Platform_OSX_Stubs/ Usage: platform_makefile=<path>/<makefile>)
# endif


#
# Target to print out help info
#
.PHONY : help
help:
	@echo "--------- AirPlay Make Help -----------"
	@echo " To build non-debug mode AirPlay Library:"
	@echo " 	make  platform_makefile=<<path_to_Patform.include.mk>"
	@echo 
	@echo " To clean AirPlay:"
	@echo " 	make  clean"
	@echo "---------------------------------------"

