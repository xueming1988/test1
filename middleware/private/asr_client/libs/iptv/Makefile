#
# Makefile to build
#

#Directory definition
CUR_DIR:=.
TOP_DIR:=$(BUILD_TOP_DIR)
INSTALL_DIR:=$(RELEASE_DIR)/lib
INCLUDE_DIR:=$(RELEASE_DIR)/include

include $(TOP_DIR)/Rule.make
include $(TOP_DIR)/Makefile.Inc

CFLAGS:=$(subst -Werror, ,$(CFLAGS))

LIBS:=-L$(RELEASE_DIR)/lib

DESTSHLIB:=$(INSTALL_DIR)/libiptv.so
DESTSTATICLIB:=$(INSTALL_DIR)/libiptv.a

INC_FILES:=iptv.h iptv_shm.h
SRC:=iptv_main.c iptv_shm.c
OBJS:=iptv_main.o iptv_shm.o

install all release debug:create_dir copy_files $(DESTSHLIB)

$(DESTSHLIB):$(OBJS)
	$(RM) $(DESTSHLIB)
	$(RM) $(DESTSTATICLIB)
	$(LD) -shared $(LDFLAGS) $(LIBS) $(OBJS) -o $@
	$(CP) -rf $(INC_FILES) $(INCLUDE_DIR)/

copy_files:
	$(CP) iptv_msg.* ../../asr_clova/

create_dir:
	$(MKDIR) $(INSTALL_DIR)
	$(MKDIR) $(INCLUDE_DIR)

clean:
	$(RM) -r *.o $(DESTSHLIB) $(DESTSTATICLIB)

distclean:
	$(RM) -r *.o $(DESTSHLIB) $(DESTSTATICLIB)

-include $(DEPS)

