CUR_DIR		:=$(shell pwd)
TOP_DIR		:=$(BUILD_TOP_DIR)

include $(TOP_DIR)/Makefile.Inc
include $(TOP_DIR)/Rule.make

DESTLIB:= ./mplayer
RELEASEDESTLIB:=$(addprefix $(RELEASE_DIR)/lib/, $(notdir $(DESTLIB)))

export CC
export NM
export STRIP
export AR
export RANLIB
export PKG_CONFIG_PATH=$(RELEASE_DIR)/lib/pkgconfig

ifeq ($(OPENWRT_AVS), YES)
CFLAGS := $(subst -Werror=implicit-function-declaration, ,$(CFLAGS))
endif

CFLAGS := $(subst -Werror, ,$(CFLAGS))

#check if debug version
ifneq (,$(findstring debug, $(MAKECMDGOALS)))
DEBUG_OPT := --enable-crash-debug --enable-debug=2
else
DEBUG_OPT :=
endif

EXTRA_LD_FLAGS := 
ifeq ($(EXPIRE_CHECK_MODULE), YES)
	EXTRA_LD_FLAGS += -lexpire -lcrypto
endif
ifeq ($(WMRMPLAY_ENABLE), YES)
	EXTRA_LD_FLAGS += -lwmrm -lshine -lmad -lmp3lame -lssl -ljson-c -lcurl -lz -lnghttp2
endif

ifeq ($(BUILD_PLATFORM), INGENIC)
	PLATFORM_LIBS 	:= -lrt -lnvram -lwmdb
else ifeq ($(BUILD_PLATFORM), AMLOGIC)
	PLATFORM_LIBS 	:=  -lnvram -lwmdb -lwmfadecache -lasound -ljson-c
else ifeq ($(BUILD_PLATFORM), QUALCOMM)
	PLATFORM_LIBS 	:=  -lnvram -lwmdb -lwmfadecache -lasound -ljson-c
else ifeq ($(BUILD_PLATFORM), ALLWINNER)
	PLATFORM_LIBS 	:=  -lnvram -lwmdb -lasound -ljson-c
else
	PLATFORM_LIBS 	:= -lnvram
endif
EXTRA_LD_FLAGS += $(PLATFORM_LIBS)

#check if disable new feature
ifneq (,$(findstring NON_NEW_FEATURE, $(CFLAGS)))
DIS_NEW_FEATURE :=		--disable-decoder=ALAC_DECODER \
				  		--disable-pthreads      \
				  		--disable-faad              \
						--disable-decoder=APE_DECODER \
						--disable-demuxer=APE_DEMUXER\
						--disable-decoder=ESCAPE124_DECODER \
						--disable-decoder=ESCAPE130_DECODER \
						--disable-decoder=FLAC_DECODER \
						--disable-demuxer=FLAC_DEMUXER\
						--disable-muxer=FLAC_MUXER\
						--disable-parser=FLAC_PARSER
else
ifeq ($(ASR_NAVER_MODULE), YES)
DIS_NEW_FEATURE :=		--disable-faad              \
				  		--disable-pthreads
else
DIS_NEW_FEATURE :=		--enable-faad              \
				  		--disable-pthreads
endif						
endif

#check if enable dra feature
ifneq (,$(findstring DRA_FORMAT_MODULE_ENABLE, $(PCGLAGS)))
PROJECT_FEATURE :=		--enable-libDraDec
else
PROJECT_FEATURE :=		--disable-libDraDec
endif

#check if build for ARM
ifeq ($(TARGETENV),ARM)
DEF_PKG_CONFIG_LIBDIR	:=

ifeq ($(BUILD_PLATFORM), QUALCOMM)
CONFIGOPTION		    := --host-cc=gcc --target=arm-linux --cc=arm-openwrt-linux-gcc \
                           --disable-mencoder
else ifeq ($(BUILD_PLATFORM), ALLWINNER)
#CONFIGOPTION		    := --host-cc=gcc --target=arm-linux --cc=aarch64-openwrt-linux-musl-gcc \
#                           --disable-mencoder
CONFIGOPTION		    := --host-cc=gcc --target=aarch64-linux --cc=aarch64-openwrt-linux-musl-gcc \
                           --disable-mencoder
else
CONFIGOPTION		    := --host-cc=gcc --target=arm-linux --cc=arm-linux-gnueabihf-gcc \
                           --disable-mencoder --disable-neon --enable-pthreads  --enable-libopencore_amrnb  --enable-demuxer=AMR_DEMUXER
endif
SPECCFLAGS                  :=-fPIC

else
ifeq ($(TARGETENV),MIPS)
CONFIGOPTION		    := --host-cc=gcc  --target=mips-linux --cc=mipsel-linux-gcc --disable-mencoder --enable-libopencore_amrnb  --enable-demuxer=AMR_DEMUXER
SPECCFLAGS                  :=

else
DEF_PKG_CONFIG_LIBDIR	:=
CONFIGOPTION		    := --yasm=''  --disable-mmx --disable-mmxext --disable-3dnow --disable-3dnowext --enable-libopencore_amrnb  --enable-demuxer=AMR_DEMUXER
SPECCFLAGS                  :=-DX86 -g -ggdb

endif
endif

ifeq ($(ASR_NAVER_MODULE), YES)
SPECCFLAGS += -pie -fstack-protector -Wa,--noexecstack
EXTRA_LD_FLAGS += -pie -fstack-protector -Wl,-z,noexecstack
endif

ifeq ($(ASR_AMAZON_MODULE), YES)
#CONFIGOPTION += --enable-hardcoded-tables
endif

all release debug:config.mak
	make


ifeq ($(BUILD_PLATFORM), ALLWINNER)
  CONFIGURE=./configure_aarch64
else
  CONFIGURE=./configure
endif

$(CONFIGURE):
	rm -rf autom4te.cache
	aclocal
	libtoolize --force --copy
	autoheader
	autoconf
	automake -a

ifeq ($(EXPIRE_CHECK_MODULE), YES)
config.mak: clean
else
config.mak:
endif
	$(DEF_PKG_CONFIG_LIBDIR) $(CONFIGURE) $(CONFIGOPTION) --prefix=$(RELEASE_DIR) \
		--extra-cflags="-I$(RELEASE_DIR)/include/live555 $(CFLAGS) $(SPECCFLAGS)" \
		--extra-ldflags="$(LDFLAGS) -lm -lliveMedia -lgroupsock -lBasicUsageEnvironment -lUsageEnvironment \
		-lstdc++ -lssl -lcrypto -lpthread -lmvmsg -lwl_util $(EXTRA_LD_FLAGS)" \
        \
        $(DIS_NEW_FEATURE) \
        $(PROJECT_FEATURE) \
        --enable-minimize-size   \
        --enable-live \
  		--disable-mencoder      \
  		--disable-gui            \
  		--disable-gtk1           \
  		--disable-termcap        \
  		--disable-termios        \
  		--disable-iconv         \
  		--disable-langinfo      \
  		--disable-lirc           \
  		--disable-lircc          \
  		--disable-joystick       \
  		--disable-apple-remote   \
  		--disable-apple-ir       \
  		--disable-vm            \
  		--disable-xf86keysym    \
  		--disable-radio          \
  		--disable-radio-capture  \
  		--disable-radio-v4l2    \
  		--disable-radio-bsdbt848    \
  		--disable-tv            \
  		--disable-tv-v4l1       \
  		--disable-tv-v4l2       \
  		--disable-tv-bsdbt848   \
  		--disable-pvr           \
  		--disable-rtc           \
  		--disable-winsock2_h     \
  		--disable-smb            \
  		--disable-nemesi         \
  		--disable-librtmp        \
  		--disable-vcd           \
  		--disable-bluray        \
  		--disable-dvdnav        \
  		--disable-dvdread       \
  		--disable-dvdread-internal   \
  		--disable-libdvdcss-internal  \
  		--disable-cdparanoia    \
  		--disable-cddb          \
  		--disable-freetype      \
  		--disable-fontconfig    \
  		--disable-unrarexec     \
  		--disable-menu           \
  		--disable-sortsub       \
  		--disable-fribidi        \
  		--disable-enca          \
  		--disable-maemo         \
  		--disable-macosx-finder  \
  		--disable-macosx-bundle  \
  		--disable-inet6         \
  		--disable-sctp          \
  		--disable-gethostbyname2   \
  		--disable-ftp           \
  		--disable-vstream       \
  		--disable-w32threads    \
  		--disable-os2threads    \
  		--disable-ass-internal   \
  		--disable-ass           \
  		--disable-gif               \
  		--disable-png               \
  		--disable-mng               \
  		--disable-jpeg              \
  		--disable-libcdio           \
  		--disable-liblzo            \
  		--disable-win32dll         \
  		--disable-qtx              \
  		--disable-xanim            \
  		--disable-real             \
  		--disable-xvid             \
  		--disable-xvid-lavc        \
  		--disable-x264             \
  		--disable-x264-lavc        \
  		--disable-libdirac-lavc    \
  		--disable-libschroedinger-lavc    \
  		--disable-libvpx-lavc      \
  		--disable-libnut           \
  		--disable-ffmpeg_so        \
  		--disable-postproc         \
  		--disable-vf-lavfi          \
  		--disable-libavcodec_mpegaudio_hp  \
  		--enable-tremor-internal  \
  		--disable-tremor-low        \
  		--disable-tremor            \
  		--disable-libvorbis        \
  		--disable-speex            \
  		--disable-libgsm           \
  		--disable-theora            \
  		--disable-faac             \
  		--disable-faac-lavc        \
  		--disable-ladspa           \
  		--disable-libbs2b          \
  		--disable-libdv            \
  		--disable-mpg123           \
  		--disable-mp3lame          \
  		--disable-mp3lame-lavc     \
  		--disable-toolame          \
  		--disable-twolame          \
  		--disable-xmms              \
  		--disable-libdca            \
  		--disable-mp3lib           \
  		--disable-liba52           \
  		--disable-libmpeg2         \
  		--disable-libmpeg2-internal  \
  		--disable-musepack          \
  		--disable-libopencore_amrwb  \
  		--disable-libopenjpeg      \
  		--disable-crystalhd        \
  		--disable-vidix           \
  		--disable-vidix-pcidb     \
  		--disable-dhahelper        \
  		--disable-svgalib_helper   \
  		--disable-gl               \
  		--disable-matrixview      \
  		--disable-dga2             \
  		--disable-dga1             \
  		--disable-vesa             \
  		--disable-svga             \
  		--disable-sdl              \
  		--disable-kva              \
  		--disable-aa               \
  		--disable-caca             \
  		--disable-ggi              \
  		--disable-ggiwmh           \
  		--disable-direct3d         \
  		--disable-directx          \
  		--disable-dxr2             \
  		--disable-dxr3             \
  		--disable-ivtv             \
  		--disable-v4l2             \
  		--disable-dvb              \
  		--disable-mga              \
  		--disable-xmga             \
  		--disable-xv               \
  		--disable-xvmc             \
  		--disable-vdpau            \
  		--disable-vm               \
  		--disable-xinerama         \
  		--disable-x11              \
  		--disable-xshape           \
  		--disable-xss             \
  		--disable-fbdev            \
  		--disable-mlib             \
  		--disable-3dfx             \
  		--disable-tdfxfb           \
  		--disable-s3fb             \
  		--disable-wii              \
  		--disable-directfb         \
  		--disable-zr               \
  		--disable-bl               \
  		--disable-tdfxvid          \
  		--disable-xvr100           \
  		--disable-tga             \
  		--disable-pnm             \
  		--disable-md5sum          \
  		--disable-yuv4mpeg        \
  		--disable-corevideo     \
  		--disable-quartz        \
  		--disable-ossaudio      \
  		--disable-arts          \
  		--disable-esd           \
  		--disable-pulse         \
  		--disable-jack          \
  		--disable-openal        \
  		--disable-nas           \
  		--disable-sgiaudio      \
  		--disable-sunaudio      \
  		--disable-kai           \
  		--disable-dart          \
  		--disable-win32waveout  \
  		--disable-coreaudio     \
  		--disable-select        \
  		\
		--disable-decoder=AASC_DECODER \
		--disable-decoder=AMV_DECODER \
		--disable-decoder=ANM_DECODER \
		--disable-decoder=ANSI_DECODER \
		--disable-decoder=ASV1_DECODER \
		--disable-decoder=ASV2_DECODER \
		--disable-decoder=AURA_DECODER \
		--disable-decoder=AURA2_DECODER \
		--disable-decoder=AVRP_DECODER \
		--disable-decoder=AVS_DECODER \
		--disable-decoder=AVUI_DECODER \
		--disable-decoder=AYUV_DECODER \
		--disable-decoder=BETHSOFTVID_DECODER \
		--disable-decoder=BFI_DECODER \
		--disable-decoder=BINK_DECODER \
		--disable-decoder=BMP_DECODER \
		--disable-decoder=BMV_VIDEO_DECODER \
		--disable-decoder=C93_DECODER \
		--disable-decoder=CAVS_DECODER \
		--disable-decoder=CDGRAPHICS_DECODER \
		--disable-decoder=CDXL_DECODER \
		--disable-decoder=CINEPAK_DECODER \
		--disable-decoder=CLJR_DECODER \
		--disable-decoder=CSCD_DECODER \
		--disable-decoder=CYUV_DECODER \
		--disable-decoder=DFA_DECODER \
		--disable-decoder=DIRAC_DECODER \
		--disable-decoder=DNXHD_DECODER \
		--disable-decoder=DPX_DECODER \
		--disable-decoder=DSICINVIDEO_DECODER \
		--disable-decoder=DVVIDEO_DECODER \
		--disable-decoder=DXA_DECODER \
		--disable-decoder=DXTORY_DECODER \
		--disable-decoder=EACMV_DECODER \
		--disable-decoder=EAMAD_DECODER \
		--disable-decoder=EATGQ_DECODER \
		--disable-decoder=EATGV_DECODER \
		--disable-decoder=EATQI_DECODER \
		--disable-decoder=EIGHTBPS_DECODER \
		--disable-decoder=EIGHTSVX_EXP_DECODER \
		--disable-decoder=EIGHTSVX_FIB_DECODER \
		--disable-decoder=EXR_DECODER \
		--disable-decoder=FFV1_DECODER \
		--disable-decoder=FFVHUFF_DECODER \
		--disable-decoder=FLASHSV_DECODER \
		--disable-decoder=FLASHSV2_DECODER \
		--disable-decoder=FLIC_DECODER \
		--disable-decoder=FLV_DECODER \
		--disable-decoder=FOURXM_DECODER \
		--disable-decoder=FRAPS_DECODER \
		--disable-decoder=FRWU_DECODER \
		--disable-decoder=GIF_DECODER \
		--disable-decoder=H261_DECODER \
		--disable-decoder=H263_DECODER \
		--disable-decoder=H263I_DECODER \
		--disable-decoder=H264_DECODER \
		--disable-decoder=HUFFYUV_DECODER \
		--disable-decoder=IDCIN_DECODER \
		--disable-decoder=IFF_BYTERUN1_DECODER \
		--disable-decoder=IFF_ILBM_DECODER \
		--disable-decoder=INDEO2_DECODER \
		--disable-decoder=INDEO3_DECODER \
		--disable-decoder=INDEO4_DECODER \
		--disable-decoder=INDEO5_DECODER \
		--disable-decoder=INTERPLAY_VIDEO_DECODER \
		--disable-decoder=JPEG2000_DECODER \
		--disable-decoder=JPEGLS_DECODER \
		--disable-decoder=JV_DECODER \
		--disable-decoder=KGV1_DECODER \
		--disable-decoder=KMVC_DECODER \
		--disable-decoder=LAGARITH_DECODER \
		--disable-decoder=LOCO_DECODER \
		--disable-decoder=MDEC_DECODER \
		--disable-decoder=MIMIC_DECODER \
		--disable-decoder=MJPEG_DECODER \
		--disable-decoder=MJPEGB_DECODER \
		--disable-decoder=MMVIDEO_DECODER \
		--disable-decoder=MOTIONPIXELS_DECODER \
		--disable-decoder=MPEG1VIDEO_DECODER \
		--disable-decoder=MPEG2VIDEO_DECODER \
		--disable-decoder=MPEG4_DECODER \
		--disable-decoder=MPEGVIDEO_DECODER \
		--disable-decoder=MSMPEG4V1_DECODER \
		--disable-decoder=MSMPEG4V2_DECODER \
		--disable-decoder=MSMPEG4V3_DECODER \
		--disable-decoder=MSRLE_DECODER \
		--disable-decoder=MSVIDEO1_DECODER \
		--disable-decoder=MSZH_DECODER \
		--disable-decoder=MXPEG_DECODER \
		--disable-decoder=NUV_DECODER \
		--disable-decoder=PAM_DECODER \
		--disable-decoder=PBM_DECODER \
		--disable-decoder=PCX_DECODER \
		--disable-decoder=PGM_DECODER \
		--disable-decoder=PGMYUV_DECODER \
		--disable-decoder=PICTOR_DECODER \
		--disable-decoder=PNG_DECODER \
		--disable-decoder=PPM_DECODER \
		--disable-decoder=PRORES_DECODER \
		--disable-decoder=PRORES_LGPL_DECODER \
		--disable-decoder=PTX_DECODER \
		--disable-decoder=QDRAW_DECODER \
		--disable-decoder=QPEG_DECODER \
		--disable-decoder=QTRLE_DECODER \
		--disable-decoder=R10K_DECODER \
		--disable-decoder=R210_DECODER \
		--disable-decoder=RAWVIDEO_DECODER \
		--disable-decoder=RL2_DECODER \
		--disable-decoder=ROQ_DECODER \
		--disable-decoder=RPZA_DECODER \
		--disable-decoder=RV10_DECODER \
		--disable-decoder=RV20_DECODER \
		--disable-decoder=RV30_DECODER \
		--disable-decoder=RV40_DECODER \
		--disable-decoder=S302M_DECODER \
		--disable-decoder=SGI_DECODER \
		--disable-decoder=SMACKER_DECODER \
		--disable-decoder=SMC_DECODER \
		--disable-decoder=SNOW_DECODER \
		--disable-decoder=SP5X_DECODER \
		--disable-decoder=SUNRAST_DECODER \
		--disable-decoder=SVQ1_DECODER \
		--disable-decoder=SVQ3_DECODER \
		--disable-decoder=TARGA_DECODER \
		--disable-decoder=THEORA_DECODER \
		--disable-decoder=THP_DECODER \
		--disable-decoder=TIERTEXSEQVIDEO_DECODER \
		--disable-decoder=TIFF_DECODER \
		--disable-decoder=TMV_DECODER \
		--disable-decoder=TRUEMOTION1_DECODER \
		--disable-decoder=TRUEMOTION2_DECODER \
		--disable-decoder=TSCC_DECODER \
		--disable-decoder=TXD_DECODER \
		--disable-decoder=ULTI_DECODER \
		--disable-decoder=UTVIDEO_DECODER \
		--disable-decoder=V210_DECODER \
		--disable-decoder=V210X_DECODER \
		--disable-decoder=V308_DECODER \
		--disable-decoder=V408_DECODER \
		--disable-decoder=V410_DECODER \
		--disable-decoder=VB_DECODER \
		--disable-decoder=VBLE_DECODER \
		--disable-decoder=VC1_DECODER \
		--disable-decoder=VC1IMAGE_DECODER \
		--disable-decoder=VCR1_DECODER \
		--disable-decoder=VMDVIDEO_DECODER \
		--disable-decoder=VMNC_DECODER \
		--disable-decoder=VP3_DECODER \
		--disable-decoder=VP5_DECODER \
		--disable-decoder=VP6_DECODER \
		--disable-decoder=VP6A_DECODER \
		--disable-decoder=VP6F_DECODER \
		--disable-decoder=VP8_DECODER \
		--disable-decoder=VQA_DECODER \
		--disable-decoder=WMV1_DECODER \
		--disable-decoder=WMV2_DECODER \
		--disable-decoder=WMV3_DECODER \
		--disable-decoder=WMV3IMAGE_DECODER \
		--disable-decoder=WNV1_DECODER \
		--disable-decoder=XAN_WC3_DECODER \
		--disable-decoder=XAN_WC4_DECODER \
		--disable-decoder=XBM_DECODER \
		--disable-decoder=XL_DECODER \
		--disable-decoder=XWD_DECODER \
		--disable-decoder=Y41P_DECODER \
		--disable-decoder=YOP_DECODER \
		--disable-decoder=YUV4_DECODER \
		--disable-decoder=ZEROCODEC_DECODER \
		--disable-decoder=ZLIB_DECODER \
		--disable-decoder=ZMBV_DECODER \
		--disable-decoder=AC3_DECODER \
		--disable-decoder=ALS_DECODER \
		--disable-decoder=AMRNB_DECODER \
		--disable-decoder=AMRWB_DECODER \
		--disable-decoder=ATRAC1_DECODER \
		--disable-decoder=ATRAC3_DECODER \
		--disable-decoder=BINKAUDIO_DCT_DECODER \
		--disable-decoder=BINKAUDIO_RDFT_DECODER \
		--disable-decoder=BMV_AUDIO_DECODER \
		--disable-decoder=COOK_DECODER \
		--disable-decoder=DCA_DECODER \
		--disable-decoder=DSICINAUDIO_DECODER \
		--disable-decoder=EAC3_DECODER \
		--disable-decoder=FFWAVESYNTH_DECODER \
		--disable-decoder=G723_1_DECODER \
		--disable-decoder=G729_DECODER \
		--disable-decoder=GSM_DECODER \
		--disable-decoder=GSM_MS_DECODER \
		--disable-decoder=IMC_DECODER \
		--disable-decoder=MACE3_DECODER \
		--disable-decoder=MACE6_DECODER \
		--disable-decoder=MLP_DECODER \
		--disable-decoder=MP1_DECODER \
		--disable-decoder=MP1FLOAT_DECODER \
		--disable-decoder=MP2_DECODER \
		--disable-decoder=MP2FLOAT_DECODER \
		--disable-decoder=MP3FLOAT_DECODER \
		--disable-decoder=MP3ADU_DECODER \
		--disable-decoder=MP3ADUFLOAT_DECODER \
		--disable-decoder=MP3ON4_DECODER \
		--disable-decoder=MP3ON4FLOAT_DECODER \
		--disable-decoder=MPC7_DECODER \
		--disable-decoder=MPC8_DECODER \
		--disable-decoder=NELLYMOSER_DECODER \
		--disable-decoder=QCELP_DECODER \
		--disable-decoder=QDM2_DECODER \
		--disable-decoder=RA_144_DECODER \
		--disable-decoder=RA_288_DECODER \
		--disable-decoder=RALF_DECODER \
		--disable-decoder=SHORTEN_DECODER \
		--disable-decoder=SIPR_DECODER \
		--disable-decoder=SMACKAUD_DECODER \
		--disable-decoder=SONIC_DECODER \
		--disable-decoder=TRUEHD_DECODER \
		--disable-decoder=TRUESPEECH_DECODER \
		--disable-decoder=TTA_DECODER \
		--disable-decoder=TWINVQ_DECODER \
		--disable-decoder=VMDAUDIO_DECODER \
		--disable-decoder=VORBIS_DECODER \
		--disable-decoder=WAVPACK_DECODER \
		--disable-decoder=WMAPRO_DECODER \
		--disable-decoder=WMALOSSLESS_DECODER \
		--disable-decoder=WMAVOICE_DECODER \
		--disable-decoder=WS_SND1_DECODER \
		--disable-decoder=INTERPLAY_DPCM_DECODER \
		--disable-decoder=ROQ_DPCM_DECODER \
		--disable-decoder=SOL_DPCM_DECODER \
		--disable-decoder=XAN_DPCM_DECODER \
		--disable-decoder=ADPCM_4XM_DECODER \
		--disable-decoder=ADPCM_ADX_DECODER \
		--disable-decoder=ADPCM_CT_DECODER \
		--disable-decoder=ADPCM_EA_DECODER \
		--disable-decoder=ADPCM_EA_MAXIS_XA_DECODER \
		--disable-decoder=ADPCM_EA_R1_DECODER \
		--disable-decoder=ADPCM_EA_R2_DECODER \
		--disable-decoder=ADPCM_EA_R3_DECODER \
		--disable-decoder=ADPCM_EA_XAS_DECODER \
		--disable-decoder=ADPCM_G722_DECODER \
		--disable-decoder=ADPCM_G726_DECODER \
		--disable-decoder=ADPCM_IMA_AMV_DECODER \
		--disable-decoder=ADPCM_IMA_APC_DECODER \
		--disable-decoder=ADPCM_IMA_DK3_DECODER \
		--disable-decoder=ADPCM_IMA_DK4_DECODER \
		--disable-decoder=ADPCM_IMA_EA_EACS_DECODER \
		--disable-decoder=ADPCM_IMA_EA_SEAD_DECODER \
		--disable-decoder=ADPCM_IMA_ISS_DECODER \
		--disable-decoder=ADPCM_IMA_QT_DECODER \
		--disable-decoder=ADPCM_IMA_SMJPEG_DECODER \
		--disable-decoder=ADPCM_IMA_WAV_DECODER \
		--disable-decoder=ADPCM_IMA_WS_DECODER \
		--disable-decoder=ADPCM_MS_DECODER \
		--disable-decoder=ADPCM_SBPRO_2_DECODER \
		--disable-decoder=ADPCM_SBPRO_3_DECODER \
		--disable-decoder=ADPCM_SBPRO_4_DECODER \
		--disable-decoder=ADPCM_SWF_DECODER \
		--disable-decoder=ADPCM_THP_DECODER \
		--disable-decoder=ADPCM_XA_DECODER \
		--disable-decoder=ADPCM_YAMAHA_DECODER \
		--disable-decoder=ASS_DECODER \
		--disable-decoder=DVBSUB_DECODER \
		--disable-decoder=DVDSUB_DECODER \
		--disable-decoder=JACOSUB_DECODER \
		--disable-decoder=MICRODVD_DECODER \
		--disable-decoder=PGSSUB_DECODER \
		--disable-decoder=SRT_DECODER \
		--disable-decoder=XSUB_DECODER \
		--disable-decoder=BINTEXT_DECODER \
		--disable-decoder=XBIN_DECODER \
		--disable-decoder=IDF_DECODER \
		\
		--disable-encoder=A64MULTI_ENCODER \
		--disable-encoder=A64MULTI5_ENCODER \
		--disable-encoder=AMV_ENCODER \
		--disable-encoder=ASV1_ENCODER \
		--disable-encoder=ASV2_ENCODER \
		--disable-encoder=AVRP_ENCODER \
		--disable-encoder=AVUI_ENCODER \
		--disable-encoder=AYUV_ENCODER \
		--disable-encoder=BMP_ENCODER \
		--disable-encoder=CLJR_ENCODER \
		--disable-encoder=DNXHD_ENCODER \
		--disable-encoder=DPX_ENCODER \
		--disable-encoder=DVVIDEO_ENCODER \
		--disable-encoder=FFV1_ENCODER \
		--disable-encoder=FFVHUFF_ENCODER \
		--disable-encoder=FLASHSV_ENCODER \
		--disable-encoder=FLASHSV2_ENCODER \
		--disable-encoder=FLV_ENCODER \
		--disable-encoder=GIF_ENCODER \
		--disable-encoder=H261_ENCODER \
		--disable-encoder=H263_ENCODER \
		--disable-encoder=H263P_ENCODER \
		--disable-encoder=HUFFYUV_ENCODER \
		--disable-encoder=JPEG2000_ENCODER \
		--disable-encoder=JPEGLS_ENCODER \
		--disable-encoder=LJPEG_ENCODER \
		--disable-encoder=MJPEG_ENCODER \
		--disable-encoder=MPEG1VIDEO_ENCODER \
		--disable-encoder=MPEG2VIDEO_ENCODER \
		--disable-encoder=MPEG4_ENCODER \
		--disable-encoder=MSMPEG4V2_ENCODER \
		--disable-encoder=MSMPEG4V3_ENCODER \
		--disable-encoder=MSVIDEO1_ENCODER \
		--disable-encoder=PAM_ENCODER \
		--disable-encoder=PBM_ENCODER \
		--disable-encoder=PCX_ENCODER \
		--disable-encoder=PGM_ENCODER \
		--disable-encoder=PGMYUV_ENCODER \
		--disable-encoder=PNG_ENCODER \
		--disable-encoder=PPM_ENCODER \
		--disable-encoder=PRORES_ENCODER \
		--disable-encoder=PRORES_ANATOLIY_ENCODER \
		--disable-encoder=PRORES_KOSTYA_ENCODER \
		--disable-encoder=QTRLE_ENCODER \
		--disable-encoder=R10K_ENCODER \
		--disable-encoder=R210_ENCODER \
		--disable-encoder=RAWVIDEO_ENCODER \
		--disable-encoder=ROQ_ENCODER \
		--disable-encoder=RV10_ENCODER \
		--disable-encoder=RV20_ENCODER \
		--disable-encoder=SGI_ENCODER \
		--disable-encoder=SNOW_ENCODER \
		--disable-encoder=SUNRAST_ENCODER \
		--disable-encoder=SVQ1_ENCODER \
		--disable-encoder=TARGA_ENCODER \
		--disable-encoder=TIFF_ENCODER \
		--disable-encoder=V210_ENCODER \
		--disable-encoder=V308_ENCODER \
		--disable-encoder=V408_ENCODER \
		--disable-encoder=V410_ENCODER \
		--disable-encoder=WMV1_ENCODER \
		--disable-encoder=WMV2_ENCODER \
		--disable-encoder=XBM_ENCODER \
		--disable-encoder=XWD_ENCODER \
		--disable-encoder=Y41P_ENCODER \
		--disable-encoder=YUV4_ENCODER \
		--disable-encoder=ZLIB_ENCODER \
		--disable-encoder=ZMBV_ENCODER \
		--disable-encoder=AAC_ENCODER \
		--disable-encoder=AC3_ENCODER \
		--disable-encoder=AC3_FIXED_ENCODER \
		--disable-encoder=ALAC_ENCODER \
		--disable-encoder=DCA_ENCODER \
		--disable-encoder=EAC3_ENCODER \
		--disable-encoder=FLAC_ENCODER \
		--disable-encoder=G723_1_ENCODER \
		--disable-encoder=MP2_ENCODER \
		--disable-encoder=NELLYMOSER_ENCODER \
		--disable-encoder=RA_144_ENCODER \
		--disable-encoder=SONIC_ENCODER \
		--disable-encoder=SONIC_LS_ENCODER \
		--disable-encoder=VORBIS_ENCODER \
		--disable-encoder=WMAV1_ENCODER \
		--disable-encoder=WMAV2_ENCODER \
		--disable-encoder=PCM_ALAW_ENCODER \
		--disable-encoder=PCM_F32BE_ENCODER \
		--disable-encoder=PCM_F32LE_ENCODER \
		--disable-encoder=PCM_F64BE_ENCODER \
		--disable-encoder=PCM_F64LE_ENCODER \
		--disable-encoder=PCM_MULAW_ENCODER \
		--disable-encoder=PCM_S8_ENCODER \
		--disable-encoder=PCM_S16BE_ENCODER \
		--disable-encoder=PCM_S16LE_ENCODER \
		--disable-encoder=PCM_S24BE_ENCODER \
		--disable-encoder=PCM_S24DAUD_ENCODER \
		--disable-encoder=PCM_S24LE_ENCODER \
		--disable-encoder=PCM_S32BE_ENCODER \
		--disable-encoder=PCM_S32LE_ENCODER \
		--disable-encoder=PCM_U8_ENCODER \
		--disable-encoder=PCM_U16BE_ENCODER \
		--disable-encoder=PCM_U16LE_ENCODER \
		--disable-encoder=PCM_U24BE_ENCODER \
		--disable-encoder=PCM_U24LE_ENCODER \
		--disable-encoder=PCM_U32BE_ENCODER \
		--disable-encoder=PCM_U32LE_ENCODER \
		--disable-encoder=ROQ_DPCM_ENCODER \
		--disable-encoder=ADPCM_ADX_ENCODER \
		--disable-encoder=ADPCM_G722_ENCODER \
		--disable-encoder=ADPCM_G726_ENCODER \
		--disable-encoder=ADPCM_IMA_QT_ENCODER \
		--disable-encoder=ADPCM_IMA_WAV_ENCODER \
		--disable-encoder=ADPCM_MS_ENCODER \
		--disable-encoder=ADPCM_SWF_ENCODER \
		--disable-encoder=ADPCM_YAMAHA_ENCODER \
		--disable-encoder=ASS_ENCODER \
		--disable-encoder=DVBSUB_ENCODER \
		--disable-encoder=DVDSUB_ENCODER \
		--disable-encoder=SRT_ENCODER \
		--disable-encoder=XSUB_ENCODER \
		\
		--disable-demuxer=AC3_DEMUXER\
		--disable-demuxer=ACT_DEMUXER\
		--disable-demuxer=ADF_DEMUXER\
		--disable-demuxer=ADX_DEMUXER\
		--disable-demuxer=AEA_DEMUXER\
		--disable-demuxer=ANM_DEMUXER\
		--disable-demuxer=APC_DEMUXER\
		--disable-demuxer=ASS_DEMUXER\
		--disable-demuxer=AU_DEMUXER\
		--disable-demuxer=AVI_DEMUXER\
		--disable-demuxer=AVS_DEMUXER\
		--disable-demuxer=BETHSOFTVID_DEMUXER\
		--disable-demuxer=BFI_DEMUXER\
		--disable-demuxer=BINTEXT_DEMUXER\
		--disable-demuxer=BINK_DEMUXER\
		--disable-demuxer=BIT_DEMUXER\
		--disable-demuxer=BMV_DEMUXER\
		--disable-demuxer=C93_DEMUXER\
		--disable-demuxer=CAF_DEMUXER\
		--disable-demuxer=CAVSVIDEO_DEMUXER\
		--disable-demuxer=CDG_DEMUXER\
		--disable-demuxer=CDXL_DEMUXER\
		--disable-demuxer=DAUD_DEMUXER\
		--disable-demuxer=DFA_DEMUXER\
		--disable-demuxer=DIRAC_DEMUXER\
		--disable-demuxer=DNXHD_DEMUXER\
		--disable-demuxer=DSICIN_DEMUXER\
		--disable-demuxer=DTS_DEMUXER\
		--disable-demuxer=DV_DEMUXER\
		--disable-demuxer=DXA_DEMUXER\
		--disable-demuxer=EA_DEMUXER\
		--disable-demuxer=EA_CDATA_DEMUXER\
		--disable-demuxer=EAC3_DEMUXER\
		--disable-demuxer=FFM_DEMUXER\
		--disable-demuxer=FFMETADATA_DEMUXER\
		--disable-demuxer=FILMSTRIP_DEMUXER\
		--disable-demuxer=FLIC_DEMUXER\
		--disable-demuxer=FLV_DEMUXER\
		--disable-demuxer=FOURXM_DEMUXER\
		--disable-demuxer=G722_DEMUXER\
		--disable-demuxer=G723_1_DEMUXER\
		--disable-demuxer=G729_DEMUXER\
		--disable-demuxer=GSM_DEMUXER\
		--disable-demuxer=GXF_DEMUXER\
		--disable-demuxer=H261_DEMUXER\
		--disable-demuxer=H263_DEMUXER\
		--disable-demuxer=H264_DEMUXER\
		--disable-demuxer=ICO_DEMUXER\
		--disable-demuxer=IDCIN_DEMUXER\
		--disable-demuxer=IDF_DEMUXER\
		--disable-demuxer=IFF_DEMUXER\
                --enable-demuxer=AIFF_DEMUXER\
		--disable-demuxer=IMAGE2_DEMUXER\
		--disable-demuxer=IMAGE2PIPE_DEMUXER\
		--disable-demuxer=INGENIENT_DEMUXER\
		--disable-demuxer=IPMOVIE_DEMUXER\
		--disable-demuxer=ISS_DEMUXER\
		--disable-demuxer=IV8_DEMUXER\
		--disable-demuxer=IVF_DEMUXER\
		--disable-demuxer=JACOSUB_DEMUXER\
		--disable-demuxer=JV_DEMUXER\
		--disable-demuxer=LATM_DEMUXER\
		--disable-demuxer=LMLM4_DEMUXER\
		--disable-demuxer=LOAS_DEMUXER\
		--disable-demuxer=LXF_DEMUXER\
		--disable-demuxer=M4V_DEMUXER\
		--disable-demuxer=MATROSKA_DEMUXER\
		--disable-demuxer=MICRODVD_DEMUXER\
		--disable-demuxer=MJPEG_DEMUXER\
		--disable-demuxer=MLP_DEMUXER\
		--disable-demuxer=MM_DEMUXER\
		--disable-demuxer=MMF_DEMUXER\
		--disable-demuxer=MPC_DEMUXER\
		--disable-demuxer=MPC8_DEMUXER\
		--disable-demuxer=MPEGPS_DEMUXER\
		--disable-demuxer=MPEGTSRAW_DEMUXER\
		--disable-demuxer=MPEGVIDEO_DEMUXER\
		--disable-demuxer=MSNWC_TCP_DEMUXER\
		--disable-demuxer=MTV_DEMUXER\
		--disable-demuxer=MVI_DEMUXER\
		--disable-demuxer=MXF_DEMUXER\
		--disable-demuxer=MXG_DEMUXER\
		--disable-demuxer=NC_DEMUXER\
		--disable-demuxer=NSV_DEMUXER\
		--disable-demuxer=NUT_DEMUXER\
		--disable-demuxer=NUV_DEMUXER\
		--disable-demuxer=OGG_DEMUXER\
		--disable-demuxer=OMA_DEMUXER\
		--disable-demuxer=PMP_DEMUXER\
		--disable-demuxer=PVA_DEMUXER\
		--disable-demuxer=QCP_DEMUXER\
		--disable-demuxer=R3D_DEMUXER\
		--disable-demuxer=RAWVIDEO_DEMUXER\
		--disable-demuxer=RL2_DEMUXER\
		--disable-demuxer=ROQ_DEMUXER\
		--disable-demuxer=RPL_DEMUXER\
		--disable-demuxer=RSO_DEMUXER\
		--disable-demuxer=SAP_DEMUXER\
		--disable-demuxer=SBG_DEMUXER\
		--disable-demuxer=SEGAFILM_DEMUXER\
		--disable-demuxer=SHORTEN_DEMUXER\
		--disable-demuxer=SIFF_DEMUXER\
		--disable-demuxer=SMACKER_DEMUXER\
		--disable-demuxer=SMJPEG_DEMUXER\
		--disable-demuxer=SOL_DEMUXER\
		--disable-demuxer=SOX_DEMUXER\
		--disable-demuxer=SPDIF_DEMUXER\
		--disable-demuxer=SRT_DEMUXER\
		--disable-demuxer=STR_DEMUXER\
		--disable-demuxer=SWF_DEMUXER\
		--disable-demuxer=THP_DEMUXER\
		--disable-demuxer=TIERTEXSEQ_DEMUXER\
		--disable-demuxer=TMV_DEMUXER\
		--disable-demuxer=TRUEHD_DEMUXER\
		--disable-demuxer=TTA_DEMUXER\
		--disable-demuxer=TXD_DEMUXER\
		--disable-demuxer=TTY_DEMUXER\
		--disable-demuxer=VC1_DEMUXER\
		--disable-demuxer=VC1T_DEMUXER\
		--disable-demuxer=VMD_DEMUXER\
		--disable-demuxer=VOC_DEMUXER\
		--disable-demuxer=VQF_DEMUXER\
		--disable-demuxer=W64_DEMUXER\
		--disable-demuxer=WC3_DEMUXER\
		--disable-demuxer=WSAUD_DEMUXER\
		--disable-demuxer=WSVQA_DEMUXER\
		--disable-demuxer=WTV_DEMUXER\
		--disable-demuxer=WV_DEMUXER\
		--disable-demuxer=XA_DEMUXER\
		--disable-demuxer=XBIN_DEMUXER\
		--disable-demuxer=XMV_DEMUXER\
		--disable-demuxer=XWMA_DEMUXER\
		--disable-demuxer=YOP_DEMUXER\
		--disable-demuxer=YUV4MPEGPIPE_DEMUXER	\
		\
		--disable-muxer=A64_MUXER\
		--disable-muxer=AC3_MUXER\
		--disable-muxer=ADTS_MUXER\
		--disable-muxer=ADX_MUXER\
		--disable-muxer=AMR_MUXER\
		--disable-muxer=ASF_MUXER\
		--disable-muxer=ASS_MUXER\
		--disable-muxer=ASF_STREAM_MUXER\
		--disable-muxer=AU_MUXER\
		--disable-muxer=AVI_MUXER\
		--disable-muxer=AVM2_MUXER\
		--disable-muxer=BIT_MUXER\
		--disable-muxer=CAF_MUXER\
		--disable-muxer=CAVSVIDEO_MUXER\
		--disable-muxer=CRC_MUXER\
		--disable-muxer=DAUD_MUXER\
		--disable-muxer=DIRAC_MUXER\
		--disable-muxer=DNXHD_MUXER\
		--disable-muxer=DTS_MUXER\
		--disable-muxer=DV_MUXER\
		--disable-muxer=EAC3_MUXER\
		--disable-muxer=FFM_MUXER\
		--disable-muxer=FFMETADATA_MUXER\
		--disable-muxer=FILMSTRIP_MUXER\
		--disable-muxer=FLV_MUXER\
		--disable-muxer=FRAMECRC_MUXER\
		--disable-muxer=FRAMEMD5_MUXER\
		--disable-muxer=G722_MUXER\
		--disable-muxer=G723_1_MUXER\
		--disable-muxer=GIF_MUXER\
		--disable-muxer=GXF_MUXER\
		--disable-muxer=H261_MUXER\
		--disable-muxer=H263_MUXER\
		--disable-muxer=H264_MUXER\
		--disable-muxer=IMAGE2_MUXER\
		--disable-muxer=IMAGE2PIPE_MUXER\
		--disable-muxer=IPOD_MUXER\
		--disable-muxer=ISMV_MUXER\
		--disable-muxer=IVF_MUXER\
		--disable-muxer=JACOSUB_MUXER\
		--disable-muxer=LATM_MUXER\
		--disable-muxer=M4V_MUXER\
		--disable-muxer=MD5_MUXER\
		--disable-muxer=MATROSKA_MUXER\
		--disable-muxer=MATROSKA_AUDIO_MUXER\
		--disable-muxer=MICRODVD_MUXER\
		--disable-muxer=MJPEG_MUXER\
		--disable-muxer=MLP_MUXER\
		--disable-muxer=MMF_MUXER\
		--disable-muxer=MOV_MUXER\
		--disable-muxer=MP2_MUXER\
		--disable-muxer=MP3_MUXER\
		--disable-muxer=MP4_MUXER\
		--disable-muxer=MPEG1SYSTEM_MUXER\
		--disable-muxer=MPEG1VCD_MUXER\
		--disable-muxer=MPEG1VIDEO_MUXER\
		--disable-muxer=MPEG2DVD_MUXER\
		--disable-muxer=MPEG2SVCD_MUXER\
		--disable-muxer=MPEG2VIDEO_MUXER\
		--disable-muxer=MPEG2VOB_MUXER\
		--disable-muxer=MPEGTS_MUXER\
		--disable-muxer=MPJPEG_MUXER\
		--disable-muxer=MXF_MUXER\
		--disable-muxer=MXF_D10_MUXER\
		--disable-muxer=NULL_MUXER\
		--disable-muxer=NUT_MUXER\
		--disable-muxer=OGG_MUXER\
		--disable-muxer=OMA_MUXER\
		--disable-muxer=PCM_MULAW_MUXER\
		--disable-muxer=PCM_F64BE_MUXER\
		--disable-muxer=PCM_F64LE_MUXER\
		--disable-muxer=PCM_F32BE_MUXER\
		--disable-muxer=PCM_F32LE_MUXER\
		--disable-muxer=PCM_S32BE_MUXER\
		--disable-muxer=PCM_S32LE_MUXER\
		--disable-muxer=PCM_S24BE_MUXER\
		--disable-muxer=PCM_S24LE_MUXER\
		--disable-muxer=PCM_S16BE_MUXER\
		--disable-muxer=PCM_S16LE_MUXER\
		--disable-muxer=PCM_S8_MUXER\
		--disable-muxer=PCM_U32BE_MUXER\
		--disable-muxer=PCM_U32LE_MUXER\
		--disable-muxer=PCM_U24BE_MUXER\
		--disable-muxer=PCM_U24LE_MUXER\
		--disable-muxer=PCM_U16BE_MUXER\
		--disable-muxer=PCM_U16LE_MUXER\
		--disable-muxer=PCM_U8_MUXER\
		--disable-muxer=PSP_MUXER\
		--disable-muxer=RAWVIDEO_MUXER\
		--disable-muxer=RM_MUXER\
		--disable-muxer=ROQ_MUXER\
		--disable-muxer=RSO_MUXER\
		--disable-muxer=SEGMENT_MUXER\
		--disable-muxer=SMJPEG_MUXER\
		--disable-muxer=SOX_MUXER\
		--disable-muxer=SPDIF_MUXER\
		--disable-muxer=SRT_MUXER\
		--disable-muxer=SWF_MUXER\
		--disable-muxer=TG2_MUXER\
		--disable-muxer=TGP_MUXER\
		--disable-muxer=MKVTIMESTAMP_V2_MUXER\
		--disable-muxer=TRUEHD_MUXER\
		--disable-muxer=VC1T_MUXER\
		--disable-muxer=VOC_MUXER\
		--disable-muxer=WAV_MUXER\
		--disable-muxer=WEBM_MUXER\
		--disable-muxer=WTV_MUXER\
		--disable-muxer=YUV4MPEGPIPE_MUXER \
		\
		--disable-protocol=CACHE_PROTOCOL \
		--disable-protocol=CONCAT_PROTOCOL  \
		--disable-protocol=FILE_PROTOCOL  \
		--disable-protocol=GOPHER_PROTOCOL  \
		--disable-protocol=HTTPPROXY_PROTOCOL  \
		--disable-protocol=MMSH_PROTOCOL  \
		--disable-protocol=MMST_PROTOCOL  \
		--disable-protocol=MD5_PROTOCOL  \
		--disable-protocol=PIPE_PROTOCOL  \
		--disable-protocol=RTMP_PROTOCOL  \
		--disable-protocol=SCTP_PROTOCOL  \
		--disable-protocol=LIBRTMP_PROTOCOL  \
		--disable-protocol=LIBRTMPE_PROTOCOL  \
		--disable-protocol=LIBRTMPS_PROTOCOL  \
		--disable-protocol=LIBRTMPT_PROTOCOL  \
		--disable-protocol=LIBRTMPTE_PROTOCOL \
		\
		\
		--disable-filter=ACONVERT_FILTER \
		--disable-filter=AFORMAT_FILTER \
		--disable-filter=AMERGE_FILTER \
		--disable-filter=AMIX_FILTER \
		--disable-filter=ANULL_FILTER \
		--disable-filter=ARESAMPLE_FILTER \
		--disable-filter=ASHOWINFO_FILTER \
		--disable-filter=ASPLIT_FILTER \
		--disable-filter=ASTREAMSYNC_FILTER \
		--disable-filter=ASYNCTS_FILTER \
		--disable-filter=EARWAX_FILTER \
		--disable-filter=PAN_FILTER \
		--disable-filter=SILENCEDETECT_FILTER \
		--disable-filter=VOLUME_FILTER \
		--disable-filter=RESAMPLE_FILTER \
		--disable-filter=AEVALSRC_FILTER \
		--disable-filter=AMOVIE_FILTER \
		--disable-filter=ANULLSRC_FILTER \
		--disable-filter=ABUFFERSINK_FILTER \
		--disable-filter=ANULLSINK_FILTER \
		--disable-filter=ASS_FILTER \
		--disable-filter=BBOX_FILTER \
		--disable-filter=BLACKDETECT_FILTER \
		--disable-filter=BLACKFRAME_FILTER \
		--disable-filter=BOXBLUR_FILTER \
		--disable-filter=COLORMATRIX_FILTER \
		--disable-filter=COPY_FILTER \
		--disable-filter=CROP_FILTER \
		--disable-filter=CROPDETECT_FILTER \
		--disable-filter=DELOGO_FILTER \
		--disable-filter=DESHAKE_FILTER \
		--disable-filter=DRAWBOX_FILTER \
		--disable-filter=DRAWTEXT_FILTER \
		--disable-filter=FADE_FILTER \
		--disable-filter=FIELDORDER_FILTER \
		--disable-filter=FIFO_FILTER \
		--disable-filter=FORMAT_FILTER \
		--disable-filter=FPS_FILTER \
		--disable-filter=GRADFUN_FILTER \
		--disable-filter=HFLIP_FILTER \
		--disable-filter=HQDN3D_FILTER \
		--disable-filter=IDET_FILTER \
		--disable-filter=LUT_FILTER \
		--disable-filter=LUTRGB_FILTER \
		--disable-filter=LUTYUV_FILTER \
		--disable-filter=NEGATE_FILTER \
		--disable-filter=NOFORMAT_FILTER \
		--disable-filter=NULL_FILTER \
		--disable-filter=OVERLAY_FILTER \
		--disable-filter=PAD_FILTER \
		--disable-filter=PIXDESCTEST_FILTER \
		--disable-filter=REMOVELOGO_FILTER \
		--disable-filter=SELECT_FILTER \
		--disable-filter=SETDAR_FILTER \
		--disable-filter=SETFIELD_FILTER \
		--disable-filter=SETPTS_FILTER \
		--disable-filter=SETSAR_FILTER \
		--disable-filter=SETTB_FILTER \
		--disable-filter=SHOWINFO_FILTER \
		--disable-filter=SLICIFY_FILTER \
		--disable-filter=SPLIT_FILTER \
		--disable-filter=SUPER2XSAI_FILTER \
		--disable-filter=SWAPUV_FILTER \
		--disable-filter=THUMBNAIL_FILTER \
		--disable-filter=TILE_FILTER \
		--disable-filter=TINTERLACE_FILTER \
		--disable-filter=TRANSPOSE_FILTER \
		--disable-filter=UNSHARP_FILTER \
		--disable-filter=VFLIP_FILTER \
		--disable-filter=YADIF_FILTER \
		--disable-filter=CELLAUTO_FILTER \
		--disable-filter=COLOR_FILTER \
		--disable-filter=LIFE_FILTER \
		--disable-filter=MANDELBROT_FILTER \
		--disable-filter=MOVIE_FILTER \
		--disable-filter=MPTESTSRC_FILTER \
		--disable-filter=NULLSRC_FILTER \
		--disable-filter=RGBTESTSRC_FILTER \
		--disable-filter=TESTSRC_FILTER \
		--disable-filter=BUFFERSINK_FILTER \
		--disable-filter=NULLSINK_FILTER \
		\
		--disable-parser=AC3_PARSER \
		--disable-parser=ADX_PARSER \
		--disable-parser=CAVSVIDEO_PARSER \
		--disable-parser=COOK_PARSER \
		--disable-parser=DCA_PARSER \
		--disable-parser=DIRAC_PARSER \
		--disable-parser=DNXHD_PARSER \
		--disable-parser=DVBSUB_PARSER \
		--disable-parser=DVDSUB_PARSER \
		--disable-parser=GSM_PARSER \
		--disable-parser=H261_PARSER \
		--disable-parser=H263_PARSER \
		--disable-parser=H264_PARSER \
		--disable-parser=MJPEG_PARSER \
		--disable-parser=MLP_PARSER \
		--disable-parser=MPEG4VIDEO_PARSER \
		--disable-parser=MPEGAUDIO_PARSER \
		--disable-parser=MPEGVIDEO_PARSER \
		--disable-parser=PNG_PARSER \
		--disable-parser=PNM_PARSER \
		--disable-parser=RV30_PARSER \
		--disable-parser=RV40_PARSER \
		--disable-parser=VC1_PARSER \
		--disable-parser=VORBIS_PARSER \
		--disable-parser=VP3_PARSER \
		--disable-parser=VP8_PARSER


install:$(RELEASEDESTLIB)


$(RELEASEDESTLIB):$(DESTLIB)
	echo $(STRIP) $(CC)
	make install

.PHONY: clean
clean:
	-make clean


.PHONY: distclean
distclean:
	-make clean
	-make distclean
	rm -f config.mak
