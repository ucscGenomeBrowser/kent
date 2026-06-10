#!/usr/bin/env python3
# Generate a solved game of Mastermind as a BED file for a bigLolly track.
#
# The lolly track puts the score field on the vertical axis and the genomic
# position on the horizontal axis, colors each circle by itemRgb, and scales
# the radius by the size field.  We abuse all three: score = board row,
# position = peg column, color = peg color, size = code peg vs. key peg.
#
# Layout (looking at the browser image):
#   - one horizontal row of circles per guess, oldest guess at the bottom
#   - the secret code sits on the top row, above a one-row gap
#   - four large code pegs on the left, then the small black/white key pegs
#
# The guesses are produced by a simple "first consistent candidate" solver, so
# the black/white feedback pegs on every row are the real feedback for that
# guess against the secret.

import itertools

CHROM = "chr1"
BASE = 1000000          # board sits at chr1:1,000,000

# Classic six Mastermind peg colors, name -> R,G,B
COLORS = [
    ("red",    "220,30,30"),
    ("orange", "240,140,0"),
    ("yellow", "245,215,20"),
    ("green",  "30,170,70"),
    ("blue",   "40,90,200"),
    ("purple", "150,60,180"),
]
NCOLORS = len(COLORS)
SLOTS = 4               # pegs per code

BLACK = "0,0,0"         # right color and position
# "white" feedback peg; a medium gray so it stays visible on the white
# browser background (a true white peg would vanish, only its outline showing)
WHITE = "170,170,170"

# genomic column for each peg (relative to BASE).  Kept compact so the whole
# board fits inside one browser image without the right columns clipping off.
CODE_COLS = [20, 50, 80, 110]             # the four code pegs
KEY_COLS = [130, 142, 154, 166]           # up to four feedback key pegs

CODE_SIZE = 2           # radius scale for code pegs
KEY_SIZE = 1            # radius scale for the small key pegs

SECRET = (0, 2, 0, 5)   # red, yellow, red, purple


def score(guess, secret):
    """Return (black, white): exact matches, and color-only matches."""
    black = sum(g == s for g, s in zip(guess, secret))
    white = 0
    for c in range(NCOLORS):
        white += min(guess.count(c), secret.count(c))
    white -= black
    return black, white


def solve(secret):
    """Return a list of (guess, black, white) using consistent guessing."""
    candidates = list(itertools.product(range(NCOLORS), repeat=SLOTS))
    guess = (0, 0, 1, 1)   # a reasonable opening guess
    history = []
    while True:
        b, w = score(guess, secret)
        history.append((guess, b, w))
        if b == SLOTS:
            return history
        candidates = [c for c in candidates if score(guess, c) == (b, w)]
        guess = candidates[0]


def main():
    history = solve(SECRET)
    nGuesses = len(history)
    rows = []   # (relStart, score, colorRgb, size, name, mouseOver)

    # Guesses: row 1 at the bottom, last (winning) guess just below the secret.
    for i, (guess, b, w) in enumerate(history):
        row = i + 1
        for col, peg in enumerate(guess):
            name, rgb = COLORS[peg]
            mo = "Guess %d, position %d: <b>%s</b>" % (i + 1, col + 1, name)
            rows.append((CODE_COLS[col], row, rgb, CODE_SIZE, name, mo))
        # feedback key pegs: blacks first, then whites
        pegs = [(BLACK, "correct color and position")] * b + \
               [(WHITE, "correct color, wrong position")] * w
        for k, (rgb, desc) in enumerate(pegs):
            mo = "Guess %d feedback: %d correct, %d misplaced" % (i + 1, b, w)
            label = "black" if rgb == BLACK else "white"
            rows.append((KEY_COLS[k], row, rgb, KEY_SIZE, label, mo))

    # Secret code on top, one empty row of gap above the winning guess.
    secretRow = nGuesses + 2
    for col, peg in enumerate(SECRET):
        name, rgb = COLORS[peg]
        mo = "Secret code, position %d: <b>%s</b>" % (col + 1, name)
        rows.append((CODE_COLS[col], secretRow, rgb, CODE_SIZE, name, mo))

    rows.sort(key=lambda r: (r[1], r[0]))
    with open("masterMindGame.bed", "w") as f:
        for relStart, sc, rgb, size, name, mo in rows:
            start = BASE + relStart
            f.write("\t".join([
                CHROM, str(start), str(start + 1), name, str(sc),
                ".", str(start), str(start), rgb, str(size), mo]) + "\n")

    print("secret: %s" % " ".join(COLORS[p][0] for p in SECRET))
    print("solved in %d guesses" % nGuesses)
    print("rows (top score) = %d, suggested viewLimits 0:%d" %
          (secretRow, secretRow + 1))
    print("suggested position: %s:%d-%d" % (CHROM, BASE, BASE + 180))
    print("wrote masterMindGame.bed (%d pegs)" % len(rows))


if __name__ == "__main__":
    main()
