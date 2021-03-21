
CUR_DIR		:= .
TOP_DIR		:=$(BUILD_TOP_DIR)
OBJ_DIR 	:=$(CUR_DIR)/Obj/
INSTALL_DIR	:=$(RELEASE_DIR)/lib

include $(TOP_DIR)/Rule.make
include $(TOP_DIR)/Makefile.Inc

####### Compiler, tools and options
INCPATH       = $(INCLUDEPATH)
RANLIB        = 
COPY_FILE     = $(COPY)
COPY_DIR      = $(COPY) -r
DEL_FILE      = rm -f
SYMLINK       = ln -f -s
DEL_DIR       = rmdir
MOVE          = mv -f
CHK_DIR_EXISTS= test -d
MKDIR         = mkdir -p

CFLAGS  := $(OPT) $(DEBUG) -Wall -fPIC $(DEFS) $(SCFLAGS) $(PCGLAGS)
CFLAGS +=-DNOT_HAVE_SA_LEN

OBJ :=

OBJ += ./nsec.o
OBJ += ./uDNS.o
OBJ += ./mDNSPosix.o
OBJ += ./dnssec.o
OBJ += ./DNSCommon.o
OBJ += ./CryptoAlg.o
OBJ += ./CryptoAlg.o
OBJ += ./mDNSUNP.o
OBJ += ./ExampleClientApp.o
OBJ += ./mDNS.o
OBJ += ./Responder.o
OBJ += ./DNSDigest.o
OBJ += ./GenLinkedList.o

TARGET = $(INSTALL_DIR)/libmDNS.a

all debug: $(TARGET) 

$(TARGET): $(OBJ)
	$(AR) cq $(TARGET) $(OBJ)


	
.PHONY:
%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

clean: 
	-$(DEL_FILE) $(OBJ) $(TARGET)

distclean:
	-$(DEL_FILE) $(OBJ) $(TARGET)
