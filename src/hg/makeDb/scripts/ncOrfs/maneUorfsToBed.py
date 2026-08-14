#!/usr/bin/env python3
"""
Convert 5ULTRA uORFs BED9 to bed9+1 for addIntrons.py input.
Colors features by Kozak strength (matching the palette used by all other
ncOrfs subtracks), shortens the name field to GENE_N (e.g. BRCA2_1), and
appends uorfType as field 10.

Input:  BED9 with name format GENE|Type|Rank|Kozak (may have a track header)
        Rank format is POSITION_TOTAL (e.g. 2_5).
        Score encodes Kozak: 1000=Strong, 700=Moderate, 300=Weak, 100=no context.
Output: BED9+1  (name shortened, itemRgb set by Kozak, uorfType appended)
"""
import sys

# Kozak colors matching the palette used by all other ncOrfs subtracks
KOZAK_COLOR = {
    1000: "245,166,35",   # Strong      #F5A623
    700:  "91,155,213",   # Moderate    #5B9BD5
    300:  "169,169,169",  # Weak        #A9A9A9
    100:  "211,211,211",  # no context  #D3D3D3
}

for line in sys.stdin:
    if line.startswith("track "):
        continue
    fields = line.rstrip("\n").split("\t")
    if len(fields) < 9:
        continue
    orig_name = fields[3]
    parts = orig_name.split("|")
    gene      = parts[0] if len(parts) > 0 else orig_name
    uorf_type = parts[1] if len(parts) > 1 else "Unknown"
    rank_str  = parts[2] if len(parts) > 2 else "1"
    position  = rank_str.split("_")[0]          # e.g. "2" from "2_5"
    fields[3] = f"{gene}_{position}"            # short display name
    score     = int(fields[4])
    fields[8] = KOZAK_COLOR.get(score, "169,169,169")
    fields.append(uorf_type)
    print("\t".join(fields))
