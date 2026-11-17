# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

TARGET   := tegrastats
SOURCES  := $(wildcard src/*.c)
OBJECTS  := $(SOURCES:.c=.o)
DEPENDS  := $(OBJECTS:.o=.d)

CPPFLAGS += -Iinclude
CFLAGS   ?= -O2
CFLAGS   += -std=c11 -Wall -Wextra -Wpedantic

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

src/%.o: src/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c -o $@ $<

clean:
	$(RM) $(TARGET) $(OBJECTS) $(DEPENDS)

-include $(DEPENDS)

.PHONY: clean
