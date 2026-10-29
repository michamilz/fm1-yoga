#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""The README's pictures, drawn (no photographs of the device: they are not ours to use).

  tools/gen_docs_figures.py

Runs the screenshot script in the host simulator (tests/scenarios/docs.txt, app/build/host/yoga_host: build it
first with ./build.sh --host), then writes into docs/img/:

  panel.png        the FM-1 front panel, the firmware's screen in it, each control labelled with what it does
  schnellstart.png the same panel, the five steps to a first session numbered
  bildschirm.png   the player's screen, each part labelled
  tastatur.png     the keys: the instrument on the long keys, the mutes on the small ones
  screen-*.png     the screenshots, twice their size

The panel's layout follows the FM-1 (after AMB-1's tools/gen_manual_figures.py by Charles Vestal, MPL-2.0): two
knob pairs and the octave buttons on the left, the screen in the middle, four knobs and two rows of six buttons on
the right, the keys below: 11 small keys in groups of 3, 2, 3, 2, 1 over 16 long keys. Fonts: Barlow (SIL OFL 1.1,
from fm1-base)."""
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs" / "img"
SHOTS = ROOT / "app" / "build" / "docs"
FONTS = ROOT / "base" / "assets" / "fonts"
S = 2                                                   # drawn at twice the size, for sharp pictures

BG, BODY, TRAY, KNOB, BTN, LABEL = "#f7f3ea", "#3a3d42", "#26282c", "#1f2124", "#2e3135", "#e8e8ea"
KEY_W, KEY_B, WHAT, WARM, BADGE, INK = "#d9dadc", "#c9cacd", "#7fd0c8", "#e2b56f", "#f08a5d", "#2a2c30"
BLACK_AFTER = [0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14]      # small key k sits after long key BLACK_AFTER[k]


def font(weight, size):
    name = {"bold": "BarlowSemiCondensed-Bold.ttf", "semi": "BarlowSemiCondensed-SemiBold.ttf",
            "medium": "BarlowSemiCondensed-Medium.ttf"}[weight]
    return ImageFont.truetype(str(FONTS / name), size * S)


class Canvas:
    def __init__(self, w, h, bg=BG):
        self.im = Image.new("RGB", (w * S, h * S), bg)
        self.d = ImageDraw.Draw(self.im)

    def rect(self, x, y, w, h, r, fill, outline=None, width=1):
        self.d.rounded_rectangle([x * S, y * S, (x + w) * S, (y + h) * S], r * S, fill=fill, outline=outline,
                                 width=width * S)

    def circle(self, x, y, r, fill, outline=None, width=1):
        self.d.ellipse([(x - r) * S, (y - r) * S, (x + r) * S, (y + r) * S], fill=fill, outline=outline,
                       width=width * S)

    def line(self, x0, y0, x1, y1, fill, width=2):
        self.d.line([x0 * S, y0 * S, x1 * S, y1 * S], fill=fill, width=width * S)

    def text(self, x, y, s, f, fill, anchor="mm"):
        self.d.text((x * S, y * S), s, font=f, fill=fill, anchor=anchor)

    def lines(self, x, y, s, f, fill, step=15, anchor="mm"):
        for i, t in enumerate(s.split("|")):
            if t:
                self.text(x, y + i * step, t, f, fill, anchor)

    def paste(self, im, x, y, scale):
        im = im.convert("RGB").resize((im.width * scale * S, im.height * scale * S), Image.NEAREST)
        self.im.paste(im, (x * S, y * S))

    def badge(self, x, y, n):
        self.circle(x, y, 15, BADGE, outline="white", width=2)
        self.text(x, y + 1, str(n), font("bold", 18), "white")

    def save(self, name):
        OUT.mkdir(parents=True, exist_ok=True)
        self.im.save(OUT / name, optimize=True)
        print("docs/img/" + name)


# ---- the panel ----------------------------------------------------------------------------------------------------
KNOBS = {  # name: (x, y, what it does)
    "MASTER": (105, 92, "Lautstärke"), "SELECT": (225, 92, "Programm|Seite blättern"),
    "PRESETS": (105, 222, "Dauer"), "ALGORITHM": (225, 222, "ISO / BIN"),
    "KNOB1": (690, 92, "KLANG-Lautst.|Seite: Wert 1"), "KNOB2": (790, 92, "GARTEN-Lautst.|Seite: Wert 2"),
    "KNOB3": (890, 92, "MANUELL: Hz|Seite: Wert 3"), "KNOB4": (990, 92, "MANUELL: Ton|Seite: Wert 4"),
}
BUTTONS = [  # name, what it does (two rows of six)
    ("FX", "KLANG"), ("SEL", "INSTRU-|MENT"), ("ENV", "GARTEN"), ("LFO", "TAKT"), ("EDIT", ""), ("GLO", "SETUP"),
    ("HOME", "PLAYER|halten: Stopp"), ("SAVE", "SPEI-|CHERN"), ("ARP", ""), ("SEQ", ""), ("PLAY", "START|PAUSE"),
    ("REC", "halten:|AUFWACHEN"),
]
PANEL_W, PANEL_H = 1100, 700


def button_xy(i):
    return 640 + (i % 6) * 72, 186 + (i // 6) * 78


def draw_panel(c, screen, labels=True, keys_text=False):
    lbl, small, what = font("semi", 15), font("semi", 13), font("medium", 13)
    c.rect(4, 4, PANEL_W - 8, PANEL_H - 8, 44, BODY)
    for name, (x, y, does) in KNOBS.items():
        c.circle(x, y, 30, TRAY)
        c.circle(x, y, 25, KNOB, outline="#44474c", width=2)
        c.line(x, y - 20, x, y - 10, LABEL, 3)
        c.text(x, y - 46, name, lbl, LABEL)
        if labels:
            c.lines(x, y + 44, does, what, WHAT)
    for i, (x, t) in enumerate([(70, "OCT−"), (170, "OCT+")]):
        c.rect(x, 300, 88, 36, 8, BTN, outline="#44474c")
        c.text(x + 44, 318, t, small, LABEL)
    if labels:
        c.text(165, 352, "frei", what, WHAT)
    c.rect(318, 34, 280, 280, 18, "#111214")
    c.paste(screen, 338, 54, 1)
    for i, (name, does) in enumerate(BUTTONS):
        x, y = button_xy(i)
        c.rect(x, y, 62, 40, 8, BTN, outline="#44474c")
        if name == "PLAY":
            c.lines(x + 31, y + 14, "PLAY|STOP", font("semi", 11), LABEL, 12)
        else:
            c.text(x + 31, y + 20, name, small, LABEL)
        if labels and does:
            c.lines(x + 31, y + 52, does, font("medium", 12), WHAT, 13)
    draw_keys(c, 30, 380, 1040, 300, keys_text)


def draw_keys(c, x0, y0, w, h, text=True):
    c.rect(x0, y0, w, h, 22, TRAY)
    ww = (w - 30) / 16
    wy, wh = y0 + 100, h - 115
    for i in range(16):
        x = x0 + 15 + i * ww
        c.rect(x + 3, wy, ww - 6, wh, 12, KEY_W)
    for k, after in enumerate(BLACK_AFTER):
        x = x0 + 15 + (after + 1) * ww - ww * 0.36
        c.rect(x, y0 + 16, ww * 0.72, 74, 12, KEY_B)
    if text:
        f = font("medium", 13)
        c.text(x0 + w / 2, wy + wh / 2, "Tasten: siehe Bild „Tastatur“", f, "#6b6e73")


def panel_figure(screen):
    c = Canvas(PANEL_W, PANEL_H)
    draw_panel(c, screen)
    c.save("panel.png")


def quickstart_figure(screen):
    c = Canvas(PANEL_W, PANEL_H)
    draw_panel(c, screen, labels=False)
    steps = [(KNOBS["MASTER"][0], KNOBS["MASTER"][1], 1, "Lautstärke"),
             (KNOBS["SELECT"][0], KNOBS["SELECT"][1], 2, "Programm"),
             (KNOBS["PRESETS"][0], KNOBS["PRESETS"][1], 3, "Dauer"),
             (KNOBS["ALGORITHM"][0], KNOBS["ALGORITHM"][1], 4, "ISO / BIN")]
    x, y = button_xy(10)
    steps.append((x + 31, y + 20, 5, "Start"))
    for (sx, sy, n, t) in steps:
        c.circle(sx, sy, 34 if n < 5 else 0, None, outline=BADGE, width=3) if n < 5 else \
            c.rect(sx - 35, sy - 24, 70, 48, 10, None, outline=BADGE, width=3)
        c.badge(sx - 52, sy, n) if n < 5 else c.badge(sx - 52, sy - 24, n)
        c.text(sx, sy + 50, t, font("bold", 15), BADGE)
    c.save("schnellstart.png")


# ---- the screen, explained ------------------------------------------------------------------------------------------
def screen_figure(screen):
    W, H = 1000, 560
    c = Canvas(W, H)
    sx, sy, k = 300, 40, 2                                       # the screen at twice its size
    c.rect(sx - 20, sy - 20, 240 * k + 40, 240 * k + 40, 20, "#111214")
    c.paste(screen, sx, sy, k)
    f, fb = font("medium", 16), font("bold", 16)
    notes = [  # (y on the screen, side, title, text)
        (16, "L", "Name / Meldung", "kurz: was du gerade|eingestellt hast"),
        (16, "R", "ISO oder BIN", "Lautsprecher oder|Kopfhörer"),
        (46, "L", "Programm", ""),
        (68, "L", "Phase", "Ankommen, Vertiefen, Ruhen,|Aufwachen; beim Übergang|z. B. „174 > 396“"),
        (110, "R", "Atem-Ring", "wird beim Einatmen weit,|beim Ausatmen eng"),
        (120, "L", "Grundton", "in Hertz"),
        (190, "R", "Gehirnwelle", "Band (Delta, Theta …)|und Frequenz"),
        (208, "L", "Restzeit", "bei ENDLOS: vergangene Zeit"),
        (221, "R", "Fortschritt", ""),
        (233, "L", "Drehregler K1–K4", "KLANG, GARTEN; Welle und Ton|nur bei MANUELL"),
    ]
    for (y, side, title, text) in notes:
        py = sy + y * k
        if side == "L":
            c.line(sx - 26, py, sx + 6, py, BADGE, 2)
            c.circle(sx + 6, py, 4, BADGE)
            c.text(sx - 34, py - (8 if text else 0), title, fb, INK, "rm")
            c.lines(sx - 34, py + 12, text, f, "#55585d", 18, "rm")
        else:
            ex = sx + 240 * k
            c.line(ex - 6, py, ex + 26, py, BADGE, 2)
            c.circle(ex - 6, py, 4, BADGE)
            c.text(ex + 34, py - (8 if text else 0), title, fb, INK, "lm")
            c.lines(ex + 34, py + 12, text, f, "#55585d", 18, "lm")
    c.save("bildschirm.png")


# ---- the keys --------------------------------------------------------------------------------------------------------
def keys_figure():
    W, H = 1100, 560
    c = Canvas(W, H)
    x0, y0, w, h = 30, 150, 1040, 320
    draw_keys(c, x0, y0, w, h, text=False)
    ww = (w - 30) / 16
    fb, f, fs = font("bold", 15), font("medium", 14), font("semi", 13)
    # the long keys: their numbers, and what they play
    for i in range(16):
        x = x0 + 15 + i * ww + ww / 2
        c.text(x, y0 + h - 30, str(i + 1), fs, INK)
    c.text(x0 + 15 + ww / 2, y0 + 160, "DING", fb, BADGE)
    bands = [(1, 8, "Skala"), (9, 14, "Skala, eine Oktave höher"), (15, 15, "TAK")]
    for (a, b, t) in bands:
        xa, xb = x0 + 15 + a * ww + 8, x0 + 15 + (b + 1) * ww - 8
        c.line(xa, y0 + h + 18, xb, y0 + h + 18, INK, 2)
        c.line(xa, y0 + h + 12, xa, y0 + h + 18, INK, 2)
        c.line(xb, y0 + h + 12, xb, y0 + h + 18, INK, 2)
        c.text((xa + xb) / 2, y0 + h + 36, t, fb, INK)
    c.text(x0 + 15 + 15.5 * ww, y0 + h + 56, "(nur Handpan)", f, "#55585d")
    c.text(x0 + w / 2, H - 22, "Lange Tasten: das Instrument (Handpan, Kalimba, Zunge, Schalen, Koshi), mit Anschlagdynamik",
           f, "#55585d")
    # the small keys: what they mute; lit while it sounds
    names = ["KLANG", "Drone", "Atem", "Schalen", "Raum", "GARTEN", "Pads", "Glocken", "Bass", "Echo", "ALLES"]
    for k, after in enumerate(BLACK_AFTER):
        x = x0 + 15 + (after + 1) * ww
        col = WHAT if k < 5 else WARM if k < 10 else "#9aa0a6"
        c.rect(x - ww * 0.36, y0 + 16, ww * 0.72, 74, 12, col)
        c.text(x, y0 + 53, str(k + 1), fs, INK)
        ly = y0 - 24 - (k % 2) * 34
        c.line(x, ly + 10, x, y0 + 12, col, 2)
        c.text(x, ly, names[k], fb if names[k].isupper() else f, INK)
    c.text(x0 + w / 2, 26, "Kleine Tasten: stummschalten und wieder an (leuchtet = klingt)", fb, INK)
    for (x, t, col) in [(220, "Yoga-Schicht (KLANG)", WHAT), (560, "GARTEN", WARM), (850, "alles", "#9aa0a6")]:
        c.rect(x - 70, 46, 14, 14, 3, col)
        c.text(x - 50, 53, t, f, INK, "lm")
    c.save("tastatur.png")


def main():
    host = ROOT / "app" / "build" / "host" / "yoga_host"
    if not host.exists():
        sys.exit("build the host simulator first: ./build.sh --host")
    SHOTS.mkdir(parents=True, exist_ok=True)
    subprocess.run([str(host), str(ROOT / "tests" / "scenarios" / "docs.txt"), str(SHOTS)], check=True,
                   stdout=subprocess.DEVNULL)
    for p in sorted(SHOTS.glob("*.png")):
        im = Image.open(p).convert("RGB")
        im.resize((im.width * 2, im.height * 2), Image.NEAREST).save(OUT / ("screen-" + p.name), optimize=True)
        print("docs/img/screen-" + p.name)
    running = Image.open(SHOTS / "laeuft.png")
    panel_figure(running)
    quickstart_figure(Image.open(SHOTS / "bereit.png"))
    screen_figure(Image.open(SHOTS / "uebergang.png"))
    keys_figure()


if __name__ == "__main__":
    main()
