#!/bin/bash
# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

LOG="$1"
TGT="$2"
if [[ -z "$LOG" || -z "$TGT" ]]; then
  echo "Usage: $0 verity.INFO target.manifest" 1>&2
  exit 1
fi

TMPDIR=$(mktemp -d /tmp/build_manifest.XXXXX)
if [[ -z "$TMPDIR" ]]; then
  echo "Failed to make a tempdir" 1>&2
  exit 1
fi
trap "rm -rf $TMPDIR" EXIT

learned=$TMPDIR/learned
entries=$TMPDIR/entries
grep '\[learning\]' $LOG | cut -f7- -d' ' | sort -t'|' -k1 > $learned
cut -f1 -d'|' $learned | sort -u > $entries
entry_count="$(wc -l $entries |  cut -f1 -d' ')"
echo $entry_count > $TGT

cat $entries | while read f; do
  echo -n "$f " >> $TGT
  grep $f $learned | sort -t'|' -u -n -k2 | cut -f3 -d'|' | tr -s '\n' ' ' >> $TGT
  echo "" >> $TGT
done
echo "New manifest built: $TGT $(sha1sum $TGT | cut -f1 -d' ')"
