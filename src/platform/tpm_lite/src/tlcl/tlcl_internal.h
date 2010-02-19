/* Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef TPM_LITE_TLCL_INTERNAL_H_
#define TPM_LITE_TLCL_INTERNAL_H_

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef int bool;
const bool true = 1;
const bool false = 0;

#define POSSIBLY_UNUSED __attribute__((unused))

#ifdef __STRICT_ANSI__
#define INLINE
#else
#define INLINE inline
#endif

/*
 * Output an error message and quit the program.
 */
static void error(const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  fprintf(stderr, "ERROR: ");
  vfprintf(stderr, format, ap);
  va_end(ap);
  exit(1);
}

/*
 * Output a warning and continue.
 */
POSSIBLY_UNUSED
static void warning(const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  fprintf(stderr, "WARNING: ");
  vfprintf(stderr, format, ap);
  va_end(ap);
  exit(1);
}

#define assert(expr) do { if (!(expr)) { \
      error("assert fail: %s at %s:%d\n", \
            #expr, __FILE__, __LINE__); }} while(0)

/*
 * Conversion functions.  ToTpmTYPE puts a value of type TYPE into a TPM
 * command buffer.  FromTpmTYPE gets a value of type TYPE from a TPM command
 * buffer into a variable.
 */
POSSIBLY_UNUSED
static INLINE void ToTpmUint32(uint8_t *buffer, uint32_t x) {
  buffer[0] = (x >> 24);
  buffer[1] = ((x >> 16) & 0xff);
  buffer[2] = ((x >> 8) & 0xff);
  buffer[3] = x & 0xff;
}

/*
 * See comment for above function.
 */
POSSIBLY_UNUSED
static INLINE void FromTpmUint32(const uint8_t *buffer, uint32_t *x) {
  *x = ((buffer[0] << 24) |
        (buffer[1] << 16) |
        (buffer[2] << 8) |
        buffer[3]);
}

/*
 * See comment for above function.
 */
POSSIBLY_UNUSED
static INLINE void ToTpmUint16(uint8_t *buffer, uint16_t x) {
  buffer[0] = (x >> 8);
  buffer[1] = x & 0xff;
}

/*
 * See comment for above function.
 */
POSSIBLY_UNUSED
static INLINE void FromTpmUint16(const uint8_t *buffer, uint16_t *x) {
  *x = (buffer[0] << 8) | buffer[1];
}

/*
 * These numbers derive from adding the sizes of command fields as shown in the
 * TPM commands manual.
 */
const int kTpmRequestHeaderLength = 10;
const int kTpmResponseHeaderLength = 14;
const int kTpmReadInfoLength = 12;
const int kEncAuthLength = 20;

#endif
