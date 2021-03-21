#   
#   File:    Platform.include.mk
#   Package: AirPlayAudioPOSIXReceiver
#   Version: AirPlay_Audio_POSIX_Receiver_211.1.p8
#   
#   Disclaimer: IMPORTANT: This Apple software is supplied to you, by Apple Inc. ("Apple"), in your
#   capacity as a current, and in good standing, Licensee in the MFi Licensing Program. Use of this
#   Apple software is governed by and subject to the terms and conditions of your MFi License,
#   including, but not limited to, the restrictions specified in the provision entitled ”Public
#   Software”, and is further subject to your agreement to the following additional terms, and your
#   agreement that the use, installation, modification or redistribution of this Apple software
#   constitutes acceptance of these additional terms. If you do not agree with these additional terms,
#   please do not use, install, modify or redistribute this Apple software.
#   
#   Subject to all of these terms and in consideration of your agreement to abide by them, Apple grants
#   you, for as long as you are a current and in good-standing MFi Licensee, a personal, non-exclusive
#   license, under Apple's copyrights in this original Apple software (the "Apple Software"), to use,
#   reproduce, and modify the Apple Software in source form, and to use, reproduce, modify, and
#   redistribute the Apple Software, with or without modifications, in binary form. While you may not
#   redistribute the Apple Software in source form, should you redistribute the Apple Software in binary
#   form, you must retain this notice and the following text and disclaimers in all such redistributions
#   of the Apple Software. Neither the name, trademarks, service marks, or logos of Apple Inc. may be
#   used to endorse or promote products derived from the Apple Software without specific prior written
#   permission from Apple. Except as expressly stated in this notice, no other rights or licenses,
#   express or implied, are granted by Apple herein, including but not limited to any patent rights that
#   may be infringed by your derivative works or by other works in which the Apple Software may be
#   incorporated.
#   
#   Unless you explicitly state otherwise, if you provide any ideas, suggestions, recommendations, bug
#   fixes or enhancements to Apple in connection with this software (“Feedback”), you hereby grant to
#   Apple a non-exclusive, fully paid-up, perpetual, irrevocable, worldwide license to make, use,
#   reproduce, incorporate, modify, display, perform, sell, make or have made derivative works of,
#   distribute (directly or indirectly) and sublicense, such Feedback in connection with Apple products
#   and services. Providing this Feedback is voluntary, but if you do provide Feedback to Apple, you
#   acknowledge and agree that Apple may exercise the license granted above without the payment of
#   royalties or further consideration to Participant.
#   
#   The Apple Software is provided by Apple on an "AS IS" basis. APPLE MAKES NO WARRANTIES, EXPRESS OR
#   IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY
#   AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR
#   IN COMBINATION WITH YOUR PRODUCTS.
#   
#   IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL DAMAGES
#   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
#   PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION
#   AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT
#   (INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE
#   POSSIBILITY OF SUCH DAMAGE.
#   
#   Copyright (C) 2012-2014 Apple Inc. All Rights Reserved.
#
#
# Local macros 
#
MDNSROOT        = /mDNSResponder
PLATFORMSRCROOT = $(SRCROOT)/Platform//Platform_NetBSD_Pi

ifeq ($(COMPILATION_TYPE), Cross)
TOOLROOT                    = /Volumes/PiArmNetBSD_OSX_Xtools_03312014/src/obj
ROOT_DIR                    = $(TOOLROOT)/destdir.evbarm
BIN_DIR                     = $(TOOLROOT)/tooldir.Darwin-13.2.0-x86_64/bin/
CROSSPREFIX                 = armv6--netbsdelf-eabihf-
PLATFORM_COMMONFLAGS        += --sysroot=$(ROOT_DIR)
PLATFORM_LINK_FLAGS         += --sysroot=$(ROOT_DIR)
else
ROOT_DIR                    = /
BIN_DIR                     = 
CROSSPREFIX                 = 
endif



##################################################################
#
# PLATFORM DEPENDENT MAKEFILE VARIABLES TO PASS TO AIRPLAY
#
# TO BE MODIFIED AS NEEDED BY CUSTOMER
#  |   |   |   |    |
#  |   |   |   |    |
#  V   V   V   V    V
#

##
# 1. Source root Path Information:
#
#   SRCROOT  : Path to directory where AirPlay source release is located
#
SRCROOT=$(CURDIR)/../..



##
# 2. Toolchain Information
#
#   Information about toolset to be used for the platform being ported to
#
AR              = $(BIN_DIR)$(CROSSPREFIX)ar
CC              = $(BIN_DIR)$(CROSSPREFIX)gcc
CXX             = $(BIN_DIR)$(CROSSPREFIX)g++
LD              = $(BIN_DIR)$(CROSSPREFIX)gcc
NM              = $(BIN_DIR)$(CROSSPREFIX)nm
RANLIB          = $(BIN_DIR)$(CROSSPREFIX)ranlib
STRIP           = $(BIN_DIR)$(CROSSPREFIX)strip


##
# 3. Include Path(s) Information
#
#   list of Inlcude directories for the platform being ported to:
#           - standard header files like stdlib.h etc
#           - dns_sd.h for Bonjour header
#
PLATFORM_INCLUDES   += -I$(PLATFORMSRCROOT)
PLATFORM_INCLUDES   += -I$(MDNSROOT)/mDNSShared



##
# 4. Platform Headers to be hooked in
#
#   List of Platform Header file(s) to be hooked into AirPlay during compilation
#
PLATFORM_HEADERS    += -DCommonServices_PLATFORM_HEADER=\"PlatformCommonServices.h\"



##
# 5. Flags/Conditionals to be enabled based on Platform requirements
#
#
PLATFORM_COMMONFLAGS        += 
PLATFORM_COMMONFLAGS        += -fno-builtin


##
# 6. Directory to put the object files in
#
#
PLATFORM_OBJDIR += ./obj




##
# 7. Warning setting to be used when building AirPlay
#
#   PLATFORM_C_WARNINGS: Warning for c files
#   PLATFORM_CPP_WARNINGS: Warning for c++ files
#
#
PLATFORM_WARNINGS   += -W
PLATFORM_WARNINGS   += -Wall
PLATFORM_WARNINGS   += -Werror
PLATFORM_WARNINGS   += -Wextra
PLATFORM_WARNINGS   += -Wformat
PLATFORM_WARNINGS   += -Wmissing-braces
PLATFORM_WARNINGS   += -Wno-cast-align
PLATFORM_WARNINGS   += -Wparentheses
PLATFORM_WARNINGS   += -Wshadow
PLATFORM_WARNINGS   += -Wsign-compare
PLATFORM_WARNINGS   += -Wswitch
PLATFORM_WARNINGS   += -Wuninitialized
PLATFORM_WARNINGS   += -Wunknown-pragmas
PLATFORM_WARNINGS   += -Wunused-function
PLATFORM_WARNINGS   += -Wunused-label
PLATFORM_WARNINGS   += -Wunused-parameter
PLATFORM_WARNINGS   += -Wunused-value
PLATFORM_WARNINGS   += -Wunused-variable

PLATFORM_C_WARNINGS     += $(PLATFORM_WARNINGS)
PLATFORM_C_WARNINGS     += -Wmissing-prototypes
PLATFORM_C_WARNINGS     += -fgnu89-inline 

PLATFORM_CPP_WARNINGS   += $(PLATFORM_WARNINGS)
PLATFORM_CPP_WARNINGS   += -Wnon-virtual-dtor
PLATFORM_CPP_WARNINGS   += -Woverloaded-virtual


##
# 8. Platform Firmware Version
#
#
PLATFORM_FIRMWARE_VERSION = \"Pi.NetBSD.x\"


##
# 9. Specify endianness of the target. Possible values:
#           -DTARGET_RT_BIG_ENDIAN
#             OR
#           -DTARGET_RT_LITTLE_ENDIAN
#
#
#PLATFORM_COMMONFLAGS        +=  -DTARGET_RT_BIG_ENDIAN
PLATFORM_COMMONFLAGS        += -DTARGET_RT_LITTLE_ENDIAN


##
# 10. Specify whether to enable Platform's Skew Compensation
# 			1 => Platform will do its own Skew Compensation. AirPlay's builtin Skew Compensation disabled
# 			    OR
# 			0 => AirPlay's builtin Skew Compensation will be used
#
#
PLATFORM_SKEW_COMPENSATION_ENABLE = 0

#
#####################   END   ####################################

