#!/usr/bin/env python3
"""
TP53 VCEP Clinical Domains track generator.

Builds bigBed 9+5 for the seven clinically relevant TP53 protein domains
defined in ClinGen CSpec GN009 v2.4.0 (PM1), plus the six PM1_Moderate
hotspot codons (R175, G245, R248, R249, R273, R282) overlaid on the DBD.

Transcript: NM_000546.6 / NP_000537.3 (MANE Select), 393 aa, chr17 minus strand.
PM1 is applicable for TP53 (unlike the MMR genes in the InSiGHT hub).
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tp53FuncLib as lib

DEFAULT_OUTDIR = "/hive/users/lrnassar/claude/RM37399/clinDomains"

# Domains from CSpec GN009 v2.4.0 &#167;PM1 (aa ranges on NP_000537.3)
DOMAINS = [
    ("TAD1",  17,  25,  "Transactivation domain 1"),
    ("TAD2",  48,  56,  "Transactivation domain 2"),
    ("PRR",   64,  92,  "Proline-rich region"),
    ("DBD",   100, 292, "DNA binding domain"),
    ("Hinge", 293, 324, "Hinge"),
    ("OD",    325, 356, "Oligomerization (tetramerization) domain"),
    ("CTD",   368, 387, "C-terminal (basic / regulatory) domain"),
]

# PM1_Moderate hotspot codons. Tavtigian +2 points each.
HOTSPOT_CODONS = [
    (175, "R175", "DNA-contact / structural hotspot"),
    (245, "G245", "structural hotspot"),
    (248, "R248", "DNA-contact hotspot"),
    (249, "R249", "structural hotspot"),
    (273, "R273", "DNA-contact hotspot"),
    (282, "R282", "DNA-contact hotspot"),
]

DOMAIN_COLOR = "138,111,158"    # purple
HOTSPOT_COLOR = "230,3,131"     # fuchsia

AUTOSQL = """table TP53clinDomains
"TP53 VCEP clinically relevant protein domains and PM1_Moderate hotspot codons (NM_000546.6)"
   (
   string chrom;        "Reference sequence chromosome or scaffold"
   uint   chromStart;   "Start position in chromosome"
   uint   chromEnd;     "End position in chromosome"
   string name;         "Domain name or hotspot residue"
   uint   score;        "Not used, all 0"
   char[1] strand;      "Not used, all ."
   uint   thickStart;   "Same as chromStart"
   uint   thickEnd;     "Same as chromEnd"
   uint   reserved;     "RGB value"
   string domainType;   "Either 'Domain' or 'PM1_Moderate hotspot'"
   string NMaccession;  "Transcript accession (NM_000546.6)"
   string AAlocation;   "Amino acid range"
   string description;  "Description or role"
   lstring _mouseOver;  "HTML mouseover"
   )
"""


def domain_mouseover(name, desc, aa_lo, aa_hi):
    return (
        "<b>Domain:</b> {name} ({desc})"
        "<br><b>Gene:</b> TP53"
        "<br><b>Transcript:</b> {tx} (NP_000537.3)"
        "<br><b>Amino acid loc:</b> {lo}-{hi}"
        "<br><b>Note:</b> used in PVS1 decision tree; PM1 hotspot codons "
        "overlaid on DBD contribute +2 pts each"
    ).format(name=name, desc=desc, tx=lib.TRANSCRIPT, lo=aa_lo, hi=aa_hi)


def hotspot_mouseover(label, role, codon):
    return (
        "<b>PM1_Moderate hotspot:</b> {label} (+2 pts)"
        "<br><b>Role:</b> {role}"
        "<br><b>Gene:</b> TP53"
        "<br><b>Transcript:</b> {tx} (NP_000537.3)"
        "<br><b>Codon:</b> {codon}"
        "<br><b>ACMG code:</b> PM1_Moderate"
    ).format(label=label, role=role, tx=lib.TRANSCRIPT, codon=codon)


def generate_bed(tx):
    lines = []
    chrom = tx['chrom']
    for name, aa_lo, aa_hi, desc in DOMAINS:
        mo = domain_mouseover(name, desc, aa_lo, aa_hi)
        for g_start, g_end, _ex in lib.aa_to_genomic(aa_lo, aa_hi, tx):
            if g_start >= g_end:
                continue
            lines.append("\t".join([
                chrom, str(g_start), str(g_end),
                name, "0", ".",
                str(g_start), str(g_end),
                DOMAIN_COLOR,
                "Domain", lib.TRANSCRIPT,
                "{}-{}".format(aa_lo, aa_hi),
                desc,
                mo,
            ]))
    for codon, label, role in HOTSPOT_CODONS:
        mo = hotspot_mouseover(label, role, codon)
        for g_start, g_end, _ex in lib.aa_codon_genomic(codon, tx):
            if g_start >= g_end:
                continue
            lines.append("\t".join([
                chrom, str(g_start), str(g_end),
                label, "0", ".",
                str(g_start), str(g_end),
                HOTSPOT_COLOR,
                "PM1_Moderate hotspot", lib.TRANSCRIPT,
                str(codon),
                role,
                mo,
            ]))
    return lines


def build(db, outdir):
    print("=== {} ===".format(db))
    tx = lib.get_transcript_info(db)
    print("  {} at {}:{}-{} {}".format(
        tx['name'], tx['chrom'], tx['txStart'], tx['txEnd'], tx['strand']))
    bed_lines = generate_bed(tx)
    print("  {} BED rows".format(len(bed_lines)))

    os.makedirs(outdir, exist_ok=True)
    as_file = os.path.join(outdir, "TP53clinDomains.as")
    lib.write_autosql(as_file, AUTOSQL)

    bed = os.path.join(outdir, "TP53clinDomains_{}.bed".format(db))
    with open(bed, 'w') as f:
        f.write("\n".join(bed_lines) + "\n")
    lib.bash("sort -k1,1 -k2,2n {0} -o {0}".format(bed))

    bb = os.path.join(outdir, "TP53clinDomains{}.bb".format(db.capitalize()))
    lib.run_bedToBigBed(bed, as_file, bb, lib.chrom_sizes_path(db), "bed9+5")
    print("  wrote {}".format(bb))


def main():
    import argparse
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('-o', '--output-dir', default=DEFAULT_OUTDIR)
    p.add_argument('--db', action='append',
                   help='Assembly db (hg38 or hg19); repeat for both. Default: hg38 only.')
    args = p.parse_args()
    dbs = args.db if args.db else ['hg38']
    for db in dbs:
        build(db, args.output_dir)


if __name__ == "__main__":
    main()
