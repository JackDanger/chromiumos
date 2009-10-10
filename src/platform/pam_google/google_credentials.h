// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// GoogleCredentials is the interface for objects that wrap up a set
// of credentials with which we can authenticate to Google.

#ifndef CHROMEOS_PAM_GOOGLE_CREDENTIALS_H_
#define CHROMEOS_PAM_GOOGLE_CREDENTIALS_H_

namespace chromeos_pam {

class GoogleCredentials {
 public:
  GoogleCredentials();
  virtual ~GoogleCredentials();

  /**
   * Format
   * Formats the credentials into a payload that is ready to be sent
   * to Google.
   * Parameters:
   *  payload - a buffer for output.  Cannot be NULL.
   *  length - the amount of space in the buffer.
   * Returns:
   *  -1 if formatting fails,
   *  number of bytes written to payload otherwise.
   */
  virtual int Format(char *payload, int length) = 0;

  /**
   * GetActiveUser
   * Returns the user for which the OS is to create a login session.
   * Parameters:
   *  name_buffer - output buffer.
   *  length - amount of space in name_buffer
   */
  virtual void GetActiveUser(char *name_buffer, int length) = 0;

  /**
     * GetActiveUserFull
     * Returns the full user name for which the OS is to create a login session.
     * Parameters:
     *  name_buffer - output buffer.
     *  length - amount of space in name_buffer
     */
    virtual void GetActiveUserFull(char *name_buffer, int length) = 0;

#ifdef CHROMEOS_PAM_LOCALACCOUNT
  // returns true if we're willing to accept these credentials without
  // talking to Google.
  virtual bool IsLocalAccount() = 0;
#endif

  // returns true if we're willing to send these credentials to Google.
  virtual bool IsAcceptable() = 0;

  // stores credentials in offline login store
  virtual void StoreCredentials() = 0;

  // returns a weak hash of the current credentials
  virtual void GetWeakHash(char *hash_buffer, int length) = 0;

  // checks if the credentials are in the offline login store
  virtual bool ValidForOfflineLogin() = 0;
};

}  // namespace chromeos_pam

#endif  // CHROMEOS_PAM_GOOGLE_CREDENTIALS_H_
