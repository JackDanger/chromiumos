#!/usr/bin/python
# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Produces a stub implementation for OpenGLES functions."""

__authors__ = ['"The Chromium OS Authors" <chromium-os-dev@googlegroups.com>']

import re
import string
import sys

def Indent(s, limit=79):
  """Indent a code line splitting at ","."""
  parts = re.split(r',\s*', s)
  open_col = 0

  lines = ['']
  for i, part in enumerate(re.split(r',\s*', s)):
    if i:
      canidate = '%s, %s' % (lines[-1], part)
    else:
      canidate = part
    if len(canidate) < limit:
      lines[-1] = canidate
    else:
      lines[-1] += ','
      try:
        open_col = lines[-1].rindex('(') + 1
      except ValueError:
        pass
      lines.append(open_col * ' ' + part)

  return '\n'.join(lines)


class EntryPoint(object):
  def __init__(self, type, name, args, filter):
    self.type = type
    self.name = name
    self.args = args
    self.filter = filter
    self.typed_arg_string = ', '.join('%s %s' % arg for arg in self.args)

  def GenericStub(self):
    s = []
    s.append(Indent('%s %s(%s) {' % (self.type, self.name,
                                     self.typed_arg_string)))
    if self.type != 'void':
      s.append('  return (%s)0;' % self.type)
    s.append('}')
    return '\n'.join(s)

  def GenStub(self):
    assert len(self.args) == 2

    s = []
    s.append(Indent('%s %s(%s) {' % (self.type, self.name,
                                     self.typed_arg_string)))
    s.append('  GlContext* gl = GL_CONTEXT();')
    s.append('  GLsizei i;')
    s.append('  for (i = 0; i < %s; ++i)' % self.args[0][1])
    s.append('    %s[i] = gl->next_name_++;' % self.args[1][1])
    s.append('}')
    return '\n'.join(s)

  def ManualStub(self):
    s = []
    s.append(Indent('%s %s(%s) {' % (self.type, self.name,
                                     self.typed_arg_string)))

    s.append('  GlContext* gl = GL_CONTEXT();')
    s.append('}')
    return '\n'.join(s)

  def Stub(self):
    functions = {
      'STUB': self.GenericStub,
      'GEN': self.GenStub,
      'MANUAL': self.ManualStub,
    }
    return functions[self.filter]()


def Parse(header):
  """Ad-hoc parser for the entry_points definition"""
  function_re = re.compile(
      r'^\s*([A-Z]+)\(([A-Za-z_0-9* ]+?)\s*([A-Za-z_0-9]+)\s*\(([^)]*)\)\s*\)\s*$')
  arg_re = re.compile(r'^\s*([A-Za-z_0-9* ]+?)\s*([A-Za-z_0-9]+)\s*$')

  entries = []
  for line in header:
    match = function_re.match(line)
    if match:
      filter, type, name, arg_string = match.groups()
      args = []
      if arg_string != 'void':
        for arg in arg_string.split(','):
          match = arg_re.match(arg)
          assert match
          arg_type, arg_name = match.groups()
          args.append((arg_type, arg_name))
      entries.append(EntryPoint(type, name, args, filter))
  return entries


def MakeFiles(in_files, subs):
  """Make template substitutions for every file name in in_files"""
  for in_file in in_files:
    out_file = re.sub(r'\.in$', '', in_file)
    assert in_file != out_file
    template = string.Template(file(in_file).read())
    out = file(out_file, 'w')
    out.write('/* Auto-Generated from "%s".  DO NOT EDIT! */\n' % in_file)
    out.write(template.substitute(subs))
    out.close()


def CreateSubs(entries):
  """Create a dictionary containing the template substitutions for the
  OpenGL-ES stub.

  """
  subs = {}
  subs['ManualStubs'] = '\n\n'.join(entry.Stub() for entry in entries
                                    if entry.filter == 'MANUAL')
  subs['AutoStubs'] = '\n\n'.join(entry.Stub() for entry in entries
                                  if entry.filter != 'MANUAL')
  return subs


def main(args):
  templates = args[1:]

  entries = Parse(file('entry_points', 'r'))
  subs = CreateSubs(entries)
  MakeFiles(templates, subs)


if __name__ == '__main__':
  main(sys.argv)

