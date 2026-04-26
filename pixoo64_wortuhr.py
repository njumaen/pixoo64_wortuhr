#!/usr/bin/env python3
"""
Deutsche Wordclock für Pixoo64 – klassischer Raster-Stil
Alle 11 Spalten sichtbar, Wortpositionen verifiziert.

pip install requests

Verwendung:
    python pixoo64_wordclock.py --ip 192.168.1.x
    python pixoo64_wordclock.py --ip 192.168.1.x --once
"""

import argparse
import time
import base64
import requests
from datetime import datetime

# ══════════════════════════════════════════════════════════════════
# Buchstabenraster  10 × 11  (verifiziert, alle Wörter enthalten)
# ══════════════════════════════════════════════════════════════════
GRID = [
    list("ESKISTLFÜNF"),  # 0  ES·IST·FÜNF
    list("ZEHNZWANZIG"),  # 1  ZEHN·ZWANZIG
    list("DREIVIERTELD")[:11],  # 2  DREI·VIERTEL
    list("TGNACHVORJMD")[:11],  # 3  NACH(2-5)·VOR(6-8)
    list("HALBQZWÖLFPZ")[:11],  # 4  HALB·ZWÖLF
    list("ZWEINSIEBEN"),  # 5  ZWEI·EIN(S)·SIEBEN
    list("KDREIIFÜNFQ"),  # 6  DREI·FÜNF
    list("ELFNEUNVIER"),  # 7  ELF·NEUN·VIER
    list("WACHTZEHNRS"),  # 8  ACHT·ZEHN
    list("BSECHSFMUHR"),  # 9  SECHS·UHR
]
# Sicherheitshalber auf 11 kürzen
GRID = [r[:11] for r in GRID]
ROWS, COLS = 10, 11

# ══════════════════════════════════════════════════════════════════
# Wort-Positionen  (zeile, start_spalte, länge)
# ══════════════════════════════════════════════════════════════════
WORDS = {
    "ES":       [(0, 0, 2)],
    "IST":      [(0, 3, 3)],
    # Minuten
    "FÜNF_M":  [(0, 7, 4)],
    "ZEHN_M":  [(1, 0, 4)],
    "ZWANZIG": [(1, 4, 7)],
    "DREI_V":  [(2, 0, 4)],
    "VIERTEL": [(2, 4, 7)],
    "NACH":    [(3, 2, 4)],
    "VOR":     [(3, 6, 3)],
    "HALB":    [(4, 0, 4)],
    # Stunden
    "ZWÖLF":   [(4, 5, 5)],
    "EIN":     [(5, 2, 3)],
    "EINS":    [(5, 2, 4)],
    "ZWEI":    [(5, 0, 4)],
    "SIEBEN":  [(5, 5, 6)],
    "DREI":    [(6, 1, 4)],
    "FÜNF_H":  [(6, 6, 4)],
    "ELF":     [(7, 0, 3)],
    "NEUN":    [(7, 3, 4)],
    "VIER":    [(7, 7, 4)],
    "ACHT":    [(8, 1, 4)],
    "ZEHN_H":  [(8, 5, 4)],
    "SECHS":   [(9, 1, 5)],
    "UHR":     [(9, 8, 3)],
}

# Ecken für Rest-Minuten 1–4
MINUTE_DOTS = [(0, 63), (63, 63), (63, 0), (0, 0)]

# ══════════════════════════════════════════════════════════════════
# Wordclock-Logik
# ══════════════════════════════════════════════════════════════════
HOUR_KEYS = [
    "ZWÖLF", "EIN", "ZWEI", "DREI", "VIER",
    "FÜNF_H", "SECHS", "SIEBEN", "ACHT", "NEUN",
    "ZEHN_H", "ELF",
]

def get_active_words(hour: int, minute: int) -> list:
    active = ["ES", "IST"]
    m5 = (minute // 5) * 5
    h  = hour % 12
    hn = (h + 1) % 12

    # Stunde 1 Uhr genau → "EINS UHR"
    def hkey(x):
        if x == 1 and m5 == 0:
            return "EINS"
        return HOUR_KEYS[x]

    if m5 == 0:
        active += [hkey(h), "UHR"]
    elif m5 == 5:
        active += ["FÜNF_M", "NACH", HOUR_KEYS[h]]
    elif m5 == 10:
        active += ["ZEHN_M", "NACH", HOUR_KEYS[h]]
    elif m5 == 15:
        active += ["VIERTEL", "NACH", HOUR_KEYS[h]]
    elif m5 == 20:
        active += ["ZWANZIG", "NACH", HOUR_KEYS[h]]
    elif m5 == 25:
        active += ["FÜNF_M", "VOR", "HALB", HOUR_KEYS[hn]]
    elif m5 == 30:
        active += ["HALB", HOUR_KEYS[hn]]
    elif m5 == 35:
        active += ["FÜNF_M", "NACH", "HALB", HOUR_KEYS[hn]]
    elif m5 == 40:
        active += ["ZWANZIG", "VOR", HOUR_KEYS[hn]]
    elif m5 == 45:
        active += ["VIERTEL", "VOR", HOUR_KEYS[hn]]
    elif m5 == 50:
        active += ["ZEHN_M", "VOR", HOUR_KEYS[hn]]
    elif m5 == 55:
        active += ["FÜNF_M", "VOR", HOUR_KEYS[hn]]

    return active

# ══════════════════════════════════════════════════════════════════
# 3×5 Pixel-Font
# ══════════════════════════════════════════════════════════════════
FONT = {
    'A': [0b111,0b101,0b111,0b101,0b101],
    'B': [0b110,0b101,0b110,0b101,0b110],
    'C': [0b111,0b100,0b100,0b100,0b111],
    'D': [0b110,0b101,0b101,0b101,0b110],
    'E': [0b111,0b100,0b111,0b100,0b111],
    'F': [0b111,0b100,0b111,0b100,0b100],
    'G': [0b111,0b100,0b101,0b101,0b111],
    'H': [0b101,0b101,0b111,0b101,0b101],
    'I': [0b111,0b010,0b010,0b010,0b111],
    'J': [0b011,0b001,0b001,0b101,0b111],
    'K': [0b101,0b110,0b100,0b110,0b101],
    'L': [0b100,0b100,0b100,0b100,0b111],
    'M': [0b101,0b111,0b111,0b101,0b101],
    'N': [0b101,0b111,0b111,0b111,0b101],
    'O': [0b111,0b101,0b101,0b101,0b111],
    'P': [0b111,0b101,0b111,0b100,0b100],
    'Q': [0b111,0b101,0b101,0b110,0b111],
    'R': [0b110,0b101,0b110,0b110,0b101],
    'S': [0b111,0b100,0b111,0b001,0b111],
    'T': [0b111,0b010,0b010,0b010,0b010],
    'U': [0b101,0b101,0b101,0b101,0b111],
    'V': [0b101,0b101,0b101,0b111,0b010],
    'W': [0b101,0b101,0b111,0b111,0b101],
    'X': [0b101,0b101,0b010,0b101,0b101],
    'Y': [0b101,0b101,0b111,0b010,0b010],
    'Z': [0b111,0b001,0b010,0b100,0b111],
    'Ä': [0b010,0b111,0b101,0b111,0b101],
    'Ö': [0b010,0b111,0b101,0b101,0b111],
    'Ü': [0b010,0b101,0b101,0b101,0b111],
    ' ': [0b000,0b000,0b000,0b000,0b000],
}

# ══════════════════════════════════════════════════════════════════
# Rendering
# ══════════════════════════════════════════════════════════════════
SIZE = 64

def make_buf(color=(0,0,0)):
    return [[[*color] for _ in range(SIZE)] for _ in range(SIZE)]

def draw_char(buf, cx, cy, char, color):
    glyph = FONT.get(char.upper(), FONT[' '])
    for ry, row in enumerate(glyph):
        for rx in range(3):
            if row & (0b100 >> rx):
                px, py = cx + rx, cy + ry
                if 0 <= px < SIZE and 0 <= py < SIZE:
                    buf[py][px] = list(color)

def build_frame(hour, minute, fg, dim, bg, dot):
    buf = make_buf(bg)
    active = get_active_words(hour, minute)

    # Aktive Rasterzellen
    active_cells = set()
    for key in active:
        for (row, col, length) in WORDS.get(key, []):
            for c in range(col, col + length):
                active_cells.add((row, c))

    # Layout: 11 × 10 Zeichen auf 64 × 64 Pixel
    # Zeichenzelle: 3px Glyph + 2px Abstand = 5px/Zeichen
    # 11 × 5 - 2(letzter Abstand) = 53px breit → Rand = (64-53)//2 = 5px
    # 10 × 6 - 1                  = 59px hoch  → Rand = (64-59)//2 = 2px
    CELL_W, CELL_H = 5, 6
    off_x = (SIZE - (COLS * CELL_W - 2)) // 2  # 5
    off_y = (SIZE - (ROWS * CELL_H - 1)) // 2  # 2

    for row in range(ROWS):
        for col in range(COLS):
            char  = GRID[row][col]
            cx    = off_x + col * CELL_W
            cy    = off_y + row * CELL_H
            color = fg if (row, col) in active_cells else dim
            draw_char(buf, cx, cy, char, color)

    # Minuten-Eckpunkte
    for d in range(minute % 5):
        py, px = MINUTE_DOTS[d]
        buf[py][px] = list(dot)

    return buf

def buf_to_b64(buf):
    raw = bytearray()
    for row in buf:
        for px in row:
            raw.extend(px)
    return base64.b64encode(raw).decode()

# ══════════════════════════════════════════════════════════════════
# Pixoo64 HTTP-API
# ══════════════════════════════════════════════════════════════════
class Pixoo64:
    def __init__(self, ip, port=80):
        self.base = f"http://{ip}:{port}"
        self.s = requests.Session()

    def _post(self, cmd, params=None):
        body = {"Command": cmd}
        if params:
            body.update(params)
        r = self.s.post(f"{self.base}/post", json=body, timeout=5)
        r.raise_for_status()
        return r.json()

    def set_brightness(self, v):
        self._post("Channel/SetBrightness", {"Brightness": v})

    def send_frame(self, buf):
        self._post("Draw/ResetHttpGifId")
        self._post("Draw/SendHttpGif", {
            "PicNum":    1,
            "PicWidth":  SIZE,
            "PicOffset": 0,
            "PicID":     1,
            "PicSpeed":  1000,
            "PicData":   buf_to_b64(buf),
        })

# ══════════════════════════════════════════════════════════════════
# Hauptprogramm
# ══════════════════════════════════════════════════════════════════
def main():
    p = argparse.ArgumentParser(description="Deutsche Wordclock für Pixoo64 (Raster-Stil)")
    p.add_argument("--ip",         required=True)
    p.add_argument("--port",       type=int, default=80)
    p.add_argument("--fg",         type=int, nargs=3, default=[255,255,255],
                   metavar=("R","G","B"), help="Aktive Buchstaben   (Standard: weiß)")
    p.add_argument("--dim",        type=int, nargs=3, default=[40,40,60],
                   metavar=("R","G","B"), help="Inaktive Buchstaben (Standard: dunkelblau)")
    p.add_argument("--bg",         type=int, nargs=3, default=[0,0,0],
                   metavar=("R","G","B"), help="Hintergrund         (Standard: schwarz)")
    p.add_argument("--dot",        type=int, nargs=3, default=[255,200,0],
                   metavar=("R","G","B"), help="Minuten-Punkte      (Standard: gelb)")
    p.add_argument("--brightness", type=int, default=80)
    p.add_argument("--interval",   type=int, default=30,
                   help="Prüfintervall in Sekunden (Dauerbetrieb)")
    p.add_argument("--once",       action="store_true",
                   help="Einmal senden und sofort beenden")
    args = p.parse_args()

    fg  = tuple(args.fg)
    dim = tuple(args.dim)
    bg  = tuple(args.bg)
    dot = tuple(args.dot)

    pixoo = Pixoo64(args.ip, args.port)
    try:
        pixoo.set_brightness(args.brightness)
    except Exception as e:
        print(f"Warnung – Helligkeit: {e}")

    def push(h, m):
        words = get_active_words(h, m)
        print(f"[{h:02d}:{m:02d}] → {' | '.join(words)}")
        buf = build_frame(h, m, fg, dim, bg, dot)
        pixoo.send_frame(buf)
        print("  ✓ übertragen")

    if args.once:
        now = datetime.now()
        push(now.hour, now.minute)
        return

    print("Wordclock läuft – Strg+C zum Beenden.\n")
    last_m5 = -1
    while True:
        now = datetime.now()
        m5  = (now.minute // 5) * 5
        if m5 != last_m5:
            try:
                push(now.hour, now.minute)
            except requests.exceptions.ConnectionError:
                print("  ✗ Verbindung fehlgeschlagen")
            except Exception as e:
                print(f"  ✗ {e}")
            last_m5 = m5
        time.sleep(args.interval)

if __name__ == "__main__":
    main()
