# Host unit tests for the hardware-independent logic (no Flipper SDK needed).
#   make -C tests          build and run with AddressSanitizer + UBSan
#   make -C tests clean

CC      ?= cc
ROOT    := ..
BUILD   := build
CFLAGS  += -std=c11 -Wall -Wextra -Werror -Wshadow -Wconversion -Wno-sign-conversion \
           -g -O1 -I$(ROOT) -fsanitize=address,undefined -fno-omit-frame-pointer
LDFLAGS += -fsanitize=address,undefined

SOURCES := test_main.c \
           $(ROOT)/src/counter.c \
           $(ROOT)/src/profiles.c \
           $(ROOT)/src/confirm.c \
           $(ROOT)/src/feedback.c \
           $(ROOT)/src/settings_codec.c \
           $(ROOT)/src/storage.c \
           $(ROOT)/src/counter_input.c

.PHONY: test clean
test: $(BUILD)/dhikr_tests
	./$(BUILD)/dhikr_tests

$(BUILD)/dhikr_tests: $(SOURCES) $(wildcard $(ROOT)/include/*.h) test_framework.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(SOURCES) -o $@ $(LDFLAGS)

clean:
	rm -rf $(BUILD)
