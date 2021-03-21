# MPlayer Makefile
#
# copyright (c) 2008 Diego Biurrun
# Rewritten entirely from a set of Makefiles written by Arpi and many others.
#
# This file is part of MPlayer.
#
# MPlayer is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# MPlayer is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with MPlayer; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

include config.mak
###### variable declarations #######

# local fallbacks for missing operating system features
OS_FEATURE-$(GETTIMEOFDAY)           += osdep/gettimeofday.c
OS_FEATURE-$(GLOB_WIN)               += osdep/glob-win.c
OS_FEATURE-$(MMAP)                   += osdep/mmap-os2.c
OS_FEATURE-$(SETENV)                 += osdep/setenv.c
OS_FEATURE-$(SHMEM)                  += osdep/shmem.c
OS_FEATURE-$(STRSEP)                 += osdep/strsep.c
OS_FEATURE-$(VSSCANF)                += osdep/vsscanf.c

# conditional source declarations


SRCS_COMMON-$(FAAD)                  += libmpcodecs/ad_faad.c
SRCS_COMMON-$(FASTMEMCPY)            += libvo/aclib.c
SRCS_COMMON-$(FFMPEG)                += av_helpers.c                \
                                        av_opts.c                   \
                                        libmpcodecs/ad_ffmpeg.c     \
                                        stream/stream_ffmpeg.c      


SRCS_COMMON-$(CONFIG_VF_LAVFI)      +=  libmpcodecs/vf_lavfi.c

# These filters use private headers and do not work with shared FFmpeg.

SRCS_COMMON-$(HAVE_SYS_MMAN_H)       += libaf/af_export.c osdep/mmap_anon.c
SRCS_COMMON-$(LADSPA)                += libaf/af_ladspa.c


SRCS_COMMON-$(LIBBS2B)               += libaf/af_bs2b.c

SRCS_COMMON-$(LIBMAD)                += libmpcodecs/ad_libmad.c \
                                        libmpcodecs/ad_libbypass.c 

SRCS_COMMON-$(LIBSMBCLIENT)          += stream/stream_smb.c
SRCS_COMMON-$(LIVE555)               += libmpdemux/demux_rtp.cpp \
                                        libmpdemux/demux_rtsp.cpp \
                                        libmpdemux/demux_mp3rtsp.cpp \
                                        libmpdemux/demux_rtp_codec.cpp \
                                        stream/stream_live555.c
SRCS_COMMON-$(MPG123)                += libmpcodecs/ad_mpg123.c


SRCS_MP3LIB                          += mp3lib/sr1.c \
                                        $(SRCS_MP3LIB-yes)

SRCS_COMMON-$(MP3LIB)                += libmpcodecs/ad_mp3lib.c \
                                        $(SRCS_MP3LIB)

SRCS_LIBWMA                          += libwma/bitstream.c \
                                        libwma/fft.c \
                                        libwma/mdct.c \
                                        libwma/wmadeci.c \
                                        libwma/wmafixed.c 
SRCS_COMMON-$(LIBWMA)                += libmpcodecs/ad_libwma.c \
                                        $(SRCS_LIBWMA)

SRCS_LIBDRADEC                       += libmpdemux/demux_dra.c
SRCS_COMMON-$(LIBDRADEC)             += libmpcodecs/ad_libdradec.c \
                                        $(SRCS_LIBDRADEC)

SRCS_COMMON-$(MUSEPACK)              += libmpcodecs/ad_mpc.c \
                                        libmpdemux/demux_mpc.c
SRCS_COMMON-$(NATIVE_RTSP)           += stream/stream_rtsp.c \
                                        stream/freesdp/common.c \
                                        stream/freesdp/errorlist.c \
                                        stream/freesdp/parser.c \
                                        stream/librtsp/rtsp.c \
                                        stream/librtsp/rtsp_rtp.c \
                                        stream/librtsp/rtsp_session.c \

SRCS_COMMON-$(NETWORKING)            += stream/stream_netstream.c \
                                        stream/asf_mmst_streaming.c     \
                                        stream/asf_streaming.c          \
                                        stream/cookies.c \
                                        stream/http.c \
                                        stream/network.c \
                                        stream/pnm.c \
                                        stream/rtp.c \
                                        stream/udp.c \
                                        stream/tcp.c \
                                        stream/stream_rtp.c \
                                        stream/stream_udp.c \
                                        stream/librtsp/rtsp.c \
                                        stream/realrtsp/asmrp.c \
                                        stream/realrtsp/real.c \
                                        stream/realrtsp/rmff.c \
                                        stream/realrtsp/sdpplin.c \
                                        stream/realrtsp/xbuffer.c \


SRCS_COMMON-$(PRIORITY)              += osdep/priority.c
SRCS_COMMON-$(REAL_CODECS)           += libmpcodecs/ad_realaud.c 
SRCS_COMMON-$(STREAM_CACHE)          += stream/cache2.c

SRCS_COMMON-$(TREMOR_INTERNAL)       += tremor/bitwise.c \
                                        tremor/block.c \
                                        tremor/codebook.c \
                                        tremor/floor0.c \
                                        tremor/floor1.c \
                                        tremor/framing.c \
                                        tremor/info.c \
                                        tremor/mapping0.c \
                                        tremor/mdct.c \
                                        tremor/registry.c \
                                        tremor/res012.c \
                                        tremor/sharedbook.c \
                                        tremor/synthesis.c \
                                        tremor/window.c \


SRCS_COMMON-$(VORBIS)                += libmpcodecs/ad_libvorbis.c \
                                        libmpdemux/demux_ogg.c
SRCS_COMMON-$(VSTREAM)               += stream/stream_vstream.c





SRCS_COMMON = asxparser.c \
              bstr.c \
              codec-cfg.c \
              cpudetect.c \
              fmt-conversion.c \
              m_config.c \
              m_option.c \
              m_struct.c \
              mp_msg.c \
              mp_strings.c \
              mpcommon.c \
              parser-cfg.c \
              path.c \
              playtree.c \
              playtreeparser.c \
              subopt-helper.c \
              libaf/af.c \
              libaf/af_tools.c \
              libaf/af_channels.c \
              libaf/af_format.c \
              libaf/af_resample.c \
              libaf/af_lavcresample.c     \
              libaf/filter.c \
              libaf/format.c \
              libaf/af_dummy.c \
              libaf/reorder_ch.c \
              libaf/window.c \
              libmpcodecs/ad.c \
              libmpcodecs/ad_pcm.c \
              libmpcodecs/dec_audio.c \
              libmpcodecs/dec_video.c \
              libmpcodecs/pullup.c \
              libmpcodecs/vd.c \
              libmpcodecs/vd_null.c \
              libmpcodecs/vf.c \
              libmpdemux/demuxer.c \
              libmpdemux/demux_audio.c \
              libmpdemux/demux_demuxers.c \
              libmpdemux/aviprint.c \
              libmpdemux/extension.c \
              libmpdemux/mf.c \
              libmpdemux/mp3_hdr.c \
              libmpdemux/mp_taglists.c \
              libmpdemux/mpeg_hdr.c \
              libmpdemux/mpeg_packetizer.c \
              libmpdemux/parse_es.c \
              libmpdemux/parse_mp4.c \
              libmpdemux/video.c \
              libmpdemux/demux_mov.c \
              libmpdemux/demux_lavf.c \
              libmpdemux/aac_hdr.c \
              libmpdemux/demux_aac.c \
              libmpdemux/asfheader.c \
              libmpdemux/demux_asf.c \
              libmpcodecs/ad_pcm.c \
              osdep/$(GETCH) \
              osdep/$(TIMER) \
              stream/open.c \
              stream/stream.c \
              stream/stream_cue.c \
              stream/stream_file.c \
              stream/stream_mf.c \
              stream/stream_null.c \
              stream/url.c \
              $(SRCS_COMMON-yes) \
              $(OS_FEATURE-no)


SRCS_COMMON += libmpdemux/demux_ts.c

SRCS_MPLAYER-$(ALSA)         += libao2/ao_alsa.c 



SRCS_MPLAYER-$(NETWORKING)    += udp_sync.c


SRCS_MPLAYER = command.c \
               m_property.c \
               mixer.c \
               mp_fifo.c \
               mplayer.c \
               parser-mpcmd.c \
               input/input.c \
               libao2/ao_null.c \
               libao2/audio_out.c \
               libao2/ao_alsa.c \
	       




# (linking) order matters for these libraries
FFMPEGPARTS_ALL =  libavfilter libavformat libavcodec libavutil
FFMPEGPARTS = $(foreach part, $(FFMPEGPARTS_ALL), $(if $(wildcard ffmpeg/$(part)), $(part)))
FFMPEGLIBS  = $(foreach part, $(FFMPEGPARTS), ffmpeg/$(part)/$(part).a)
FFMPEGFILES = $(foreach part, $(FFMPEGPARTS), $(wildcard $(addprefix ffmpeg/$(part)/,*.[chS] /*/*.[chS] /*/*.asm)))


COMMON_LIBS-$(FFMPEG_A)           += $(FFMPEGLIBS)
COMMON_LIBS                       += $(COMMON_LIBS-yes)


OBJS_COMMON    += $(addsuffix .o, $(basename $(SRCS_COMMON)))
OBJS_MENCODER  += $(addsuffix .o, $(basename $(SRCS_MENCODER)))
OBJS_MPLAYER   += $(addsuffix .o, $(basename $(SRCS_MPLAYER)))
OBJS_MPLAYER-$(PE_EXECUTABLE) += osdep/mplayer-rc.o
OBJS_MPLAYER   += $(OBJS_MPLAYER-yes)

MENCODER_DEPS = $(OBJS_MENCODER) $(OBJS_COMMON) $(COMMON_LIBS)
MPLAYER_DEPS  = $(OBJS_MPLAYER)  $(OBJS_COMMON) $(COMMON_LIBS)
DEP_FILES = $(SRCS_COMMON) $(SRCS_MPLAYER) $(SRCS_MENCODER)
$(foreach suffix,.c .cpp .m .S,$(eval DEP_FILES := $(DEP_FILES:$(suffix)=.d)))

ALL_PRG-$(MPLAYER)  += mplayer$(EXESUF)
ALL_PRG-$(MENCODER) += mencoder$(EXESUF)

INSTALL_TARGETS-$(GUI)      += install-gui
INSTALL_TARGETS-$(MENCODER) += install-mencoder install-mencoder-man
INSTALL_TARGETS-$(MPLAYER)  += install-mplayer  install-mplayer-man

DIRS =  . \
        gui \
        gui/skin \
        gui/ui \
        gui/ui/gtk \
        gui/util \
        gui/win32 \
        gui/wm \
        input \
        libaf \
        libao2 \
        libmenu \
        libmpcodecs \
        libmpcodecs/native \
        libmpdemux \
        libvo \
        loader \
        loader/dmo \
        loader/dshow \
        loader/wine \
        mp3lib \
        osdep \
        stream \
        stream/freesdp \
        stream/librtsp \
        stream/realrtsp \
        sub \
        TOOLS \
        vidix \

ALL_DIRS = $(DIRS) \
        libass \
        libdvdcss \
        libdvdnav \
        libdvdnav/vm \
        libdvdread4 \
        libmpeg2 \
        tremor \

ALLHEADERS = $(foreach dir,$(DIRS),$(wildcard $(dir)/*.h))

ADDSUFFIXES     = $(foreach suf,$(1),$(addsuffix $(suf),$(2)))
ADD_ALL_DIRS    = $(call ADDSUFFIXES,$(1),$(ALL_DIRS))
ADD_ALL_EXESUFS = $(1) $(call ADDSUFFIXES,$(EXESUFS_ALL),$(1))

GUI_ICONSIZES = 16x16 22x22 24x24 32x32 48x48 256x256



###### generic rules #######

all: $(ALL_PRG-yes)

%.o: %.S
	$(CC) $(CC_DEPFLAGS) $(CFLAGS) -c -o $@ $<

%.o: %.c
	$(CC) $(CC_DEPFLAGS) $(CFLAGS) -c -o $@ $<

%.o: %.cpp
	$(CC) $(CC_DEPFLAGS) $(CXXFLAGS) -c -o $@ $<

%.o: %.m
	$(CC) $(CC_DEPFLAGS) $(CFLAGS) -c -o $@ $<

%-rc.o: %.rc
	$(WINDRES) -I. $< -o $@

$(FFMPEGLIBS): $(FFMPEGFILES) config.h
	$(MAKE) -C ffmpeg $(@:ffmpeg/%=%)

mencoder$(EXESUF): $(MENCODER_DEPS)
mencoder$(EXESUF): EXTRALIBS += $(EXTRALIBS_MENCODER)
mplayer$(EXESUF): $(MPLAYER_DEPS)
mplayer$(EXESUF): EXTRALIBS += $(EXTRALIBS_MPLAYER)
mencoder$(EXESUF) mplayer$(EXESUF):
	$(CC) -o $@ $^ $(EXTRALIBS)

codec-cfg$(EXESUF): codec-cfg.c codec-cfg.h help_mp.h
	$(HOST_CC) -O -DCODECS2HTML -I. -Iffmpeg -o $@ $<

codecs.conf.h: codec-cfg$(EXESUF) etc/codecs.conf
	./$^ > $@

checksums: $(MPLAYER_DEPS) $(MENCODER_DEPS) mplayer$(EXESUF) mencoder$(EXESUF)
	md5sum $^ > checksums

check_checksums: $(MPLAYER_DEPS) $(MENCODER_DEPS) mplayer$(EXESUF) mencoder$(EXESUF)
	md5sum -c checksums

# ./configure must be rerun if it changed
config.mak: configure
	@echo "############################################################"
	@echo "####### Please run ./configure again - it's changed! #######"
	@echo "############################################################"

checkhelp: help/help_mp*.h
	help/help_check.sh $(CC) $^

help_mp.h: help/help_mp-en.h $(HELP_FILE)
	help/help_create.sh $(HELP_FILE) $(CHARSET)

# rebuild version.h each time the working copy is updated
version.h: version.sh $(wildcard .svn/entries .git/logs/HEAD)
	./$< `$(CC) -dumpversion`

%$(EXESUF): %.c
	$(CC) $(CC_DEPFLAGS) $(CFLAGS) -o $@ $^

%.ho: %.h
	$(CC) $(CFLAGS) -Wno-unused -c -o $@ -x c $<

checkheaders: $(ALLHEADERS:.h=.ho)



###### XML documentation ######

doc: html-chunked html-single

html-chunked: $(addprefix html-chunked-,$(DOC_LANGS))
html-single:  $(addprefix html-single-,$(DOC_LANGS))

xmllint: $(addprefix xmllint-,$(DOC_LANGS))

define lang-def
html-chunked-$(lang): DOCS/HTML/$(lang)/dummy.html
html-single-$(lang):  DOCS/HTML/$(lang)/MPlayer.html
DOCS/HTML/$(lang)/dummy.html DOCS/HTML/$(lang)/MPlayer.html: DOCS/xml/$(lang)/main.xml $(wildcard DOCS/xml/$(lang)/*.xml) DOCS/xml/html-common.xsl DOCS/HTML/$(lang)/default.css

DOCS/HTML/$(lang)/default.css:
	mkdir -p $$(@D)
	cp -f DOCS/xml/default.css $$(@D)

DOCS/HTML/$(lang)/dummy.html:
	SGML_CATALOG_FILES=$(CATALOG) $(XSLT_COMMAND) $$@ DOCS/xml/html-chunk.xsl $$<

DOCS/HTML/$(lang)/MPlayer.html:
	SGML_CATALOG_FILES=$(CATALOG) $(XSLT_COMMAND) $$@ DOCS/xml/html-single.xsl $$<

xmllint-$(lang):
	SGML_CATALOG_FILES=$(CATALOG) $(XMLLINT_COMMAND) DOCS/xml/$(lang)/main.xml
endef

$(foreach lang, $(DOC_LANG_ALL),$(eval $(lang-def)))



###### dependency declarations / specific CFLAGS ######

# Make sure all generated header files are created.
codec-cfg.o: codecs.conf.h
$(DEP_FILES) $(MENCODER_DEPS) $(MPLAYER_DEPS): help_mp.h
mpcommon.o osdep/mplayer-rc.o gui/ui/gtk/about.o gui/win32/gui.o: version.h

osdep/mplayer-rc.o: osdep/mplayer.exe.manifest

gui/%: CFLAGS += -Wno-strict-prototypes

libdvdcss/%:   CFLAGS := -Ilibdvdcss -D_GNU_SOURCE -DVERSION=\"1.2.10\" $(CFLAGS_LIBDVDCSS) $(CFLAGS)
libdvdnav/%:   CFLAGS := -Ilibdvdnav -D_GNU_SOURCE -DHAVE_CONFIG_H -DVERSION=\"MPlayer-custom\" $(CFLAGS)
libdvdread4/%: CFLAGS := -Ilibdvdread4 -D_GNU_SOURCE $(CFLAGS_LIBDVDCSS_DVDREAD) $(CFLAGS)

loader/%: CFLAGS += -fno-omit-frame-pointer $(CFLAGS_NO_OMIT_LEAF_FRAME_POINTER)
#loader/%: CFLAGS += -Ddbg_printf=__vprintf -DTRACE=__vprintf -DDETAILED_OUT
loader/win32%: CFLAGS += $(CFLAGS_STACKREALIGN)

mp3lib/decode_i586%: CFLAGS += -fomit-frame-pointer

stream/stream_dvdnav%: CFLAGS := $(CFLAGS_LIBDVDNAV) $(CFLAGS)

tremor/%: CFLAGS += $(CFLAGS_TREMOR_LOW)

vidix/%: CFLAGS += $(CFLAGS_DHAHELPER) $(CFLAGS_SVGALIB_HELPER)

VIDIX_PCI_FILES = vidix/pci_dev_ids.c vidix/pci_ids.h vidix/pci_names.c \
                  vidix/pci_vendors.h

$(VIDIX_PCI_FILES): vidix/pci_db2c.awk vidix/pci.db
	awk -f $^ $(VIDIX_PCIDB)

VIDIX_OBJS = $(filter vidix/%,$(SRCS_MPLAYER:.c=.o))

$(VIDIX_OBJS): $(VIDIX_PCI_FILES)



###### installation / clean / generic rules #######

install: $(INSTALL_TARGETS-yes)

install-dirs:
	$(INSTALL) -d $(BINDIR) $(CONFDIR) $(LIBDIR)

install-%: %$(EXESUF) install-dirs
	$(INSTALL) -m 755 $(INSTALLSTRIP) $< $(BINDIR)

install-gui: install-mplayer install-gui-icons
	-ln -sf mplayer$(EXESUF) $(BINDIR)/gmplayer$(EXESUF)
	$(INSTALL) -d $(DATADIR)/skins $(prefix)/share/applications
	$(INSTALL) -m 644 etc/mplayer.desktop $(prefix)/share/applications/

install-gui-icons:    $(foreach size,$(GUI_ICONSIZES),install-gui-icon-$(size))
install-gui-man:      $(foreach lang,$(MAN_LANGS),install-gui-man-$(lang))
install-mencoder-man: $(foreach lang,$(MAN_LANGS),install-mencoder-man-$(lang))
install-mplayer-man:  $(foreach lang,$(MAN_LANGS),install-mplayer-man-$(lang))

install-gui-man-en: install-mplayer-man-en
	cd $(MANDIR)/man1/ && ln -sf mplayer.1 gmplayer.1

install-mencoder-man-en: install-mplayer-man-en
	cd $(MANDIR)/man1 && ln -sf mplayer.1 mencoder.1

install-mplayer-man-en:
	$(INSTALL) -d $(MANDIR)/man1
	$(INSTALL) -m 644 DOCS/man/en/mplayer.1 $(MANDIR)/man1/

define GUI_ICON_RULE
install-gui-icon-$(size):
	$(INSTALL) -d $(prefix)/share/icons/hicolor/$(size)/apps
	$(INSTALL) -m 644 etc/mplayer$(size).png $(prefix)/share/icons/hicolor/$(size)/apps/mplayer.png
endef

define GUI_MAN_RULE
install-gui-man-$(lang): install-mplayer-man-$(lang)
	cd $(MANDIR)/$(lang)/man1/ && ln -sf mplayer.1 gmplayer.1
endef

define MENCODER_MAN_RULE
install-mencoder-man-$(lang): install-mplayer-man-$(lang)
	cd $(MANDIR)/$(lang)/man1 && ln -sf mplayer.1 mencoder.1
endef

define MPLAYER_MAN_RULE
install-mplayer-man-$(lang):
	$(INSTALL) -d $(MANDIR)/$(lang)/man1
	$(INSTALL) -m 644 DOCS/man/$(lang)/mplayer.1 $(MANDIR)/$(lang)/man1/
endef

$(foreach size,$(GUI_ICONSIZES),$(eval $(GUI_ICON_RULE)))
$(foreach lang,$(filter-out en,$(MAN_LANG_ALL)),$(eval $(GUI_MAN_RULE)))
$(foreach lang,$(filter-out en,$(MAN_LANG_ALL)),$(eval $(MENCODER_MAN_RULE)))
$(foreach lang,$(filter-out en,$(MAN_LANG_ALL)),$(eval $(MPLAYER_MAN_RULE)))

uninstall:
	rm -f $(BINDIR)/mplayer$(EXESUF) $(BINDIR)/gmplayer$(EXESUF)
	rm -f $(BINDIR)/mencoder$(EXESUF)
	rm -f $(MANDIR)/man1/mencoder.1 $(MANDIR)/man1/mplayer.1
	rm -f $(foreach size,$(GUI_ICONSIZES),$(prefix)/share/icons/hicolor/$(size)/apps/mplayer.png)
	rm -f $(prefix)/share/applications/mplayer.desktop
	rm -f $(MANDIR)/man1/mplayer.1 $(MANDIR)/man1/mencoder.1
	rm -f $(foreach lang,$(MAN_LANGS),$(foreach man,mplayer.1 mencoder.1,$(MANDIR)/$(lang)/man1/$(man)))

clean:
#	-$(MAKE) -C ffmpeg $@
	-rm -rf tests/res
	-rm -f $(call ADD_ALL_DIRS,/*.o /*.a /*.ho /*~)
	-rm -f $(call ADD_ALL_EXESUFS,mplayer mencoder)
	-rm -rf DOCS/tech/doxygen DOCS/HTML

distclean: clean testsclean toolsclean driversclean dhahelperclean
	-$(MAKE) -C ffmpeg $@
	-rm -f DOCS/xml/html-chunk.xsl DOCS/xml/html-single.xsl
	-rm -f $(call ADD_ALL_DIRS,/*.d)
	-rm -f config.* codecs.conf.h help_mp.h version.h TAGS tags
	-rm -f $(VIDIX_PCI_FILES)
	-rm -f $(call ADD_ALL_EXESUFS,codec-cfg cpuinfo)

doxygen:
	doxygen DOCS/tech/Doxyfile

TAGS:
	rm -f $@; find . -name '*.[chS]' -o -name '*.asm' | xargs etags -a

tags:
	rm -f $@; find . -name '*.[chS]' -o -name '*.asm' | xargs ctags -a



###### regression tests #######

BROKEN_SAMPLES =                         \
    h264-conformance/FM1_BT_B.h264       \
    h264-conformance/FM1_FT_E.264        \
    h264-conformance/FM2_SVA_B.264       \
    pva/PVA_test-partial.pva             \
    wmv8/wmv_drm.wmv                     \
    wtv/law-and-order-partial.wtv        \

AUDIO_ONLY_SAMPLES =                                               \
    aac/% ac3/% amrnb/% amrwb/% atrac1/% atrac3/% bink/binkaudio%  \
    creative/% dts/% duck/%-audio-only.avi eac3/% gsm/% imc/%      \
    lossless-audio/% mp3-conformance/% musepack/% nellymoser/%     \
    qcp/%                                                          \
    qt-surge-suite/% real/ra% sipr/% truespeech/% vorbis/%         \
    vqf/% w64/% wmapro/% wmavoice/%                                \

# running wildcard with empty FATE_SAMPLES seems to cause a lot of issues
ifdef FATE_SAMPLES
ALLSAMPLES_FULLPATH = $(wildcard $(FATE_SAMPLES)/*/*.*)
ALLSAMPLES          = $(patsubst $(FATE_SAMPLES)/%,%,$(ALLSAMPLES_FULLPATH))
SAMPLES := $(filter-out $(BROKEN_SAMPLES),$(ALLSAMPLES))
SAMPLES := $(filter-out $(AUDIO_ONLY_SAMPLES),$(SAMPLES))
RESULTS  = $(patsubst %,tests/res/%.md5,$(SAMPLES))

fatetest: $(RESULTS)

tests/res/%.md5: mplayer$(EXESUF) $(FATE_SAMPLES)/%
	@tests/faterun.sh $*
else
fatetest:
	@echo "You need to set FATE_SAMPLES for fatetest to work"
endif



###### tests / tools #######

TEST_OBJS = mp_msg.o mp_fifo.o osdep/$(GETCH) osdep/$(TIMER) -ltermcap -lm

codec-cfg-test$(EXESUF): codec-cfg.c codecs.conf.h help_mp.h $(TEST_OBJS)
	$(CC) -I. -Iffmpeg -DTESTING -o $@ $^

codecs2html$(EXESUF): codec-cfg.c help_mp.h
	$(CC) -I. -Iffmpeg -DCODECS2HTML -o $@ $^

libvo/aspecttest$(EXESUF): libvo/aspect.o libvo/geometry.o $(TEST_OBJS)

LOADER_TEST_OBJS = $(SRCS_WIN32_EMULATION:.c=.o) $(SRCS_QTX_EMULATION:.S=.o) ffmpeg/libavutil/libavutil.a osdep/mmap_anon.o cpudetect.o path.o $(TEST_OBJS)

loader/qtx/list$(EXESUF) loader/qtx/qtxload$(EXESUF): CFLAGS += -g
loader/qtx/list$(EXESUF) loader/qtx/qtxload$(EXESUF): $(LOADER_TEST_OBJS)

mp3lib/test$(EXESUF) mp3lib/test2$(EXESUF): $(SRCS_MP3LIB:.c=.o) libvo/aclib.o cpudetect.o $(TEST_OBJS)

TESTS = codecs2html codec-cfg-test libvo/aspecttest mp3lib/test mp3lib/test2

ifdef ARCH_X86_32
TESTS += loader/qtx/list loader/qtx/qtxload
endif

TESTS_DEP_FILES = $(addsuffix .d,$(TESTS))

tests: $(addsuffix $(EXESUF),$(TESTS))

testsclean:
	-rm -f $(call ADD_ALL_EXESUFS,$(TESTS))

TOOLS = $(addprefix TOOLS/,alaw-gen asfinfo avi-fix avisubdump compare dump_mp4 movinfo netstream subrip vivodump)

ifdef ARCH_X86
TOOLS += TOOLS/fastmemcpybench TOOLS/modify_reg
endif

ALLTOOLS = $(TOOLS) TOOLS/bmovl-test TOOLS/vfw2menc

TOOLS_DEP_FILES = $(addsuffix .d,$(ALLTOOLS))

tools: $(addsuffix $(EXESUF),$(TOOLS))
alltools: $(addsuffix $(EXESUF),$(ALLTOOLS))

toolsclean:
	-rm -f $(call ADD_ALL_EXESUFS,$(ALLTOOLS))
	-rm -f TOOLS/realcodecs/*.so.6.0

TOOLS/bmovl-test$(EXESUF): -lSDL_image

TOOLS/subrip$(EXESUF): path.o sub/vobsub.o sub/spudec.o sub/unrar_exec.o \
    ffmpeg/libswscale/libswscale.a ffmpeg/libavutil/libavutil.a $(TEST_OBJS)

TOOLS/vfw2menc$(EXESUF): -lwinmm -lole32

mplayer-nomain.o: mplayer.c
	$(CC) $(CFLAGS) -DDISABLE_MAIN -c -o $@ $<

TOOLS/netstream$(EXESUF): TOOLS/netstream.c
TOOLS/vivodump$(EXESUF): TOOLS/vivodump.c
TOOLS/netstream$(EXESUF) TOOLS/vivodump$(EXESUF): $(subst mplayer.o,mplayer-nomain.o,$(OBJS_MPLAYER)) $(filter-out %mencoder.o,$(OBJS_MENCODER)) $(OBJS_COMMON) $(COMMON_LIBS)
	$(CC) $(CC_DEPFLAGS) $(CFLAGS) -o $@ $^ $(EXTRALIBS_MPLAYER) $(EXTRALIBS_MENCODER) $(EXTRALIBS)

REAL_SRCS    = $(wildcard TOOLS/realcodecs/*.c)
REAL_TARGETS = $(REAL_SRCS:.c=.so.6.0)

realcodecs: $(REAL_TARGETS)
realcodecs: CFLAGS += -g

%.so.6.0: %.o
	ld -shared -o $@ $< -ldl -lc



###### drivers #######

KERNEL_INC = /lib/modules/`uname -r`/build/include
KERNEL_VERSION = $(shell grep RELEASE $(KERNEL_INC)/linux/version.h | cut -d'"' -f2)
KERNEL_CFLAGS = -O2 -D__KERNEL__ -DMODULE -Wall -I$(KERNEL_INC) -include $(KERNEL_INC)/linux/modversions.h
KERNEL_OBJS = $(addprefix drivers/, mga_vid.o tdfx_vid.o radeon_vid.o rage128_vid.o)
MODULES_DIR = /lib/modules/$(KERNEL_VERSION)/misc
DRIVER_OBJS = $(KERNEL_OBJS) drivers/mga_vid_test drivers/tdfx_vid_test
DRIVER_DEP_FILES = $(KERNEL_OBJS:.o=.d) drivers/mga_vid_test.d drivers/tdfx_vid_test.d

drivers: $(DRIVER_OBJS)

$(DRIVER_OBJS): CFLAGS = $(KERNEL_CFLAGS)
drivers/radeon_vid.o drivers/rage128_vid.o: CFLAGS += -fomit-frame-pointer -fno-strict-aliasing -fno-common -ffast-math

install-drivers: $(DRIVER_OBJS)
	-mkdir -p $(MODULES_DIR)
	$(INSTALL) -m 644 $(KERNEL_OBJS) $(MODULES_DIR)
	depmod -a
	-mknod /dev/mga_vid    c 178 0
	-mknod /dev/tdfx_vid   c 178 0
	-mknod /dev/radeon_vid c 178 0
	-ln -s /dev/radeon_vid /dev/rage128_vid

driversclean:
	-rm -f $(DRIVER_OBJS) drivers/*~

DHAHELPER_DEP_FILES = vidix/dhahelper/dhahelper.d vidix/dhahelper/test.d vidix/dhahelperwin/dhahelper.d vidix/dhahelperwin/dhasetup.d
dhahelper: vidix/dhahelper/dhahelper.o vidix/dhahelper/test

vidix/dhahelper/dhahelper.o vidix/dhahelper/test: CFLAGS = $(KERNEL_CFLAGS)

install-dhahelper: vidix/dhahelper/dhahelper.o
	-mkdir -p $(MODULES_DIR)
	$(INSTALL) -m 644 $< $(MODULES_DIR)
	depmod -a
	-mknod /dev/dhahelper c 180 0

dhahelperwin: vidix/dhahelperwin/dhasetup.exe vidix/dhahelperwin/dhahelper.sys

vidix/dhahelperwin/dhahelper-rc.o: vidix/dhahelperwin/common.ver vidix/dhahelperwin/ntverp.h

vidix/dhahelperwin/base.tmp: vidix/dhahelperwin/dhahelper.o vidix/dhahelperwin/dhahelper-rc.o
	$(CC) -Wl,--base-file,$@ -Wl,--entry,_DriverEntry@8 -nostartfiles \
            -nostdlib -o $(@D)/junk.tmp $^ -lntoskrnl
	-rm -f $(@D)/junk.tmp

vidix/dhahelperwin/temp.exp: vidix/dhahelperwin/base.tmp
	dlltool --dllname $(@D)/dhahelper.sys --base-file $< --output-exp $@

vidix/dhahelperwin/dhahelper.sys: vidix/dhahelperwin/temp.exp vidix/dhahelperwin/dhahelper.o vidix/dhahelperwin/dhahelper-rc.o
	$(CC) -Wl,--subsystem,native -Wl,--image-base,0x10000 \
            -Wl,--file-alignment,0x1000 -Wl,--section-alignment,0x1000 \
            -Wl,--entry,_DriverEntry@8 -Wl,$< -mdll -nostartfiles -nostdlib \
            -o $@ $(@:.sys=.o) $(@:.sys=-rc.o) -lntoskrnl
	strip $@

install-dhahelperwin:
	vidix/dhahelperwin/dhasetup.exe install

dhahelperclean:
	-rm -f vidix/dhahelper/*.o vidix/dhahelper/*~ vidix/dhahelper/test
	-rm -f $(addprefix vidix/dhahelperwin/,*.o *~ dhahelper.sys dhasetup.exe base.tmp temp.exp)


-include $(DEP_FILES) $(DRIVER_DEP_FILES) $(TESTS_DEP_FILES) $(TOOLS_DEP_FILES) $(DHAHELPER_DEP_FILES)

.PHONY: all doxygen *install* *tools drivers dhahelper*
.PHONY: checkheaders *clean tests check_checksums fatetest checkhelp
.PHONY: doc html-chunked* html-single* xmllint*

# Disable suffix rules.  Most of the builtin rules are suffix rules,
# so this saves some time on slow systems.
.SUFFIXES:
