# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

CFLAGS 	+= -DNV_IS_LDK=1
TARGET	= dfs_stress
SRC	= main.c

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	$(RM) $(TARGET)

.PHONY:	clean
