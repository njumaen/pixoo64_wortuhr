#!/usr/bin/env python3
"""
English Wordclock for Pixoo64 – classic grid style
Fixed 11×10 letter grid; active words light up.

pip install requests

Usage:
    python pixoo64_wordclock_en.py --ip 192.168.1.x
    python pixoo64_wordclock_en.py --ip 192.168.1.x --once
    python pixoo64_wordclock_en.py --ip 192.168.1.x --fg 255 220 80 --dim 40 40 60
"""

import argparse
import time
import base64
import requests
from datetime import datetime

# ══════════════════════════════════════════════════════════════════
# Letter grid  10 × 11  (all words verified)
# ══════════════════════════════════════════════════════════════════
GRID = [
    list("ITKISHALFAS"),   # 0  IT IS HALF
    list("TENQUARTERX"),   # 1  TEN(min) QUARTER
    list("TWENTYFIVEW"),   # 2  TWENTY FIVE(min)
    list("MINUTESTOAZ"),   # 3  MINUTES TO
    list("PASTAONETWO"),   # 4  PAST ONE TWO
    list("THREEXFOURX"),   # 5  THREE FOUR
    list("FIVEXSIXZAB"),   # 6  FIVE(h) SIX
    list("SEVENXEIGHT"),   # 7  SEVEN EIGHT
    list("NINEXELEVEN"),   # 8  NINE ELEVEN
    list("TENXTWELVEX"),   # 9  TEN(h) TWELVE
]
ROWS, COLS = 10, 11

# ══════════════════════════════════════════════════════════════════
# Word positions  (row, start_col, length)
# ══════════════════════════════════════════════════════════════════
WORDS = {
    "IT":       [(0, 0, 2)],
    "IS":       [(0, 3, 2)],
    # Minutes
    "HALF":     [(0, 5, 4)],
    "TEN_M":    [(1, 0, 3)],
    "QUARTER":  [(1, 3, 7)],
    "TWENTY":   [(2, 0, 6)],
    "FIVE_M":   [(2, 6, 4)],
    "MINUTES":  [(3, 0, 7)],
    "TO":       [(3, 7, 2)],
    "PAST":     [(4, 0, 4)],
    # Hours
    "ONE":      [(4, 5, 3)],
    "TWO":      [(4, 8, 3)],
    "THREE":    [(5, 0, 5)],
    "FOUR":     [(5, 6, 4)],
    "FIVE_H":   [(6, 0, 4)],
    "SIX":      [(6, 5, 3)],
    "SEVEN":    [(7, 0, 5)],
    "EIGHT":    [(7, 6, 5)],
    "NINE":     [(8, 0, 4)],
    "ELEVEN":   [(8, 5, 6)],
    "TEN_H":    [(9, 0, 3)],
    "TWELVE":   [(9, 4, 6)],
}

# Corner dots for remainder minutes 1–4
MINUTE_DOTS = [(0, 63), (63, 63), (63, 0), (0, 0)]

# ══════════════════════════════════════════════════════════════════
# Wordclock logic
# ══════════════════════════════════════════════════════════════════
HOUR_KEYS = [
    "TWELVE", "ONE", "TWO", "THREE", "FOUR",
    "FIVE_H", "SIX", "SEVEN", "EIGHT", "NINE",
    "TEN_H", "ELEVEN",
]

def get_active_words(hour: int, minute: int) -> list:
    active = ["IT", "IS"]
    m5 = (minute // 5) * 5
    h  = hour % 12
    hn = (h + 1) % 12

    if m5 == 0:
        active += [HOUR_KEYS[h]]
        # no "O'CLOCK" word in grid – just the hour
    elif m5 == 5:
        active += ["FIVE_M", "MINUTES", "PAST", HOUR_KEYS[h]]
    elif m5 == 10:
        active += ["TEN_M", "MINUTES", "PAST", HOUR_KEYS[h]]
    elif m5 == 15:
        active += ["QUARTER", "PAST", HOUR_KEYS[h]]
    elif m5 == 20:
        active += ["TWENTY", "MINUTES", "PAST", HOUR_KEYS[h]]
    elif m5 == 25:
        active += ["TWENTY", "FIVE_M", "MINUTES", "PAST", HOUR_KEYS[h]]
    elif m5 == 30:
        active += ["HALF", "PAST", HOUR_KEYS[h]]
    elif m5 == 35:
        active += ["TWENTY", "FIVE_M", "MINUTES", "TO", HOUR_KEYS[hn]]
    elif m5 == 40:
        active += ["TWENTY", "MINUTES", "TO", HOUR_KEYS[hn]]
    elif m5 == 45:
        active += ["QUARTER", "TO", HOUR_KEYS[hn]]
    elif m5 == 50:
        active += ["TEN_M", "MINUTES", "TO", HOUR_KEYS[hn]]
    elif m5 == 55:
        active += ["FIVE_M", "MINUTES", "TO", HOUR_KEYS[hn]]

    return active

# ══════════════════════════════════════════════════════════════════
# 3×5 pixel font
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

    active_cells = set()
    for key in active:
        for (row, col, length) in WORDS.get(key, []):
            for c in range(col, col + length):
                active_cells.add((row, c))

    # 11×10 chars on 64×64 px
    # cell: 3px glyph + 2px gap = 5px/col, 5px glyph-height + 1px gap = 6px/row
    # 11×5-2 = 53px wide  → off_x = (64-53)//2 = 5
    # 10×6-1 = 59px high  → off_y = (64-59)//2 = 2
    CELL_W, CELL_H = 5, 6
    off_x = (SIZE - (COLS * CELL_W - 2)) // 2
    off_y = (SIZE - (ROWS * CELL_H - 1)) // 2

    for row in range(ROWS):
        for col in range(COLS):
            char  = GRID[row][col]
            cx    = off_x + col * CELL_W
            cy    = off_y + row * CELL_H
            color = fg if (row, col) in active_cells else dim
            draw_char(buf, cx, cy, char, color)

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
# Pixoo64 HTTP API
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
# Main
# ══════════════════════════════════════════════════════════════════
def main():
    p = argparse.ArgumentParser(description="English Wordclock for Pixoo64 (grid style)")
    p.add_argument("--ip",         required=True)
    p.add_argument("--port",       type=int, default=80)
    p.add_argument("--fg",         type=int, nargs=3, default=[255,255,255],
                   metavar=("R","G","B"), help="Active letters    (default: white)")
    p.add_argument("--dim",        type=int, nargs=3, default=[40,40,60],
                   metavar=("R","G","B"), help="Inactive letters  (default: dark blue)")
    p.add_argument("--bg",         type=int, nargs=3, default=[0,0,0],
                   metavar=("R","G","B"), help="Background        (default: black)")
    p.add_argument("--dot",        type=int, nargs=3, default=[255,200,0],
                   metavar=("R","G","B"), help="Minute dots       (default: yellow)")
    p.add_argument("--brightness", type=int, default=80)
    p.add_argument("--interval",   type=int, default=30,
                   help="Poll interval in seconds (continuous mode)")
    p.add_argument("--once",       action="store_true",
                   help="Send once and exit immediately")
    args = p.parse_args()

    fg  = tuple(args.fg)
    dim = tuple(args.dim)
    bg  = tuple(args.bg)
    dot = tuple(args.dot)

    pixoo = Pixoo64(args.ip, args.port)
    try:
        pixoo.set_brightness(args.brightness)
    except Exception as e:
        print(f"Warning – brightness: {e}")

    def push(h, m):
        words = get_active_words(h, m)
        print(f"[{h:02d}:{m:02d}] → {' | '.join(words)}")
        buf = build_frame(h, m, fg, dim, bg, dot)
        pixoo.send_frame(buf)
        print("  ✓ sent")

    if args.once:
        now = datetime.now()
        push(now.hour, now.minute)
        return

    print("Wordclock running – Ctrl+C to stop.\n")
    last_m5 = -1
    while True:
        now = datetime.now()
        m5  = (now.minute // 5) * 5
        if m5 != last_m5:
            try:
                push(now.hour, now.minute)
            except requests.exceptions.ConnectionError:
                print("  ✗ Connection failed")
            except Exception as e:
                print(f"  ✗ {e}")
            last_m5 = m5
        time.sleep(args.interval)

if __name__ == "__main__":
    main()
