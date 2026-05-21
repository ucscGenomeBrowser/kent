#!/usr/bin/env python3
"""
InSiGHT VCEP PMS2 Pseudogene Caution Regions Track Generator

This script generates UCSC Genome Browser tracks (bigBed format) flagging
PMS2 exons that share high sequence homology with the PMS2CL pseudogene
(chr7), and the PMS2CL pseudogene region itself. Variants called in these
regions by short-read NGS may be pseudogene-derived and benefit from
additional orthogonal confirmation.

Per-exon homology data is drawn from the literature (Hayward 2007,
van der Klift 2010, Vaughn 2011, Clendenning 2006).

Exon coordinates are queried from hgsql ncbiRefSeq for both hg38 and hg19.
PMS2CL coordinates are also queried from ncbiRefSeq.

Author: Generated for InSiGHT VCEP
Date: 2026
"""

import subprocess
import os

def bash(cmd):
    """Run the cmd in bash subprocess"""
    try:
        rawBashOutput = subprocess.run(cmd, check=True, shell=True,
                                       stdout=subprocess.PIPE, universal_newlines=True, stderr=subprocess.STDOUT)
        bashStdout = rawBashOutput.stdout
    except subprocess.CalledProcessError as e:
        raise RuntimeError("command '{}' return with error (code {}): {}".format(e.cmd, e.returncode, e.output))
    return(bashStdout)

# ============================================================================
# Configuration
# ============================================================================
OUTPUT_DIR = "/hive/users/lrnassar/insightHub/pms2Caution"

PMS2_ACC = "NM_000535.7"
PMS2CL_ACC = "NR_002217.1"

# Per-exon caution data for PMS2 (15 exons, minus strand, so exon 1 has the
# highest genomic coordinate). Codon ranges are approximate (boundaries fall
# between codons). Homology percentages are from the literature.
# Format: (exon_num, codon_start, codon_end, homology_text, caution_level, color)
PMS2_EXONS = [
    (1,  None, None, None,    'Safe', '0,170,0'),
    (2,  None, None, None,    'Safe', '0,170,0'),
    (3,  None, None, None,    'Safe', '0,170,0'),
    (4,  None, None, None,    'Safe', '0,170,0'),
    (5,  None, None, None,    'Safe', '0,170,0'),
    (6,  None, None, None,    'Safe', '0,170,0'),
    (7,  None, None, None,    'Safe', '0,170,0'),
    (8,  None, None, None,    'Safe', '0,170,0'),
    (9,  302,  329,  '~98%',  'Moderate', '230,150,0'),
    (10, 330,  381,  None,    'Safe',     '0,170,0'),
    (11, 382,  668,  '~99%',  'High',     '210,0,0'),
    (12, 669,  724,  '>99%',  'High',     '210,0,0'),
    (13, 725,  758,  '>99%',  'High',     '210,0,0'),
    (14, 759,  815,  '>99%',  'High',     '210,0,0'),
    (15, 816,  862,  '>99%',  'High',     '210,0,0'),
]

# Recommendation text by caution level
RECOMMENDATIONS = {
    'High':     'Variants called by short-read NGS in this region may be pseudogene-derived and may require additional orthogonal validation (e.g., long-range PCR).',
    'Moderate': 'This region shares moderate homology with PMS2CL; short-read NGS calls may benefit from additional orthogonal validation.',
    'Safe':     'No homology to PMS2CL; standard NGS calls are reliable in this region.',
}

# Pseudogene description
PMS2CL_DESC = ('PMS2CL pseudogene; not a coding copy of PMS2. Variants assigned here '
               'are not pathogenic for Lynch syndrome.')

# AutoSQL definition for BED9+5 format
AUTOSQL = """table InSiGHTPMS2Caution
"PMS2 pseudogene caution regions: PMS2 exons with PMS2CL homology, plus the PMS2CL pseudogene"
   (
   string chrom;          "Reference sequence chromosome or scaffold"
   uint   chromStart;     "Start position in chromosome"
   uint   chromEnd;       "End position in chromosome"
   string name;           "Region name"
   uint   score;          "Not used, all 0"
   char[1] strand;        "Not used, all ."
   uint   thickStart;     "Same as chromStart"
   uint   thickEnd;       "Same as chromEnd"
   uint   reserved;       "RGB value (use R,G,B string in input file)"
   string region;         "PMS2 exon number or PMS2CL pseudogene"
   string homology;       "Sequence identity to PMS2CL"
   string codonRange;     "Approximate amino acid range in PMS2"
   string cautionLevel;   "High / Moderate / Safe / Pseudogene"
   lstring _mouseOver;    "Field only used as mouseOver"
   )
"""

# ============================================================================
# Functions
# ============================================================================

def get_pms2_exons(db):
    """Query hgsql for PMS2 NM_000535.7 exon coordinates.
    Returns list of (exon_num, genomic_start, genomic_end) in transcript order
    (i.e., reversed for minus strand)."""
    result = bash(f'hgsql {db} -Ne \'select chrom, txStart, exonStarts, exonEnds, strand from ncbiRefSeq where name="{PMS2_ACC}"\'')
    fields = result.strip().split('\t')
    chrom = fields[0]
    starts = [int(x) for x in fields[2].rstrip(',').split(',')]
    ends = [int(x) for x in fields[3].rstrip(',').split(',')]
    strand = fields[4]
    exons_genomic = list(zip(starts, ends))
    # For minus strand, reverse so exon 1 is first in the list
    if strand == '-':
        exons_genomic = exons_genomic[::-1]
    # Return (exon_num, start, end) where exon_num is 1-based transcript order
    return chrom, [(i+1, s, e) for i, (s, e) in enumerate(exons_genomic)]


def get_pms2cl_region(db):
    """Query hgsql for PMS2CL coordinates."""
    result = bash(f'hgsql {db} -Ne \'select chrom, txStart, txEnd from ncbiRefSeq where name="{PMS2CL_ACC}"\'')
    fields = result.strip().split('\t')
    return fields[0], int(fields[1]), int(fields[2])


def html_escape_rule(text):
    """HTML-encode special characters for mouseover."""
    return text.replace('≥', '&ge;').replace('≤', '&le;').replace('>', '&gt;').replace('<', '&lt;')


def generate_bed_entries(db):
    """Generate BED entries for PMS2 exons and PMS2CL pseudogene."""
    bed_lines = []

    # PMS2 exons
    pms2_chrom, pms2_exons = get_pms2_exons(db)
    for (exon_num, _, _, _, _, _), (_, ex_start, ex_end) in zip(PMS2_EXONS, pms2_exons):
        meta = PMS2_EXONS[exon_num - 1]
        _, codon_start, codon_end, homology, caution, color = meta

        name = f"PMS2 exon {exon_num}"
        if codon_start is not None:
            codon_range = f"codons {codon_start}-{codon_end}"
        else:
            codon_range = ""
        homology_str = homology if homology else "none"
        rec = RECOMMENDATIONS[caution]

        mouse_over = (f"<b>Region:</b> PMS2 exon {exon_num}</br>"
                      f"<b>Homology to PMS2CL:</b> {html_escape_rule(homology_str)}</br>"
                      f"<b>Caution:</b> {caution}</br>"
                      f"<b>Note:</b> {html_escape_rule(rec)}")

        bed_line = (f"{pms2_chrom}\t{ex_start}\t{ex_end}\tPMS2 exon {exon_num}\t0\t.\t"
                    f"{ex_start}\t{ex_end}\t{color}\t"
                    f"exon {exon_num}\t{homology_str}\t{codon_range}\t{caution}\t{mouse_over}")
        bed_lines.append(bed_line)

    # PMS2CL pseudogene
    psg_chrom, psg_start, psg_end = get_pms2cl_region(db)
    psg_color = '128,128,128'
    mouse_over = (f"<b>Region:</b> PMS2CL pseudogene</br>"
                  f"<b>Note:</b> {html_escape_rule(PMS2CL_DESC)}")
    bed_line = (f"{psg_chrom}\t{psg_start}\t{psg_end}\tPMS2CL pseudogene\t0\t.\t"
                f"{psg_start}\t{psg_end}\t{psg_color}\t"
                f"PMS2CL pseudogene\tn/a\t\tPseudogene\t{mouse_over}")
    bed_lines.append(bed_line)

    return bed_lines


def create_track(db):
    """Create BED and bigBed files for a given genome assembly."""
    print(f"\n{'='*70}")
    print(f"Processing {db}")
    print(f"{'='*70}")

    bed_lines = generate_bed_entries(db)
    print(f"  Generated {len(bed_lines)} entries")

    bed_file = os.path.join(OUTPUT_DIR, f"InSiGHTPMS2Caution_{db}.bed")
    print(f"  Writing BED file: {bed_file}")
    with open(bed_file, 'w') as f:
        for line in bed_lines:
            f.write(line + '\n')

    sorted_bed = bed_file.replace('.bed', '.sorted.bed')
    bash(f"sort -k1,1 -k2,2n {bed_file} > {sorted_bed}")

    as_file = os.path.join(OUTPUT_DIR, "InSiGHTPMS2Caution.as")
    bb_file = os.path.join(OUTPUT_DIR, f"InSiGHTPMS2Caution{db.capitalize()}.bb")
    chrom_sizes = f"/cluster/data/{db}/chrom.sizes"

    bash(f"bedToBigBed -type=bed9+5 -as={as_file} -tab {sorted_bed} {chrom_sizes} {bb_file}")
    print(f"  Successfully created: {bb_file}")

    os.remove(sorted_bed)
    return bb_file


def main():
    print("=" * 70)
    print("InSiGHT VCEP PMS2 Pseudogene Caution Regions Track Generator")
    print("=" * 70)

    os.makedirs(OUTPUT_DIR, exist_ok=True)

    as_file = os.path.join(OUTPUT_DIR, "InSiGHTPMS2Caution.as")
    print(f"\nWriting AutoSql file: {as_file}")
    with open(as_file, 'w') as f:
        f.write(AUTOSQL)

    for db in ['hg38', 'hg19']:
        create_track(db)

    print("\n" + "=" * 70)
    print("Done!")
    print("=" * 70)


if __name__ == '__main__':
    main()
