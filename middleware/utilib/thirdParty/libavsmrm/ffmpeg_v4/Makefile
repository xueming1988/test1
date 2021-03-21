include ../common.mk

## build

PACKAGE := $(PKG_CONFIG_DIR)/libavcodec.pc
MAKEFILE := out/$(TARGET_PRODUCT)/Makefile
make_dir := $(dir $(MAKEFILE))

all: $(PACKAGE)

$(PACKAGE): $(MAKEFILE)
	$(MAKE) -C $(make_dir)
	$(MAKE) -C $(make_dir) install

$(MAKEFILE): | $(make_dir)

$(make_dir):
	mkdir -p $(make_dir)

# TODO: remove protocols once we have in-memory playback implemented
protocols := file http
demuxers := mpegts mpegtsraw mov aac mp3 ogg
parsers := aac aac_latm mpegaudio opus vorbis
decoders := aac aac_at aac_fixed aac_latm mp3 opus vorbis \
            pcm_f16le pcm_f24le pcm_f32be pcm_f32le \
    	    pcm_s16be pcm_s16le pcm_s24be pcm_s24le pcm_s32be pcm_s32le pcm_s8

$(MAKEFILE): src/Makefile
	cd out/$(TARGET_PRODUCT) && \
        PKG_CONFIG_LIBDIR= PKG_CONFIG_PATH=$(PKG_CONFIG_DIR) $(CURDIR)/src/configure \
        --prefix=$(OUT) \
        --enable-cross-compile --arch=$(TARGET) --target-os=linux --cross-prefix=$(TARGET)- \
        --extra-cflags='$(TARGET_CFLAGS)' \
        --disable-static \
        --enable-shared \
        --enable-pic \
        --disable-programs \
        --disable-doc \
        --disable-avdevice \
        --disable-swscale \
        --disable-postproc \
        --disable-avfilter \
        --disable-pixelutils \
        --disable-everything \
        $(foreach p,$(protocols),--enable-protocol=$(p)) \
        $(foreach d,$(demuxers),--enable-demuxer=$(d)) \
        $(foreach p,$(parsers),--enable-parser=$(p)) \
        $(foreach d,$(decoders),--enable-decoder=$(d))

## clean

clean:
	-[ ! -f $(MAKEFILE) ] || $(MAKE) -C $(make_dir) uninstall distclean
	rm -rf out/$(TARGET_PRODUCT)

## fetch

GIT_REPO := https://git.ffmpeg.org/ffmpeg.git
GIT_COMMIT := n4.0
include ../fetch.mk

fetch::
	[ ! -d src ] || (cd src && git checkout -- .)
	@echo "Applying patches..."
	cd src && patch -p1 < ../0001-Add-WHA-player-version.patch
	cd src && patch -p1 < ../0002-Strip-962-samples-added-by-decoder-in-case-of-SBR.patch
	cd src && patch -p1 < ../0003-Strip-decoder-delay-529-samples-if-LAME-header-is-mi.patch

