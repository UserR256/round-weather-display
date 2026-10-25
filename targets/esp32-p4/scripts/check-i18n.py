#!/usr/bin/env python3
"""Sanity-checks main/i18n_nl.inc against the sources.

  * every tr("...") / STORM_TR("...") literal has a Dutch entry
  * printf conversions (%s, %d, %%, ...) match between English and Dutch
  * no duplicate keys, and the Dutch text is ASCII (the built-in LVGL fonts have
    no accented letters; the degree sign is written as \\xc2\\xb0)

Strings that are only translated through a variable (weekday and month names,
compass names, alert labels) cannot be found this way; keep them in the table
by hand.

Usage: python3 scripts/check-i18n.py   (exit status 1 when something is wrong)
"""
import glob
import os
import re
import sys

MAIN = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "main")
LITERAL = r'"(?:[^"\\]|\\.)*"'

table = {}
problems = []
for line in open(os.path.join(MAIN, "i18n_nl.inc"), encoding="utf-8"):
    m = re.match(r'\s*\{\s*(%s)\s*,\s*(%s)\s*\},' % (LITERAL, LITERAL), line)
    if not m:
        continue
    en, nl = m.group(1)[1:-1], m.group(2)[1:-1]
    if en in table:
        problems.append("duplicate key: " + en)
    table[en] = nl

spec = lambda s: re.findall(r"%[-0-9.]*[a-z%]", s)
for en, nl in table.items():
    if spec(en) != spec(nl):
        problems.append("format mismatch: %r -> %r" % (en, nl))
    if any(ord(c) > 127 for c in nl.replace("\\xc2\\xb0", "")):
        problems.append("non-ASCII Dutch text: " + nl)

used = {}
for path in glob.glob(os.path.join(MAIN, "*.[ch]")):
    if os.path.basename(path) in ("i18n.c", "i18n.h"):
        continue
    text = open(path, encoding="utf-8", errors="replace").read()
    for m in re.finditer(r'\b(?:STORM_TR|tr)\(\s*(%s)\s*\)' % LITERAL, text):
        used.setdefault(m.group(1)[1:-1], os.path.basename(path))

for key, where in sorted(used.items()):
    if key not in table:
        problems.append("missing translation (%s): %s" % (where, key))

print("%d entries, %d tr() literals checked" % (len(table), len(used)))
for p in problems:
    print("PROBLEM:", p)
sys.exit(1 if problems else 0)
