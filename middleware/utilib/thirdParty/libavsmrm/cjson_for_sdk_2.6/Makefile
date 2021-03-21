include ../common.mk

## build

all: $(OUT)/lib/libcjson.so $(OUT)/include/cJSON.h

$(OUT)/lib/libcjson.so: src/cJSON.c src/cJSON.h
	$(CC) -shared -fPIC $(CFLAGS) $(CPPFLAGS) -o $@ $<

$(OUT)/include/cJSON.h: src/cJSON.h
	install -D $< $@

## No fetch required since files are local

## clean

clean:
	rm -f $(OUT)/lib/libcjson.so
	rm -f $(OUT)/include/cJSON.h
