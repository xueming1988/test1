include ../common.mk

all: config system chrono thread regex headers atomic
chrono: system
thread: system

# cflags used to build all boost libs, collected from manual b2 run
boost_cflags := \
	-DBOOST_ALL_NO_LIB=1 \
	-DBOOST_MATH_DISABLE_FLOAT128 \
	-DBOOST_THREAD_POSIX \
	-DBOOST_SYSTEM_NO_DEPRECATED \
	-DNDEBUG \
	-Wno-unused-parameter \
	-Wno-overloaded-virtual \
	-Wno-deprecated-register \
	-Wno-maybe-uninitialized

CPPFLAGS += -I$(CURDIR)/src
CFLAGS += -fPIC -O2
CXXFLAGS += $(CFLAGS)

.PHONY: system
system_sources = src/libs/system/src/error_code.cpp
system: $(OUT)/lib/libboost_system.so
$(OUT)/lib/libboost_system.so: $(system_sources) | $(OUT)/lib
	$(CXX) -shared -o $@ $^ $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS)

.PHONY: chrono
chrono_sources = $(wildcard src/libs/chrono/src/*.cpp)
chrono: $(OUT)/lib/libboost_chrono.so
$(OUT)/lib/libboost_chrono.so: $(chrono_sources) | $(OUT)/lib
	$(CXX) -shared -L$(OUT)/lib -lboost_system -o $@ $^ $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS)

.PHONY: regex
regex_sources = $(wildcard src/libs/regex/src/*.cpp)
regex: $(OUT)/lib/libboost_regex.so
$(OUT)/lib/libboost_regex.so: $(regex_sources) | $(OUT)/lib
	$(CXX) -shared -o $@ $^ $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS)

.PHONY: thread
thread_sources = \
	src/libs/thread/src/pthread/once.cpp \
	src/libs/thread/src/pthread/thread.cpp \
	src/libs/thread/src/future.cpp
thread: $(OUT)/lib/libboost_thread.so
$(OUT)/lib/libboost_thread.so: $(thread_sources) | $(OUT)/lib
	$(CXX) -shared -pthread -L$(OUT)/lib -lboost_system -o $@ $^ $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS)

.PHONY: headers
headers: $(OUT)/include/boost
$(OUT)/include/boost: src/boost | $(OUT)/include config
	ln -s -f $(CURDIR)/$< $@

.PHONY: atomic
atomic_sources = $(wildcard src/libs/atomic/src/*.cpp)
atomic: $(OUT)/lib/libboost_atomic.so
$(OUT)/lib/libboost_atomic.so: $(atomic_sources) | $(OUT)/lib
	$(CXX) -shared -o $@ $^ $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS)
	
# execute b2 which generates headers folder
.PHONY: config
config:
	cd $(dir $@) && cd src && ./bootstrap.sh --includedir=boost --with-libraries=system,chrono,regex,thread,atomic
	sed "s/REPLACE_ME/$(CXX)/" template.jam > src/project-config.jam
	cd src && ./b2

# create output directories

$(OUT)/lib $(OUT)/include:
	mkdir -p $@

# clean

clean:
	rm -f $(OUT)/lib/libboost*
	rm -f $(OUT)/include/boost

# fetch
fetch: 
	wget -O boost_1_57_0.tar.gz http://sourceforge.net/projects/boost/files/boost/1.57.0/boost_1_57_0.tar.gz/download
	
