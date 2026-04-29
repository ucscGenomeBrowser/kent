#!/usr/bin/env python3
"""
TP53 VCEP cancerhotspots.org subtrack generator (PM1 Evidence composite).

Fetches cancerhotspots.org single-residue hotspots, filters to TP53, and
emits a per-AA-change bigBed 9+4 with ACMG PM1 strength assignment:

    PM1_Moderate   (+2 pts)  >=10 somatic occurrences of this exact aa change
    PM1_Supporting (+1 pt)   2-9 occurrences

Synonymous (WT->WT) and stop-gain (*) variants are excluded; only missense.
Live pull with a cached-snapshot fallback.
"""

import argparse
import json
import os
import sys
import urllib.request

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tp53FuncLib as lib

DEFAULT_OUTDIR = "/hive/users/lrnassar/claude/RM37399/cancerHotspots"
SNAPSHOT_NAME = "cancerhotspots_single.json"
API_URL = "https://www.cancerhotspots.org/api/hotspots/single"

COLOR_MOD = "230,3,131"        # fuchsia &#8212; PM1_Moderate
COLOR_SUP = "245,182,207"      # light fuchsia &#8212; PM1_Supporting

AUTOSQL = """table TP53CancerHotspots
"TP53 somatic hotspots from cancerhotspots.org mapped to ACMG PM1 strength"
   (
   string chrom;        "Reference sequence chromosome or scaffold"
   uint   chromStart;   "Start position in chromosome"
   uint   chromEnd;     "End position in chromosome"
   string name;         "Amino acid change (e.g. R175H)"
   uint   score;        "Not used, all 0"
   char[1] strand;      "Not used, all ."
   uint   thickStart;   "Same as chromStart"
   uint   thickEnd;     "Same as chromEnd"
   uint   reserved;     "RGB color"
   string acmgCode;     "PM1_Moderate or PM1_Supporting"
   string points;       "Tavtigian points contribution"
   uint   somaticCount; "Somatic occurrence count (cancerhotspots.org)"
   lstring _mouseOver;  "HTML mouseover"
   )
"""


def fetch_or_load(outdir):
    path = os.path.join(outdir, SNAPSHOT_NAME)
    try:
        req = urllib.request.Request(API_URL, headers={'User-Agent': 'UCSC-kent/TP53-hub'})
        with urllib.request.urlopen(req, timeout=30) as resp:
            body = resp.read()
        data = json.loads(body)
        # Write snapshot on success
        with open(path, 'wb') as f:
            f.write(body)
        print("  fetched {} records from cancerhotspots.org".format(len(data)))
        return data
    except Exception as e:
        if os.path.exists(path):
            print("  WARNING: live fetch failed ({}); falling back to snapshot {}".format(e, path))
            with open(path) as f:
                return json.load(f)
        raise RuntimeError("cancerhotspots fetch failed and no snapshot: {}".format(e))


def records_for_tp53(data):
    out = []
    for h in data:
        if h.get('hugoSymbol') != 'TP53':
            continue
        res = h.get('residue', '')
        if not res:
            continue
        # residue is like "R175"; first char = WT aa, rest = codon position.
        # cancerhotspots.org also reports splice-site somatic hotspots where
        # residue starts with 'X' and variants contain 'sp'. Those are NOT
        # missense and must not be classified PM1 (CSpec &#167;PM1 is missense only).
        wt = res[0]
        if wt == 'X' or not wt.isalpha():
            continue
        try:
            codon = int(res[1:])
        except ValueError:
            continue
        variants = h.get('variantAminoAcid') or {}
        for alt, count in variants.items():
            if alt == wt:         # synonymous &#8212; not missense
                continue
            if alt in ('*', 'X'): # stop-gain &#8212; handled by PVS1, not PM1
                continue
            if not alt.isalpha() or len(alt) != 1:
                # splice-site codes ("sp", "fs", etc.) are not missense
                continue
            if count is None or count < 2:
                continue
            if count >= 10:
                acmg, points, color = "PM1_Moderate", "+2", COLOR_MOD
            else:
                acmg, points, color = "PM1_Supporting", "+1", COLOR_SUP
            out.append({
                'wt': wt,
                'codon': codon,
                'alt': alt,
                'count': count,
                'acmg': acmg,
                'points': points,
                'color': color,
                'name': "{}{}{}".format(wt, codon, alt),
            })
    return out


def mouseover(rec):
    return (
        "<b>{acmg}</b> ({points} pts)"
        "<br><b>Variant:</b> {name} (TP53 NP_000537.3)"
        "<br><b>Somatic occurrences (cancerhotspots.org):</b> {count}"
        "<br><b>Codon:</b> {codon}"
        "<br><b>Source:</b> cancerhotspots.org &#8212; PM1 per CSpec GN009 v2.4.0"
    ).format(**rec)


def generate_bed(records, tx):
    lines = []
    chrom = tx['chrom']
    for rec in records:
        segs = lib.aa_codon_genomic(rec['codon'], tx)
        if not segs:
            continue
        for g_start, g_end, _ex in segs:
            lines.append("\t".join([
                chrom, str(g_start), str(g_end),
                rec['name'], "0", ".",
                str(g_start), str(g_end),
                rec['color'],
                rec['acmg'], rec['points'], str(rec['count']),
                mouseover(rec),
            ]))
    return lines


def build(db, outdir):
    print("=== {} ===".format(db))
    os.makedirs(outdir, exist_ok=True)
    data = fetch_or_load(outdir)
    records = records_for_tp53(data)
    print("  {} TP53 PM1 records".format(len(records)))
    tx = lib.get_transcript_info(db)
    bed_lines = generate_bed(records, tx)
    print("  {} BED rows".format(len(bed_lines)))

    as_file = os.path.join(outdir, "TP53CancerHotspots.as")
    lib.write_autosql(as_file, AUTOSQL)
    bed = os.path.join(outdir, "TP53CancerHotspots_{}.bed".format(db))
    with open(bed, 'w') as f:
        f.write("\n".join(bed_lines) + "\n")
    lib.bash("sort -k1,1 -k2,2n {0} -o {0}".format(bed))
    bb = os.path.join(outdir, "TP53CancerHotspots{}.bb".format(db.capitalize()))
    lib.run_bedToBigBed(bed, as_file, bb, lib.chrom_sizes_path(db), "bed9+4")
    print("  wrote {}".format(bb))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('-o', '--output-dir', default=DEFAULT_OUTDIR)
    p.add_argument('--db', action='append', help='hg38 or hg19 (repeat). Default hg38.')
    args = p.parse_args()
    dbs = args.db if args.db else ['hg38']
    for db in dbs:
        build(db, args.output_dir)


if __name__ == "__main__":
    main()
