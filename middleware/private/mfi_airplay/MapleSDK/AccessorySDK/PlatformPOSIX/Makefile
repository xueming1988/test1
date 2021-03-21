#
#	Copyright (C) 2013-2015 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
#
#	Build options
#	-------------
#	aes_gcm		-- 1=Include AES-GCM support.
#	arch		-- "arm" for ARMv7 or "x86" for x86.
#	debug		-- 1=Compile in debug code, asserts, etc. 0=Strip out debug code for a release build.
#	libtommath	-- 1=Use LibTomMath for big number math.
#	mfi			-- 1=Enable support for MFi auth IC's.
#	nobonjour	-- 1=Don't include Bonjour support.
#	openssl		-- 1=Use OpenSSL for AES, SHA*, etc. 0=Compile in AES, SHA*, etc. code directly.
#	os			-- "linux" or "qnx".
#	sharedtoolchain -- 1=Use toolchain exported by another makefile 0=Set up toolchain in this makefile
#

# Build tools

# By default, MapleSDK sets up the toolchain and exports it to AccessorySDK.
# This behavior can be turned off by passing sharedtoolchain=0

sharedtoolchain ?= 0

ifeq ($(sharedtoolchain), 0)
	# Toolchain directory

	TOOLROOT                    =

	# Path to toolchain sysroot, e.g. $(TOOLROOT)/linux-tools/

	ROOT_DIR                    =

	# Path to toolchain binaries, usually $(ROOT_DIR)/usr/bin

	BIN_DIR                     =

	# Prefix of cross compiler, e.g. arm-linux-gnueabihf-

	CROSSPREFIX                 =

	# Toolchain
	AR              = $(BIN_DIR)$(CROSSPREFIX)ar
	CC              = $(BIN_DIR)$(CROSSPREFIX)gcc
	CXX             = $(BIN_DIR)$(CROSSPREFIX)g++
	LD              = $(BIN_DIR)$(CROSSPREFIX)gcc
	NM              = $(BIN_DIR)$(CROSSPREFIX)nm
	RANLIB          = $(BIN_DIR)$(CROSSPREFIX)ranlib
	STRIP           = $(BIN_DIR)$(CROSSPREFIX)strip
endif

ifeq ($(debug),1)
	# Override "STRIP" to do nothing in debug builds.
	STRIP				= :
endif

ifeq ($(debug),1)
	config	= Debug
else
	config	= Release
endif

ifeq ($(aes_gcm),1)
	gladman_aes = 1
endif

ifneq ($(openssl),1)
	gladman_aes = 1
endif

# Setup toolchain.
SRCROOT 		= $(CURDIR)/..
BUILDROOT		= $(SRCROOT)/build/$(config)-$(os)-$(arch)
DSTROOTDIR		= $(BUILDROOT)/root.dst
OBJDIR			= $(BUILDROOT)/obj
MDNSROOT		= $(SRCROOT)/../mDNSResponder

# Targets
TARGETS			+= $(BUILDROOT)/aktest
TARGETS			+= $(BUILDROOT)/libCoreUtils.so

# Paths
VPATH			+= $(SRCROOT)/External/Curve25519
VPATH			+= $(SRCROOT)/External/Ed25519
ifeq ($(gladman_aes),1)
	VPATH		+= $(SRCROOT)/External/GladmanAES
endif
ifeq ($(libtommath),1)
	VPATH		+= $(SRCROOT)/External/LibTomMath
endif
VPATH			+= $(SRCROOT)/External/Small25519
VPATH			+= $(SRCROOT)/External/SRP
VPATH			+= $(SRCROOT)/PlatformPOSIX
VPATH			+= $(SRCROOT)/Sources
VPATH			+= $(SRCROOT)/Support

# Includes
INCLUDES		+= -I$(SRCROOT)/External/Curve25519
INCLUDES		+= -I$(SRCROOT)/External/Ed25519
ifeq ($(gladman_aes),1)
	INCLUDES	+= -I$(SRCROOT)/External/GladmanAES
endif
ifeq ($(libtommath),1)
	INCLUDES	+= -I$(SRCROOT)/External/LibTomMath
endif
INCLUDES		+= -I$(SRCROOT)/External/Small25519
INCLUDES		+= -I$(SRCROOT)/External/SRP
INCLUDES		+= -I$(SRCROOT)/Sources
INCLUDES		+= -I$(SRCROOT)/Support

# aktest
aktest_SRCS		+= aktest.c

# Support
CoreUtils_SRCS	+= AESUtils.c
CoreUtils_SRCS	+= AsyncConnection.c
aktest_SRCS		+= AtomicUtils.c
CoreUtils_SRCS	+= Base64Utils.c
CoreUtils_SRCS  += BLAKEUtils.c
ifneq ($(nobonjour),1)
	CoreUtils_SRCS	+= BonjourBrowser.c
endif
CoreUtils_SRCS	+= CFCompat.c
CoreUtils_SRCS	+= CFLite.c
CoreUtils_SRCS	+= CFLiteBinaryPlist.c
aktest_SRCS		+= CFLiteNotifications.c
CoreUtils_SRCS	+= CFLitePreferencesFile.c
CoreUtils_SRCS	+= CFLiteRunLoopSelect.c
CoreUtils_SRCS	+= CFUtils.c
CoreUtils_SRCS	+= ChaCha20Poly1305.c
CoreUtils_SRCS	+= CommandLineUtils.c
CoreUtils_SRCS  += CryptoHashUtils.c
CoreUtils_SRCS	+= curve25519-donna.c
aktest_SRCS		+= curve25519-test.c
CoreUtils_SRCS	+= DataBufferUtils.c
CoreUtils_SRCS	+= DebugIPCUtils.c
CoreUtils_SRCS	+= DebugServices.c
CoreUtils_SRCS	+= DispatchLite.c
CoreUtils_SRCS	+= ed25519.c
aktest_SRCS		+= ed25519-test.c
CoreUtils_SRCS	+= HIDUtils.c
CoreUtils_SRCS	+= HTTPClient.c
CoreUtils_SRCS	+= HTTPMessage.c
CoreUtils_SRCS	+= HTTPServer.c
aktest_SRCS		+= HTTPTest.c
CoreUtils_SRCS	+= HTTPUtils.c
CoreUtils_SRCS	+= KeychainLiteFile.c
CoreUtils_SRCS	+= KeychainUtils.c
CoreUtils_SRCS	+= LogUtils.c
CoreUtils_SRCS	+= MD5Utils.c
CoreUtils_SRCS	+= MathUtils.c
ifeq ($(mfi),1)
	CoreUtils_SRCS += MFiSAP.c
	CoreUtils_SRCS += MFiServerPlatformLinux.c
endif
CoreUtils_SRCS	+= MiscUtils.c
CoreUtils_SRCS	+= NetTransportChaCha20Poly1305.c
CoreUtils_SRCS	+= NetUtils.c
CoreUtils_SRCS	+= NetworkChangeListener.c
CoreUtils_SRCS	+= OPACKUtils.c
CoreUtils_SRCS	+= PairingUtils.c
CoreUtils_SRCS	+= PrintFUtils.c
CoreUtils_SRCS	+= RandomNumberUtils.c
CoreUtils_SRCS	+= SDPUtils.c
CoreUtils_SRCS	+= SHAUtils.c
aktest_SRCS		+= Small25519.c
CoreUtils_SRCS	+= SRPUtils.c
CoreUtils_SRCS	+= StringUtils.c
CoreUtils_SRCS	+= ThreadUtils.c
CoreUtils_SRCS	+= TickUtils.c
CoreUtils_SRCS	+= TimeUtils.c
CoreUtils_SRCS	+= TLVUtils.c
CoreUtils_SRCS	+= URLUtils.c
CoreUtils_SRCS	+= utfconv.c
CoreUtils_SRCS	+= UUIDUtils.c

ifeq ($(gladman_aes),1)
	# GladmanAES
	CoreUtils_SRCS	+= aes_modes.c
	CoreUtils_SRCS	+= aescrypt.c
	CoreUtils_SRCS	+= aeskey.c
	CoreUtils_SRCS	+= aestab.c
endif
ifeq ($(aes_gcm),1)
	CoreUtils_SRCS	+= gcm.c
	CoreUtils_SRCS	+= gf128mul.c
endif

ifeq ($(libtommath),1)
	# LibTomMath
	CoreUtils_SRCS	+= bn_fast_mp_invmod.c
	CoreUtils_SRCS	+= bn_fast_mp_montgomery_reduce.c
	CoreUtils_SRCS	+= bn_fast_s_mp_mul_digs.c
	CoreUtils_SRCS	+= bn_fast_s_mp_mul_high_digs.c
	CoreUtils_SRCS	+= bn_fast_s_mp_sqr.c
	CoreUtils_SRCS	+= bn_mp_2expt.c
	CoreUtils_SRCS	+= bn_mp_abs.c
	CoreUtils_SRCS	+= bn_mp_add_d.c
	CoreUtils_SRCS	+= bn_mp_add.c
	CoreUtils_SRCS	+= bn_mp_clamp.c
	CoreUtils_SRCS	+= bn_mp_clear_multi.c
	CoreUtils_SRCS	+= bn_mp_clear.c
	CoreUtils_SRCS	+= bn_mp_cmp_d.c
	CoreUtils_SRCS	+= bn_mp_cmp_mag.c
	CoreUtils_SRCS	+= bn_mp_cmp.c
	CoreUtils_SRCS	+= bn_mp_cnt_lsb.c
	CoreUtils_SRCS	+= bn_mp_copy.c
	CoreUtils_SRCS	+= bn_mp_count_bits.c
	CoreUtils_SRCS	+= bn_mp_div_2.c
	CoreUtils_SRCS	+= bn_mp_div_2d.c
	CoreUtils_SRCS	+= bn_mp_div_3.c
	CoreUtils_SRCS	+= bn_mp_div_d.c
	CoreUtils_SRCS	+= bn_mp_div.c
	CoreUtils_SRCS	+= bn_mp_dr_is_modulus.c
	CoreUtils_SRCS	+= bn_mp_dr_reduce.c
	CoreUtils_SRCS	+= bn_mp_dr_setup.c
	CoreUtils_SRCS	+= bn_mp_exch.c
	CoreUtils_SRCS	+= bn_mp_exptmod_fast.c
	CoreUtils_SRCS	+= bn_mp_exptmod.c
	CoreUtils_SRCS	+= bn_mp_grow.c
	CoreUtils_SRCS	+= bn_mp_init_copy.c
	CoreUtils_SRCS	+= bn_mp_init_multi.c
	CoreUtils_SRCS	+= bn_mp_init_size.c
	CoreUtils_SRCS	+= bn_mp_init.c
	CoreUtils_SRCS	+= bn_mp_invmod_slow.c
	CoreUtils_SRCS	+= bn_mp_invmod.c
	CoreUtils_SRCS	+= bn_mp_karatsuba_mul.c
	CoreUtils_SRCS	+= bn_mp_karatsuba_sqr.c
	CoreUtils_SRCS	+= bn_mp_lshd.c
	CoreUtils_SRCS	+= bn_mp_mod_2d.c
	CoreUtils_SRCS	+= bn_mp_mod_d.c
	CoreUtils_SRCS	+= bn_mp_mod.c
	CoreUtils_SRCS	+= bn_mp_montgomery_calc_normalization.c
	CoreUtils_SRCS	+= bn_mp_montgomery_reduce.c
	CoreUtils_SRCS	+= bn_mp_montgomery_setup.c
	CoreUtils_SRCS	+= bn_mp_mul_2.c
	CoreUtils_SRCS	+= bn_mp_mul_2.c
	CoreUtils_SRCS	+= bn_mp_mul_2d.c
	CoreUtils_SRCS	+= bn_mp_mul_d.c
	CoreUtils_SRCS	+= bn_mp_mul.c
	CoreUtils_SRCS	+= bn_mp_mulmod.c
	CoreUtils_SRCS	+= bn_mp_prime_is_divisible.c
	CoreUtils_SRCS	+= bn_mp_prime_is_prime.c
	CoreUtils_SRCS	+= bn_mp_prime_miller_rabin.c
	CoreUtils_SRCS	+= bn_mp_prime_rabin_miller_trials.c
	CoreUtils_SRCS	+= bn_mp_read_unsigned_bin.c
	CoreUtils_SRCS	+= bn_mp_reduce_2k_l.c
	CoreUtils_SRCS	+= bn_mp_reduce_2k_setup_l.c
	CoreUtils_SRCS	+= bn_mp_reduce_2k_setup.c
	CoreUtils_SRCS	+= bn_mp_reduce_2k.c
	CoreUtils_SRCS	+= bn_mp_reduce_is_2k_l.c
	CoreUtils_SRCS	+= bn_mp_reduce_is_2k.c
	CoreUtils_SRCS	+= bn_mp_reduce_setup.c
	CoreUtils_SRCS	+= bn_mp_reduce.c
	CoreUtils_SRCS	+= bn_mp_rshd.c
	CoreUtils_SRCS	+= bn_mp_set_int.c
	CoreUtils_SRCS	+= bn_mp_set.c
	CoreUtils_SRCS	+= bn_mp_sqr.c
	CoreUtils_SRCS	+= bn_mp_sqrmod.c
	CoreUtils_SRCS	+= bn_mp_sub_d.c
	CoreUtils_SRCS	+= bn_mp_sub.c
	CoreUtils_SRCS	+= bn_mp_to_unsigned_bin.c
	CoreUtils_SRCS	+= bn_mp_toom_mul.c
	CoreUtils_SRCS	+= bn_mp_toom_sqr.c
	CoreUtils_SRCS	+= bn_mp_unsigned_bin_size.c
	CoreUtils_SRCS	+= bn_mp_zero.c
	CoreUtils_SRCS	+= bn_prime_tab.c
	CoreUtils_SRCS	+= bn_reverse.c
	CoreUtils_SRCS	+= bn_s_mp_add.c
	CoreUtils_SRCS	+= bn_s_mp_exptmod.c
	CoreUtils_SRCS	+= bn_s_mp_mul_digs.c
	CoreUtils_SRCS	+= bn_s_mp_mul_high_digs.c
	CoreUtils_SRCS	+= bn_s_mp_sqr.c
	CoreUtils_SRCS	+= bn_s_mp_sub.c
	CoreUtils_SRCS	+= bncore.c
endif

# SRP
CoreUtils_SRCS	+= cstr.c
CoreUtils_SRCS	+= srp.c
CoreUtils_SRCS	+= srp6_client.c
CoreUtils_SRCS	+= srp6_server.c
CoreUtils_SRCS	+= t_math.c

aktest_SRCS		+= $(CoreUtils_SRCS)

# Map sources to built objects in the obj folder.
aktest_OBJS				= $(patsubst %.c,$(OBJDIR)/%.o,$(aktest_SRCS))
CoreUtils_OBJS_direct	= $(patsubst %.c,$(OBJDIR)/%.o,$(CoreUtils_SRCS))
CoreUtils_OBJS_shared	= $(patsubst %.c,$(OBJDIR)/%.so.o,$(CoreUtils_SRCS))

# Libraries
LINKFLAGS		+= -L$(BUILDROOT)

LIBS			+= -lc
ifeq ($(openssl),1)
	LIBS		+= -lcrypto
endif
ifneq ($(nobonjour),1)
	LIBS		+= -ldns_sd
endif
LIBS			+= -lm
LIBS		+= -lpthread
LIBS		+= -lrt

# Bonjour additions for projects building it directly.
INCLUDES		+= -I$(MDNSROOT)/mDNSShared
LINKFLAGS		+= -L$(MDNSROOT)/mDNSPosix/build/prod

# Flags
COMMONFLAGS		+= -D_GNU_SOURCE
COMMONFLAGS		+= -DDEBUG_EXPORT_ERROR_STRINGS=1
COMMONFLAGS		+= -DFRAMEWORK_STYLE_INCLUDES=0
COMMONFLAGS		+= -DTARGET_HAS_NATIVE_INT128=0
COMMONFLAGS		+= -DTARGET_NEEDS_NATURAL_ALIGNMENT=1
#COMMONFLAGS		+= -DMFI_AUTH_DEVICE_ADDRESS=0x10
#COMMONFLAGS		+= -DMFI_SAP_SERVER_AUTHV3=1
#COMMONFLAGS		+= -DEXCLUDE_UNIT_TESTS=1

ifeq ($(debug),1)
	COMMONFLAGS	+= -DDEBUG=1
else
	COMMONFLAGS	+= -DNDEBUG=1
endif
ifeq ($(mfi),1)
endif
ifeq ($(nobonjour),1)
	COMMONFLAGS	+= -D_DNS_SD_LIBDISPATCH=0
	COMMONFLAGS	+= -DASYNC_CONNECTION_BONJOUR=0
	COMMONFLAGS	+= -DBONJOUR_ENABLED=0
endif
ifeq ($(libtommath),1)
	COMMONFLAGS	+= -DTARGET_HAS_LIBTOMMATH=1
endif
ifeq ($(aes_gcm),1)
	COMMONFLAGS	+= -DAES_UTILS_HAS_GLADMAN_GCM=1
else
	COMMONFLAGS	+= -DAES_UTILS_HAS_GLADMAN_GCM=0
endif
ifeq ($(gladman_aes),1)
	COMMONFLAGS	+= -DAES_UTILS_USE_GLADMAN_AES=1
	COMMONFLAGS	+= -DUSE_VIA_ACE_IF_PRESENT=0
endif
ifneq ($(openssl),1)
	COMMONFLAGS	+= -DTARGET_HAS_MD5_UTILS=1
	COMMONFLAGS	+= -DTARGET_HAS_SHA_UTILS=1
	COMMONFLAGS	+= -DTARGET_NO_OPENSSL=1
else
	COMMONFLAGS	+= -DTARGET_NO_OPENSSL=0
	COMMONFLAGS	+= -DTARGET_HAS_COMMON_CRYPTO=0
endif
COMMONFLAGS             += -DAIRPLAY=1
COMMONFLAGS             += -DAIRPLAY_SDK=1

ifneq ($(qnxold),1)
	COMMON_WARNINGS	+= -pedantic
	COMMON_WARNINGS	+= -std=gnu99
endif
COMMON_WARNINGS	+= -W
COMMON_WARNINGS	+= -Wall
COMMON_WARNINGS	+= -Wextra

COMMON_WARNINGS	+= -Wempty-body
COMMON_WARNINGS	+= -Werror
COMMON_WARNINGS	+= -Wformat
COMMON_WARNINGS	+= -Wmissing-braces
COMMON_WARNINGS	+= -Wmissing-field-initializers
COMMON_WARNINGS	+= -Wparentheses
COMMON_WARNINGS	+= -Wpointer-sign
COMMON_WARNINGS	+= -Wshadow
COMMON_WARNINGS	+= -Wsign-compare
COMMON_WARNINGS	+= -Wswitch
COMMON_WARNINGS	+= -Wundef
COMMON_WARNINGS	+= -Wuninitialized
COMMON_WARNINGS	+= -Wunknown-pragmas
COMMON_WARNINGS	+= -Wunused-function
COMMON_WARNINGS	+= -Wunused-label
COMMON_WARNINGS	+= -Wunused-parameter
COMMON_WARNINGS	+= -Wunused-value
COMMON_WARNINGS	+= -Wunused-variable

COMMON_WARNINGS	+= -Wno-cast-align
COMMON_WARNINGS	+= -Wno-unused-result

CWARNS			+= $(COMMON_WARNINGS)
CWARNS			+= -Wmissing-prototypes
CXXWARNS		+= $(COMMON_WARNINGS)
CXXWARNS		+= -Wnon-virtual-dtor
CXXWARNS		+= -Woverloaded-virtual

ifeq ($(debug),1)
	CFLAGS			+= $(INCLUDES) $(COMMONFLAGS) $(CWARNS) -g -O0
CFLAGS_direct	+= -DSNPRINTF_USE_ASSERTS=0
	CXXFLAGS		+= $(INCLUDES) $(COMMONFLAGS) $(CXXWARNS) -g -O0
else
	CFLAGS			+= $(INCLUDES) $(COMMONFLAGS) $(CWARNS) -g -Os
	CFLAGS_direct	+= -DSNPRINTF_USE_ASSERTS=0
	CXXFLAGS		+= $(INCLUDES) $(COMMONFLAGS) $(CXXWARNS) -g -Os
endif

# Pretty printing
INTERACTIVE := $(shell [ -t 0 ] && echo 1)
ifdef INTERACTIVE
	ColorMagenta=\033[0;35m
	ColorCyan=\033[0;36m
	ColorEnd=\033[0m
endif

ifneq ($(verbose),1)
	quiet	= @
endif

# Rules
.PHONY : all
all: $(TARGETS)


# aktest
.PHONY : aktest
aktest : $(BUILDROOT)/aktest

$(BUILDROOT)/aktest: $(aktest_OBJS)
	@echo -e  "Linking ($(os)-$(arch)-$(config)) $(ColorMagenta)$(notdir $@)$(ColorEnd)"
	$(quiet)$(CC) $(LINKFLAGS) -o $@ $^ $(LIBS)
	@echo -e  "$(ColorCyan)=== BUILD COMPLETE: $(notdir $@) ($(os)-$(arch)-$(config))$(ColorEnd)"

.PHONY : install
install: $(TARGETS)
	$(quiet)mkdir -p "$(CROSSROOT)/usr/include/CoreUtils"
	$(quiet)rsync -avp $(SRCROOT)/External/Curve25519/curve25519-donna.h "$(CROSSROOT)/usr/include/CoreUtils/"
	$(quiet)rsync -avp $(SRCROOT)/External/Ed25519/ed25519.h "$(CROSSROOT)/usr/include/CoreUtils/"
ifeq ($(aes_gcm),1)
	$(quiet)rsync -avp $(SRCROOT)/External/GladmanAES/gcm.h "$(CROSSROOT)/usr/include/"
	$(quiet)rsync -avp $(SRCROOT)/External/GladmanAES/gf128mul.h "$(CROSSROOT)/usr/include/"
endif
ifeq ($(gladman_aes),1)
	$(quiet)rsync -avp $(SRCROOT)/External/GladmanAES/aes.h "$(CROSSROOT)/usr/include/"
	$(quiet)rsync -avp $(SRCROOT)/External/GladmanAES/brg_endian.h "$(CROSSROOT)/usr/include/"
	$(quiet)rsync -avp $(SRCROOT)/External/GladmanAES/brg_types.h "$(CROSSROOT)/usr/include/"
endif
	$(quiet)rsync -avp $(SRCROOT)/External/Small25519/Small25519.h "$(CROSSROOT)/usr/include/CoreUtils/"
	$(quiet)rsync -avp $(SRCROOT)/Support/*.h "$(CROSSROOT)/usr/include/CoreUtils/"
	$(quiet)cp "$(BUILDROOT)/libCoreUtils.so" "$(CROSSLIB)/"

.PHONY : makeroot
makeroot: $(TARGETS)
	$(quiet)rm -rf "$(DSTROOTDIR)"
	$(quiet)mkdir -p "$(DSTROOTDIR)/$(CROSS_RROOT)/usr/include/CoreUtils"
	$(quiet)rsync -avp $(SRCROOT)/External/Curve25519/curve25519-donna.h "$(DSTROOTDIR)/$(CROSS_RROOT)/usr/include/CoreUtils/"
	$(quiet)rsync -avp $(SRCROOT)/External/Ed25519/ed25519.h "$(DSTROOTDIR)/$(CROSS_RROOT)/usr/include/CoreUtils/"
ifeq ($(aes_gcm),1)
	$(quiet)rsync -avp $(SRCROOT)/External/GladmanAES/gcm.h "$(DSTROOTDIR)/$(CROSS_RROOT)/usr/include/"
	$(quiet)rsync -avp $(SRCROOT)/External/GladmanAES/gf128mul.h "$(DSTROOTDIR)/$(CROSS_RROOT)/usr/include/"
endif
ifeq ($(gladman_aes),1)
	$(quiet)rsync -avp $(SRCROOT)/External/GladmanAES/aes.h "$(DSTROOTDIR)/$(CROSS_RROOT)/usr/include/"
	$(quiet)rsync -avp $(SRCROOT)/External/GladmanAES/brg_endian.h "$(DSTROOTDIR)/$(CROSS_RROOT)/usr/include/"
	$(quiet)rsync -avp $(SRCROOT)/External/GladmanAES/brg_types.h "$(DSTROOTDIR)/$(CROSS_RROOT)/usr/include/"
endif
	$(quiet)rsync -avp $(SRCROOT)/External/Small25519/Small25519.h "$(DSTROOTDIR)/$(CROSS_RROOT)/usr/include/CoreUtils/"
	$(quiet)rsync -avp $(SRCROOT)/Support/*.h "$(DSTROOTDIR)/$(CROSS_RROOT)/usr/include/CoreUtils/"
	$(quiet)mkdir -p "$(DSTROOTDIR)/$(CROSS_RLIB)/"
	$(quiet)cp "$(BUILDROOT)/libCoreUtils.so" "$(DSTROOTDIR)/$(CROSS_RLIB)/"
	ditto -c -j "$(DSTROOTDIR)" "$(DSTROOTDIR).cpio.bz2"

# Library rules.

.PHONY : CoreUtils
CoreUtils: $(BUILDROOT)/libCoreUtils.so

.PHONY : CoreUtilsStatic
CoreUtilsStatic: $(BUILDROOT)/libCoreUtils.a

$(BUILDROOT)/libCoreUtils.so: $(CoreUtils_OBJS_shared)
	@echo -e  "Linking ($(os)-$(arch)-$(config)) $(ColorMagenta)$(notdir $@)$(ColorEnd)"
	$(quiet)$(CC) -shared -Wl,-soname,libCoreUtils.so -o $@ $(LINKFLAGS) $^ $(LIBS)
	@if [ "$(debug)" != "1" ]; then \
		echo "Stripping $(notdir $@)"; \
		$(STRIP) $@; \
	fi
	@echo -e  "$(ColorCyan)=== BUILD COMPLETE: $(notdir $@) ($(os)-$(arch)-$(config))$(ColorEnd)"

$(BUILDROOT)/libCoreUtils.a: $(CoreUtils_OBJS_direct)
	@echo -e  "Linking ($(os)-$(config)) $(ColorMagenta)$(notdir $@)$(ColorEnd)"
	$(quiet)$(AR) rcs $@ $^
	@echo -e  "$(ColorCyan)=== BUILD COMPLETE: $(notdir $@) ($(os)-$(config))$(ColorEnd)"

.PHONY : clean
clean:
	-rm -rf $(BUILDROOT)

$(aktest_OBJS):				| $(BUILDROOT)/obj
$(CoreUtils_OBJS_direct):	| $(BUILDROOT)/obj
$(CoreUtils_OBJS_shared):	| $(BUILDROOT)/obj

$(BUILDROOT)/obj:
	$(quiet)mkdir -p $@

# General rules
$(OBJDIR)/%.o: %.c
	@echo -e  "Compiling ($(os)-$(arch)-$(config)-direct) $(ColorMagenta)$(notdir $<)$(ColorEnd)"
	$(quiet)$(CC) -c $(CPPFLAGS) $(CFLAGS) $(CFLAGS_direct) -o $@ $<

$(OBJDIR)/%.o: %.cpp
	@echo -e  "Compiling ($(os)-$(arch)-$(config)-direct) $(ColorMagenta)$(notdir $<)$(ColorEnd)"
	$(quiet)$(CXX) -c $(CXXFLAGS) $(CFLAGS_direct) -o $@ $<

$(OBJDIR)/%.so.o: %.c
	@echo -e  "Compiling ($(os)-$(arch)-$(config)-shared) $(ColorMagenta)$(notdir $<)$(ColorEnd)"
	$(quiet)$(CC) -c -fPIC $(CPPFLAGS) $(CFLAGS) -o $@ $<

$(OBJDIR)/%.so.o: %.cpp
	@echo -e  "Compiling ($(os)-$(arch)-$(config)-shared) $(ColorMagenta)$(notdir $<)$(ColorEnd)"
	$(quiet)$(CXX) -c -fPIC $(CXXFLAGS) -o $@ $<

# Bonjour rules
.PHONY : bonjour
bonjour:
ifeq ($(os),qnx)
	make os=$(os) arch="$(qnx_arch)" -C $(MDNSROOT)/mDNSPosix setup Daemon libdns_sd Clients
else
ifeq ($(hw),generic)
	make os=$(os) -C $(MDNSROOT)/mDNSPosix setup Daemon libdns_sd Clients
else
	make CC="$(CC)" LD="$(LD)" STRIP="$(STRIP)" os=$(os) -C $(MDNSROOT)/mDNSPosix setup Daemon libdns_sd Clients
endif
endif

.PHONY : clean_bonjour
clean_bonjour:
	make os=$(os) -C $(MDNSROOT)/mDNSPosix clean
