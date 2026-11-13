#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# The website (web installer, emulator, download; the front page in English, and in German at de/) from a release's
# signed package, into app/build/site:
#   tools/site.sh VERSION            (1.0-rc3: the release v1.0-rc3)
# The package comes from the GitHub release and is checked against its SHA256SUMS, so the installer offers exactly
# the file GitHub built and signed. The emulator is built here, from the checkout: check out the release's tag (or a
# commit with the same app/) first. Then commit app/build/site to the gh-pages branch.
set -e
cd "$(dirname "$0")/.."
V="${1:?usage: tools/site.sh VERSION}"
REL="https://github.com/michamilz/fm1-yoga/releases/download/v$V"
PKG="yoga-$V.fwsc"
TAGLINE_DE="Klänge für Endentspannung und Meditation"
export JIELI_TOOLCHAIN="${JIELI_TOOLCHAIN:-$HOME/.jieli/toolchain}"
export AC79_SDK="${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}"
command -v emcc >/dev/null 2>&1 || . "$HOME/emsdk/emsdk_env.sh" >/dev/null 2>&1
mkdir -p app/build/rel
curl -fsSL -o "app/build/rel/$PKG" "$REL/$PKG"
curl -fsSL -o app/build/rel/SHA256SUMS "$REL/SHA256SUMS"
WANT="$(awk -v p="$PKG" '$2 ~ ("(^|/)" p "$") {print $1}' app/build/rel/SHA256SUMS)"
GOT="$(shasum -a 256 "app/build/rel/$PKG" | cut -d' ' -f1)"
[ -n "$WANT" ] && [ "$WANT" = "$GOT" ] || { echo "$PKG: checksum $GOT, the release says ${WANT:-nothing}" >&2; exit 1; }
./build.sh --web --release "$V"
rm -rf app/build/site
python3 base/tools/make_site.py app "app/build/rel/$PKG" "$V" app/build/site
# the manuals: the READMEs (English in manual/, German in de/manual/), their pictures in manual/img
H="python3 base/tools/manual_html.py"
R=https://github.com/michamilz/fm1-yoga
N="FM1 YOGA"
T="$(sed -n 's/^tagline = "\(.*\)"/\1/p' app/fm1.toml)"
mkdir -p app/build/site/manual app/build/site/de/manual
cp -R docs/img app/build/site/manual/img
python3 tools/readme_manual.py README.md app/build/rel/manual.md img/ ../de/manual/
$H app/build/rel/manual.md app/build/site/manual/index.html --name "$N" --repo "$R" --tagline "$T"
python3 tools/readme_manual.py README-de.md app/build/rel/manual-de.md ../../manual/img/ ../../manual/
$H app/build/rel/manual-de.md app/build/site/de/manual/index.html --name "$N" --repo "$R" --tagline "$TAGLINE_DE"
# the front pages (make_site's, again: with the name in its header, as the title says it)
sed -e "s/{{VERSION}}/$V/g" -e "s/{{PKG}}/$PKG/g" app/site/landing.md > app/build/rel/landing.md
$H app/build/rel/landing.md app/build/site/index.html --name "$N" --repo "$R" --tagline "$T" --home
sed -e "s/{{VERSION}}/$V/g" -e "s/{{PKG}}/$PKG/g" app/site/landing-de.md > app/build/rel/landing-de.md
$H app/build/rel/landing-de.md app/build/site/de/index.html --name "$N" --repo "$R" --tagline "$TAGLINE_DE" --home
# the German pages: in German, their header's links one folder further up (the emulator and installer are shared)
for p in de/index.html de/manual/index.html; do
    up=../; [ "$p" = de/manual/index.html ] && up=../../
    sed -i.bak -e 's#<html lang="en">#<html lang="de">#' \
        -e "s#href=\"\(\.\./\)\{0,1\}emu/\">Emulator#href=\"${up}emu/\">Emulator#" \
        -e "s#href=\"\(\.\./\)\{0,1\}install/\">Install<#href=\"${up}install/\">Installieren<#" \
        -e 's#">Manual</a>#">Anleitung</a>#' -e 's#">Source</a>#">Quellcode</a>#' \
        -e 's#>Skip to content<#>Zum Inhalt springen<#' -e 's#<h1>Manual</h1>#<h1>Anleitung</h1>#' -e 's#<title>Manual ·#<title>Anleitung ·#' \
        -e 's#aria-label="Contents"><h2 id="contents">Contents<#aria-label="Inhalt"><h2 id="contents">Inhalt<#' "app/build/site/$p"
    rm -f "app/build/site/$p.bak"
done
sed -i.bak 's#href="\.\./manual/">Anleitung#href="./">Anleitung#' app/build/site/de/manual/index.html
rm -f app/build/site/de/manual/index.html.bak
echo "site: app/build/site ($PKG, sha256 $GOT)"
