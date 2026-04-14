#!/usr/bin/env python3
"""
InSiGHT VCEP PVS1 Decision Track Generator

This script generates UCSC Genome Browser tracks (bigBed format) for the
InSiGHT Variant Curation Expert Panel (VCEP) PVS1 decision regions
for Lynch syndrome genes: MLH1, MSH2, MSH6, and PMS2.

The track shows regions where truncating variants receive different ACMG
classifications (PVS1, PVS1_Moderate, or PVS1_n.a.) based on their position.

Coordinates are computed dynamically from codon positions by querying
NCBI RefSeq transcript coordinates from hgsql for both hg38 and hg19.

Transcripts used:
  MLH1: NM_000249.4  (chr3, + strand)
  MSH2: NM_000251.3  (chr2, + strand)
  MSH6: NM_000179.3  (chr2, + strand)
  PMS2: NM_000535.7  (chr7, - strand)

Author: Generated for InSiGHT VCEP
Date: 2025
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
OUTPUT_DIR = "/hive/users/lrnassar/insightHub/pvs1"

# Transcripts to query
TRANSCRIPTS = {
    'MLH1': 'NM_000249.4',
    'MSH2': 'NM_000251.3',
    'MSH6': 'NM_000179.3',
    'PMS2': 'NM_000535.7',
}

# PVS1 regions defined by codon ranges
# Format: (gene, name, aa_start, aa_end, color, rule, acmg_code)
# aa_start/aa_end are 1-based codon positions (inclusive)
# Use None for aa_end to indicate "to end of protein"
PVS1_REGIONS = [
    # MLH1
    ('MLH1', 'NMD', 1, 684, '180,0,0', '≤ codon 684', 'PVS1'),
    ('MLH1', 'CritRegion', 685, 753, '180,0,0', '> codon 684 ≤ codon 753', 'PVS1'),
    ('MLH1', 'FuncUnknown', 754, 756, '204,102,0', 'Codons 754, 755, 756', 'PVS1_Moderate'),
    ('MLH1', 'PVS1_n.a.', 757, None, '128,128,128', '> codon 756', 'PVS1_n.a.'),

    # MSH2
    ('MSH2', 'NMD', 1, 861, '180,0,0', '≤ codon 861', 'PVS1'),
    ('MSH2', 'CritRegion', 862, 891, '180,0,0', '> codon 861 ≤ codon 891', 'PVS1'),
    ('MSH2', 'FuncUnknown', 892, 934, '204,102,0', '> codon 891 ≤ codon 934', 'PVS1_Moderate'),
    ('MSH2', 'PVS1_n.a.', 935, None, '128,128,128', '> codon 934', 'PVS1_n.a.'),

    # MSH6
    ('MSH6', 'NMD', 1, 1317, '180,0,0', '≤ codon 1317', 'PVS1'),
    ('MSH6', 'CritRegion', 1318, 1341, '180,0,0', '> codon 1317 ≤ codon 1341', 'PVS1'),
    ('MSH6', 'FuncUnknown', 1342, 1360, '204,102,0', '> codon 1341 ≤ codon 1360', 'PVS1_Moderate'),
    ('MSH6', 'PVS1_n.a.', 1361, None, '128,128,128', '> codon 1360', 'PVS1_n.a.'),

    # PMS2
    ('PMS2', 'NMD', 1, 798, '180,0,0', '≤ codon 798', 'PVS1'),
    ('PMS2', 'PVS1_n.a.', 799, None, '128,128,128', '> codon 798', 'PVS1_n.a.'),
]

# AutoSQL definition for the BED9+3 format
AUTOSQL = """table InSiGHTPVS1
"InSiGHT VCEP PVS1 decision regions for Lynch syndrome genes (MLH1, MSH2, MSH6, PMS2)"
   (
   string chrom;       "Reference sequence chromosome or scaffold"
   uint   chromStart;  "Start position in chromosome"
   uint   chromEnd;    "End position in chromosome"
   string name;        "Region name (NMD, CritRegion, FuncUnknown, PVS1_n.a.)"
   uint score;         "Not used, all 0"
   char[1] strand;     "Not used, all ."
   uint thickStart;    "Same as chromStart"
   uint thickEnd;      "Same as chromEnd"
   uint reserved;      "RGB value (use R,G,B string in input file)"
   string rule;        "Codon position rule"
   string acmgCode;    "ACMG classification code"
   string _mouseOver;  "Field only used as mouseOver"
   )"""

# ============================================================================
# Functions for coordinate computation
# ============================================================================

def get_transcript_info(db, accession):
    """Query hgsql to get transcript information from ncbiRefSeq"""
    query = f"SELECT name, chrom, strand, txStart, txEnd, cdsStart, cdsEnd, exonStarts, exonEnds FROM ncbiRefSeq WHERE name='{accession}'"
    result = bash(f'hgsql {db} -Ne "{query}"')

    if not result.strip():
        raise ValueError(f"Transcript {accession} not found in {db}.ncbiRefSeq")

    fields = result.strip().split('\t')

    # Parse exon starts and ends (comma-separated, trailing comma)
    exon_starts = [int(x) for x in fields[7].rstrip(',').split(',')]
    exon_ends = [int(x) for x in fields[8].rstrip(',').split(',')]

    return {
        'name': fields[0],
        'chrom': fields[1],
        'strand': fields[2],
        'txStart': int(fields[3]),
        'txEnd': int(fields[4]),
        'cdsStart': int(fields[5]),
        'cdsEnd': int(fields[6]),
        'exonStarts': exon_starts,
        'exonEnds': exon_ends,
    }

def build_cds_regions(tx):
    """Build list of CDS regions from transcript info"""
    cds_regions = []
    for i in range(len(tx['exonStarts'])):
        ex_start = tx['exonStarts'][i]
        ex_end = tx['exonEnds'][i]
        # Skip exons outside CDS
        if ex_end <= tx['cdsStart'] or ex_start >= tx['cdsEnd']:
            continue
        # Clip exon to CDS boundaries
        region_start = max(ex_start, tx['cdsStart'])
        region_end = min(ex_end, tx['cdsEnd'])
        cds_regions.append((region_start, region_end, i+1))

    # For minus strand, reverse order (CDS starts from high genomic coords)
    if tx['strand'] == '-':
        cds_regions = cds_regions[::-1]

    return cds_regions

def get_protein_length(cds_regions):
    """Calculate protein length from CDS regions"""
    total_cds_bp = sum(end - start for start, end, _ in cds_regions)
    return total_cds_bp // 3

def aa_to_genomic_plus(aa_start, aa_end, cds_regions):
    """Convert AA range to genomic segments for + strand genes"""
    nt_start = (aa_start - 1) * 3 + 1  # 1-based nucleotide position
    nt_end = aa_end * 3

    segments = []
    cumulative = 0

    for start, end, exon_num in cds_regions:
        region_len = end - start
        region_nt_start = cumulative + 1
        region_nt_end = cumulative + region_len

        if region_nt_end >= nt_start and region_nt_start <= nt_end:
            overlap_nt_start = max(nt_start, region_nt_start)
            overlap_nt_end = min(nt_end, region_nt_end)

            genomic_start = start + (overlap_nt_start - region_nt_start)
            genomic_end = start + (overlap_nt_end - region_nt_start) + 1

            segments.append((genomic_start, genomic_end, exon_num))

        cumulative += region_len

    return segments

def aa_to_genomic_minus(aa_start, aa_end, cds_regions):
    """Convert AA range to genomic segments for - strand genes"""
    nt_start = (aa_start - 1) * 3 + 1
    nt_end = aa_end * 3

    segments = []
    cumulative = 0

    for start, end, exon_num in cds_regions:
        region_len = end - start
        region_nt_start = cumulative + 1
        region_nt_end = cumulative + region_len

        if region_nt_end >= nt_start and region_nt_start <= nt_end:
            overlap_nt_start = max(nt_start, region_nt_start)
            overlap_nt_end = min(nt_end, region_nt_end)

            offset_from_end_start = overlap_nt_start - region_nt_start
            offset_from_end_end = overlap_nt_end - region_nt_start

            genomic_end = end - offset_from_end_start
            genomic_start = end - offset_from_end_end - 1

            if genomic_start > genomic_end:
                genomic_start, genomic_end = genomic_end, genomic_start

            segments.append((genomic_start, genomic_end, exon_num))

        cumulative += region_len

    return segments

def generate_bed_entries(db, transcripts_info):
    """Generate BED entries for all PVS1 regions"""
    bed_lines = []

    for gene, name, aa_start, aa_end, color, rule, acmg_code in PVS1_REGIONS:
        tx = transcripts_info[gene]
        chrom = tx['chrom']
        strand = tx['strand']
        accession = tx['name']

        cds_regions = build_cds_regions(tx)
        protein_length = get_protein_length(cds_regions)

        # If aa_end is None, use protein length
        if aa_end is None:
            aa_end = protein_length

        # Skip if aa_start is beyond protein length
        if aa_start > protein_length:
            print(f"  WARNING: Skipping {gene} {name} - aa_start ({aa_start}) > protein length ({protein_length})")
            continue

        # Clamp aa_end to protein length
        if aa_end > protein_length:
            aa_end = protein_length

        if strand == '+':
            segments = aa_to_genomic_plus(aa_start, aa_end, cds_regions)
        else:
            segments = aa_to_genomic_minus(aa_start, aa_end, cds_regions)

        for seg_start, seg_end, exon_num in segments:
            if seg_start >= seg_end:
                print(f"  WARNING: Skipping invalid segment for {gene} {name}: {seg_start}-{seg_end}")
                continue

            # HTML-encode special characters for mouseOver (UCSC browser can't handle ≤ ≥)
            rule_html = rule.replace('≤', '&le;').replace('≥', '&ge;').replace('>', '&gt;').replace('<', '&lt;')
            mouse_over = f"<b>Name: </b>{name}</br><b>Gene: </b>{gene}</br><b>Rule: </b>{rule_html}</br><b>ACMG Code: </b>{acmg_code}"
            bed_line = f"{chrom}\t{seg_start}\t{seg_end}\t{name}\t0\t.\t{seg_start}\t{seg_end}\t{color}\t{rule}\t{acmg_code}\t{mouse_over}"
            bed_lines.append(bed_line)

    return bed_lines

def create_track(db, output_dir):
    """Create BED and bigBed files for a given genome assembly"""
    print(f"\n{'='*70}")
    print(f"Processing {db}")
    print(f"{'='*70}")

    # Query transcript info from hgsql
    print(f"\nQuerying transcript coordinates from {db}.ncbiRefSeq...")
    transcripts_info = {}
    for gene, accession in TRANSCRIPTS.items():
        tx_info = get_transcript_info(db, accession)
        cds_regions = build_cds_regions(tx_info)
        protein_length = get_protein_length(cds_regions)
        print(f"  {gene}: {accession} (protein length: {protein_length} aa)")
        transcripts_info[gene] = tx_info

    # Generate BED entries
    print("\nGenerating BED entries...")
    bed_lines = generate_bed_entries(db, transcripts_info)
    print(f"  Generated {len(bed_lines)} region segments")

    # Write BED file
    bed_file = os.path.join(output_dir, f"InSiGHTPVS1_{db}.bed")
    print(f"\nWriting BED file: {bed_file}")
    with open(bed_file, 'w') as f:
        f.write('\n'.join(bed_lines) + '\n')

    # Sort BED file
    print("Sorting BED file...")
    bash(f"sort -k1,1 -k2,2n {bed_file} -o {bed_file}")

    # Create bigBed
    as_file = os.path.join(output_dir, "InSiGHTPVS1.as")
    bb_file = os.path.join(output_dir, f"InSiGHTPVS1{db.capitalize()}.bb")
    chrom_sizes = f"/cluster/data/{db}/chrom.sizes"

    print(f"\nCreating bigBed file: {bb_file}")
    try:
        bash(f"bedToBigBed -as={as_file} -type=bed9+3 -tab {bed_file} {chrom_sizes} {bb_file}")
        print(f"  Successfully created: {bb_file}")
    except Exception as e:
        print(f"  ERROR creating bigBed: {e}")

    return bed_file, bb_file

# ============================================================================
# Main execution
# ============================================================================
if __name__ == "__main__":
    print("=" * 70)
    print("InSiGHT VCEP PVS1 Decision Track Generator")
    print("=" * 70)

    # Create output directory if needed
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    # Write AutoSql file
    as_file = os.path.join(OUTPUT_DIR, "InSiGHTPVS1.as")
    print(f"\nWriting AutoSql file: {as_file}")
    with open(as_file, 'w') as f:
        f.write(AUTOSQL)

    # Process both assemblies
    output_files = {}
    for db in ['hg38', 'hg19']:
        try:
            bed_file, bb_file = create_track(db, OUTPUT_DIR)
            output_files[db] = {'bed': bed_file, 'bb': bb_file}
        except Exception as e:
            print(f"\nERROR processing {db}: {e}")

    # Print summary
    print("\n" + "=" * 70)
    print("Output Files")
    print("=" * 70)
    print(f"  AutoSql file: {as_file}")
    for db, files in output_files.items():
        print(f"\n  {db}:")
        print(f"    BED file:    {files['bed']}")
        if os.path.exists(files['bb']):
            print(f"    BigBed file: {files['bb']}")

    print("\n" + "=" * 70)
    print("Custom Track Line (for bigBed)")
    print("=" * 70)
    print("""
  track type=bigBed name="InSiGHT PVS1" \\
    description="InSiGHT VCEP PVS1 Decision Regions" \\
    visibility=pack itemRgb=on \\
    bigDataUrl=<URL_TO_YOUR_BB_FILE>
""")

    print("=" * 70)
    print("Done!")
    print("=" * 70)
