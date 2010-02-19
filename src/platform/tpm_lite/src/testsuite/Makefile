# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

LIBTLCL = ../tlcl/libtlcl.a
LIBTPMEMU = ../../../../third_party/tpm-emulator/tpmemu/libtpmemu.a
CC ?= cc

readonly: readonly.o $(LIBTLCL) $(LIBTPMEMU)
	$(CC) -g readonly.o -o readonly \
	$(LIBTLCL) \
	$(LIBTPMEMU) \
	-lgmp

readonly.o: readonly.c
	cc -g -c -Werror -Wall -pedantic -ansi readonly.c -I../include

clean:
	rm -f readonly *.o *~
