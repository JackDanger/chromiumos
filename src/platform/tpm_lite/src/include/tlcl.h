/* Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* TPM Lightweight Command Library.
 *
 * A low-level library for interfacing to TPM hardware or an emulator.
 */

#ifndef TPM_LITE_TLCL_H_
#define TPM_LITE_TLCL_H_

#include <stdint.h>

/* Call this first.
 */
void TlclLibinit(void);

/* Sends a TPM_Startup(ST_CLEAR).  Note that this is a no-op for the emulator,
 * because it runs this command during initialization.
 */
void TlclStartup(void);

/* Run the self test.  Note---this is synchronous.  To run this in parallel
 * with other firmware, use ContinueSelfTest.
 */
void TlclSelftestfull(void);

/* Defines a space with permission [perm].  [index] is the index for the space,
 * [size] the usable data size.  Errors are ignored.
 */
void TlclDefineSpace(uint32_t index, uint32_t perm, uint32_t size);

/* Writes [length] bytes of [data] to space at [index].  The TPM error code is
 * returned (0 for success).
 */
uint32_t TlclWrite(uint32_t index, uint8_t *data, uint32_t length);

/* Reads [length] bytes from space at [index] into [data].  The TPM error code
 * is returned (0 for success).
 */
uint32_t TlclRead(uint32_t index, uint8_t *data, uint32_t length);

/* Write-locks space at [index].
 */
void TlclWriteLock(uint32_t index);

/* Read-locks space at [index].
 */
void TlclReadLock(uint32_t index);

/* Asserts physical presence in software.
 */
void TlclAssertPhysicalPresence(void);

/* Sets the nvLocked bit.
 */
void TlclSetNvLocked(void);

#endif  /* TPM_LITE_TLCL_H_ */
