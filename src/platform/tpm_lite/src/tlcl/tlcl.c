/* Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* A lightweight TPM command library.
 *
 * The general idea is that TPM commands are array of bytes whose fields are
 * mostly compile-time constant.  The goal is to build much of the commands at
 * compile time (or build time) and change some of the fields at run time as
 * needed.  The code in generator.c builds structures containing the commands,
 * as well as the offsets of the fields that need to be set at run time.
 */

#include "tlcl.h"

#include <string.h>
#include <tss/tcs.h>

#include "structures.h"
#include "tlcl_internal.h"
#include "tpmemu.h"
#include "tpmextras.h"

/* Gets the tag field of a TPM command.
 */
static INLINE int TpmTag(uint8_t* buffer) {
  uint16_t tag;
  FromTpmUint16(buffer, &tag);
  return (int) tag;
}

/* Sets the size field of a TPM command.
 */
static INLINE void SetTpmCommandSize(uint8_t* buffer, uint32_t size) {
  ToTpmUint32(buffer + 2, size);
}

/* Gets the size field of a TPM command.
 */
static INLINE int TpmCommandSize(const uint8_t* buffer) {
  uint32_t size;
  FromTpmUint32(buffer + 2, &size);
  return (int) size;
}

/* Gets the code field of a TPM command.
 */
static INLINE int TpmCommandCode(const uint8_t* buffer) {
  uint32_t code;
  FromTpmUint32(buffer + 6, &code);
  return code;
}

/* Gets the code field of a TPM result.
 */
static INLINE int TpmReturnCode(const uint8_t* buffer) {
  return TpmCommandCode(buffer);
}

/* Checks for errors in a TPM response.
 */
static void CheckResult(uint8_t* request, uint8_t* response, bool warn_only) {
  int command = TpmCommandCode(request);
  int result = TpmReturnCode(response);
  if (result != TPM_SUCCESS) {
    (warn_only? warning : error)("command 0x%x failed: 0x%x\n",
                                 command, result);
  }
}

/* Sends a request and receive a response.
 */
static void SendReceive(uint8_t* request, uint8_t* response, int max_length) {
  uint32_t response_length = max_length;
  int tag, response_tag;

  tpmemu_execute(request, TpmCommandSize(request), response, &response_length);

  /* sanity checks */
  tag = TpmTag(request);
  response_tag = TpmTag(response);
  assert(
    (tag == TPM_TAG_RQU_COMMAND &&
     response_tag == TPM_TAG_RSP_COMMAND) ||
    (tag == TPM_TAG_RQU_AUTH1_COMMAND &&
     response_tag == TPM_TAG_RSP_AUTH1_COMMAND) ||
    (tag == TPM_TAG_RQU_AUTH2_COMMAND &&
     response_tag == TPM_TAG_RSP_AUTH2_COMMAND));
  assert(response_length == TpmCommandSize(response));
}

/* Sends a command and checks the result for errors.  Note that this error
 * checking is only meaningful when running in user mode.  TODO: The entire
 * error recovery strategy in the firmware needs more work.
 */
static void Send(uint8_t* command) {
  uint8_t response[TPM_LARGE_ENOUGH_COMMAND_SIZE];
  SendReceive(command, response, sizeof(response));
  CheckResult(command, response, false);
}


/* Exported functions.
 */

void TlclLibinit(void) {
  tpmemu_init();
}

void TlclStartup(void) {
  Send(tpm_startup_cmd.buffer);
}

void TlclSelftestfull(void) {
  Send(tpm_selftestfull_cmd.buffer);
}

void TlclDefineSpace(uint32_t index, uint32_t perm, uint32_t size) {
  ToTpmUint32(tpm_nv_definespace_cmd.index, index);
  ToTpmUint32(tpm_nv_definespace_cmd.perm, perm);
  ToTpmUint32(tpm_nv_definespace_cmd.size, size);
  Send(tpm_nv_definespace_cmd.buffer);
}

uint32_t TlclWrite(uint32_t index, uint8_t* data, uint32_t length) {
  uint8_t response[TPM_LARGE_ENOUGH_COMMAND_SIZE];
  const int total_length =
    kTpmRequestHeaderLength + kWriteInfoLength + length;

  assert(total_length <= TPM_LARGE_ENOUGH_COMMAND_SIZE);
  SetTpmCommandSize(tpm_nv_write_cmd.buffer, total_length);

  ToTpmUint32(tpm_nv_write_cmd.index, index);
  ToTpmUint32(tpm_nv_write_cmd.length, length);
  memcpy(tpm_nv_write_cmd.data, data, length);

  SendReceive(tpm_nv_write_cmd.buffer, response, sizeof(response));
  CheckResult(tpm_nv_write_cmd.buffer, response, true);

  return TpmReturnCode(response);
}

uint32_t TlclRead(uint32_t index, uint8_t* data, uint32_t length) {
  uint8_t response[TPM_LARGE_ENOUGH_COMMAND_SIZE];
  uint32_t result_length;
  uint32_t result;

  ToTpmUint32(tpm_nv_read_cmd.index, index);
  ToTpmUint32(tpm_nv_read_cmd.length, length);

  SendReceive(tpm_nv_read_cmd.buffer, response, sizeof(response));
  result = TpmReturnCode(response);
  if (result == TPM_SUCCESS && length > 0) {
    uint8_t* nv_read_cursor = response + kTpmResponseHeaderLength;
    FromTpmUint32(nv_read_cursor, &result_length);
    nv_read_cursor += sizeof(uint32_t);
    memcpy(data, nv_read_cursor, result_length);
  }

  return result;
}

void TlclWriteLock(uint32_t index) {
  (void) TlclWrite(index, NULL, 0);
}

void TlclReadLock(uint32_t index) {
  (void) TlclRead(index, NULL, 0);
}

void TlclAssertPhysicalPresence(void) {
  Send(tpm_physicalpresence_cmd.buffer);
}

void TlclSetNvLocked(void) {
  TlclDefineSpace(TPM_NV_INDEX_LOCK, 0, 0);
}
