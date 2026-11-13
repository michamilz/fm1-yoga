#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""A README as the site's manual (base/tools/manual_html.py's Markdown), for tools/site.sh:

  tools/readme_manual.py README.md OUT.md IMG_PREFIX OTHER_LANG_URL

GitHub's own Markdown made plain for it: the contents list as titles (the generator links them itself), in-page links
to the generator's anchors, the callout without its [!IMPORTANT] tag, the pictures under IMG_PREFIX, and the link to
the README in the other language to its manual."""
import re
import sys


def github_anchor(t):
    t = re.sub(r"[^\w\- ]", "", t.strip().lower())
    return t.replace(" ", "-")


def site_anchor(t):                                   # manual_html.py's slug()
    return re.sub(r"[^a-z0-9]+", "-", t.lower()).strip("-")


src, dst, img, other = sys.argv[1:5]
lines = open(src, encoding="utf-8").read().split("\n")
anchors = {}
for ln in lines:
    m = re.match(r"#{2,3} (.+)", ln)
    if m:
        anchors[github_anchor(m.group(1))] = site_anchor(m.group(1))
out, contents = [], False
for ln in lines:
    if re.match(r"## (Contents|Inhalt)$", ln):
        contents = True
        out.append("## Contents")
        continue
    if contents and ln.strip() == "---":
        contents = False
    if contents:
        ln = re.sub(r"^(\d+\.) \[(.+)\]\(#[^)]*\)$", r"\1 \2", ln)
    if ln.strip() == "> [!IMPORTANT]":
        continue
    ln = re.sub(r"^> - ", "> ", ln)
    ln = ln.replace("docs/img/", img)
    ln = re.sub(r"\((README(-de)?\.md)\)", "(" + other + ")", ln)
    ln = re.sub(r"\(#([^)]+)\)", lambda m: "(#" + anchors.get(m.group(1), m.group(1)) + ")", ln)
    out.append(ln)
open(dst, "w", encoding="utf-8").write("\n".join(out))
