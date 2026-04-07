# Makefile — nRettap Schwung module

CC_NATIVE := gcc
CC_CROSS := aarch64-linux-gnu-gcc
CFLAGS := -std=c99 -Wall -Wextra -Werror -O2 -fPIC -Isrc/dsp -Isrc/host

DSP_SRCS := src/dsp/nrettap_engine.c
HOST_SRCS := src/host/nrettap_plugin.c
HOST_HDRS := src/host/midi_fx_api_v1.h src/host/plugin_api_v1.h
ALL_SRCS := $(DSP_SRCS) $(HOST_SRCS)

.PHONY: native
native: build/native/dsp.so

build/native/dsp.so: $(ALL_SRCS) $(HOST_HDRS) | build/native
	$(CC_NATIVE) $(CFLAGS) -shared -o $@ $(ALL_SRCS)

build/native:
	mkdir -p build/native

.PHONY: aarch64
aarch64: build/aarch64/dsp.so

build/aarch64/dsp.so: $(ALL_SRCS) $(HOST_HDRS) | build/aarch64
	$(CC_CROSS) $(CFLAGS) -shared -o $@ $(ALL_SRCS)

build/aarch64:
	mkdir -p build/aarch64

.PHONY: test
test: build/native/nrettap_engine_test build/native/nrettap_midi_fx_test
	build/native/nrettap_engine_test
	build/native/nrettap_midi_fx_test

build/native/nrettap_engine_test: tests/nrettap_engine_test.c $(DSP_SRCS) | build/native
	$(CC_NATIVE) $(CFLAGS) -o $@ $^

build/native/nrettap_midi_fx_test: tests/nrettap_midi_fx_test.c $(ALL_SRCS) $(HOST_HDRS) | build/native
	$(CC_NATIVE) $(CFLAGS) -o $@ tests/nrettap_midi_fx_test.c $(ALL_SRCS)

.PHONY: smoke-test
smoke-test: test

.PHONY: check-symbols
check-symbols: build/native/dsp.so
	@nm $< | grep move_midi_fx_init \
	  && echo "OK: move_midi_fx_init exported" \
	  || echo "FAIL: entry point not found"

.PHONY: clean
clean:
	rm -rf build
