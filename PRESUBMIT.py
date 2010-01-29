# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for Chromium OS.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into gcl and git cl.
"""

_EXCLUDED_PATHS = (
    r".*[\\\/]debian[\\\/]rules$",
)

_TEXT_FILES = (
    r".*\.txt",
    r".*\.json",
)

_LICENSE_HEADER = (
     r".*? Copyright \(c\) 20[0-9\-]{2,7} The Chromium Authors\. All rights "
       r"reserved\." "\n"
     r".*? Use of this source code is governed by a BSD-style license that can "
       "be\n"
     r".*? found in the LICENSE file\."
       "\n"
)


def _CommonChecks(input_api, output_api):
  results = []
  # This loads the default black list and adds our black list.  See
  # presubmit_support.py InputApi.FilterSourceFile for the (simple)
  # usage.
  black_list = input_api.DEFAULT_BLACK_LIST + _EXCLUDED_PATHS
  white_list = input_api.DEFAULT_WHITE_LIST + _TEXT_FILES
  sources = lambda x: input_api.FilterSourceFile(x, black_list=black_list)
  text_files = lambda x: input_api.FilterSourceFile(x, black_list=black_list,
                                                    white_list=white_list)
  results.extend(input_api.canned_checks.CheckLongLines(
      input_api, output_api, sources))
  results.extend(input_api.canned_checks.CheckChangeHasNoTabs(
      input_api, output_api, sources))
  results.extend(input_api.canned_checks.CheckChangeHasNoStrayWhitespace(
      input_api, output_api, sources))
  results.extend(input_api.canned_checks.CheckChangeHasBugField(
      input_api, output_api))
  results.extend(input_api.canned_checks.CheckChangeHasTestField(
      input_api, output_api))
  results.extend(input_api.canned_checks.CheckLicense(
      input_api, output_api, _LICENSE_HEADER, sources))
  return results


def CheckChangeOnUpload(input_api, output_api):
  results = []
  results.extend(_CommonChecks(input_api, output_api))
  return results


def CheckChangeOnCommit(input_api, output_api):
  results = []
  results.extend(_CommonChecks(input_api, output_api))
  # Make sure the tree is 'open'.  If not, don't submit.
  # TODO: Run it in a separate thread to parallelize checks?
  results.extend(CheckTreeIsOpen(
      input_api,
      output_api,
      'http://chromiumos-status.appspot.com/status',
      '0',
      'http://chromiumos-status.appspot.com/current?format=raw'))
  # Check to see if there's a backlog on the pending CL queue and warn
  # about it if there is.
  results.extend(CheckPendingBuilds(
      input_api,
      output_api,
      'http://build.chromium.org/buildbot/chromiumos/json/builders',
      6,
      []))
  return results

def CheckTreeIsOpen(input_api, output_api, url, closed, url_text):
  """Similar to the one in presubmit_canned_checks except it shows an helpful
  status text instead.
  """
  assert(input_api.is_committing)
  try:
    connection = input_api.urllib2.urlopen(url)
    status = connection.read()
    connection.close()
    if input_api.re.match(closed, status):
      long_text = status + '\n' + url
      try:
        connection = input_api.urllib2.urlopen(url_text)
        long_text = connection.read().strip()
        connection.close()
      except IOError:
        pass
      return [output_api.PresubmitError("The tree is closed.",
                                        long_text=long_text)]
  except IOError:
    pass
  return []


def CheckPendingBuilds(input_api, output_api, url, max_pendings, ignored):
  try:
    connection = input_api.urllib2.urlopen(url)
    raw_data = connection.read()
    connection.close()
    try:
      import simplejson
      data = simplejson.loads(raw_data)
    except ImportError:
      # simplejson is much safer. But we should be just fine with this:
      data = eval(raw_data.replace('null', 'None'))
    out = []
    for (builder_name, builder)  in data.iteritems():
      if builder_name in ignored:
        continue
      pending_builds_len = len(builder.get('pending_builds', []))
      if pending_builds_len > max_pendings:
        out.append('%s has %d build(s) pending' %
                   (builder_name, pending_builds_len))
    if out:
      return [output_api.PresubmitPromptWarning(
          'Build(s) pending. It is suggested to wait that no more than %d '
              'builds are pending.' % max_pendings,
          long_text='\n'.join(out))]
  except IOError:
    # Silently pass.
    pass
  return []
