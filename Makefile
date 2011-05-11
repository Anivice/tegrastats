# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

SUBDIRS=tegrastats dfs_stress
all:
	for d in $(SUBDIRS); do \
		$(MAKE) DEBUG=1 -C $$d; \
	done

install:
	for d in $(SUBDIRS); do \
		$(MAKE) -C $$d install; \
	done

clean:
	for d in $(SUBDIRS); do \
		$(MAKE)-C $$d clean; \
	done
