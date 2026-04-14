#!/usr/bin/env python3
"""
InSiGHT VCEP HCI Priors Track Generator

This script generates UCSC Genome Browser tracks (bigBed format) for HCI
(Huntsman Cancer Institute) prior probability of pathogenicity data for
Lynch syndrome genes: MLH1, MSH2, MSH6, and PMS2.

Coordinates are computed from HGVSc positions by querying transcript
coordinates from hgsql for both hg38 and hg19.

Author: Generated for InSiGHT VCEP
Date: 2025
"""

import subprocess
import os
import re

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
OUTPUT_DIR = "/hive/users/lrnassar/insightHub/hciPriors"

# Gene to transcript and file mapping
GENES = {
    'MLH1': {
        'transcript': 'NM_000249.4',
        'file': 'LOVD_MLH1_priors_2025-12-11_00.15.01.txt',
    },
    'MSH2': {
        'transcript': 'NM_000251.3',
        'file': 'LOVD_MSH2_priors_2025-12-11_00.15.17.txt',
    },
    'MSH6': {
        'transcript': 'NM_000179.3',
        'file': 'LOVD_MSH6_priors_2025-12-11_00.15.31.txt',
    },
    'PMS2': {
        'transcript': 'NM_000535.7',
        'file': 'LOVD_PMS2_priors_2025-12-11_00.15.47.txt',
    },
}

# ACMG classification thresholds
# PP3_moderate: > 0.81
# PP3_supporting: > 0.68 and <= 0.81
# BP4_supporting: < 0.11
# Variants between 0.11 and 0.68 are excluded

# Colors (RGB)
COLORS = {
    'PP3_moderate': '138,111,158',
    'PP3_supporting': '196,181,209',
    'BP4_supporting': '158,213,200',
}

# Rule text (full version for BED field)
RULES = {
    'PP3_moderate': 'Missense variant with HCI prior probability for pathogenicity >0.81',
    'PP3_supporting': 'Missense variant with HCI prior probability for pathogenicity >0.68 & ≤0.81',
    'BP4_supporting': 'Missense variant with HCI-prior probability of pathogenicity <0.11',
}

# Short rule text for mouseOver (HTML encoded)
RULES_SHORT = {
    'PP3_moderate': 'HCI prior for path &gt;0.81',
    'PP3_supporting': 'HCI prior for path &gt;0.68 &amp; &le;0.81',
    'BP4_supporting': 'HCI prior for path &lt;0.11',
}

# AutoSQL definition for the BED9+5 format
AUTOSQL = """table InSiGHTHCIPriors
"InSiGHT VCEP HCI prior probability classifications for Lynch syndrome genes"
   (
   string chrom;       "Reference sequence chromosome or scaffold"
   uint   chromStart;  "Start position in chromosome"
   uint   chromEnd;    "End position in chromosome"
   string name;        "HGVSc notation"
   uint score;         "Not used, all 0"
   char[1] strand;     "Not used, all ."
   uint thickStart;    "Same as chromStart"
   uint thickEnd;      "Same as chromEnd"
   uint reserved;      "RGB value (use R,G,B string in input file)"
   string hgvsp;       "HGVSp notation (protein change)"
   string prior;       "MAPP/PP2 Prior probability value"
   string acmgCode;    "ACMG classification code"
   string rule;        "Classification rule/criteria"
   string _mouseOver;  "Field only used as mouseOver"
   )"""

# ============================================================================
# Functions
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
        cds_regions.append((region_start, region_end))

    # For minus strand, reverse order (CDS starts from high genomic coords)
    if tx['strand'] == '-':
        cds_regions = cds_regions[::-1]

    return cds_regions

def cds_pos_to_genomic(cds_pos, cds_regions, strand):
    """
    Convert a 1-based CDS position to 0-based genomic coordinate.
    Returns (genomic_start, genomic_end) for the single nucleotide.
    """
    cumulative = 0

    for region_start, region_end in cds_regions:
        region_len = region_end - region_start
        region_cds_start = cumulative + 1
        region_cds_end = cumulative + region_len

        if cds_pos >= region_cds_start and cds_pos <= region_cds_end:
            offset = cds_pos - region_cds_start

            if strand == '+':
                genomic_pos = region_start + offset
            else:
                # Minus strand: count from end of region
                genomic_pos = region_end - offset - 1

            return genomic_pos, genomic_pos + 1

        cumulative += region_len

    return None, None

def parse_hgvsc(hgvsc):
    """
    Parse HGVSc notation to extract the CDS position.
    Returns the position as integer, or None if parsing fails.

    Examples:
    - c.4T>A -> 4
    - c.100G>C -> 100
    """
    # Match patterns like c.4T>A, c.100G>C, etc.
    match = re.match(r'c\.(\d+)[ACGT]>[ACGT]', hgvsc)
    if match:
        return int(match.group(1))
    return None

def classify_variant(prior_value):
    """Classify variant based on prior probability value"""
    try:
        prior = float(prior_value)
    except (ValueError, TypeError):
        return None

    if prior > 0.81:
        return 'PP3_moderate'
    elif prior > 0.68 and prior <= 0.81:
        return 'PP3_supporting'
    elif prior < 0.11:
        return 'BP4_supporting'
    else:
        return None  # Between 0.11 and 0.68 - excluded

def parse_data_file(filepath):
    """Parse LOVD data file and return list of variants"""
    variants = []
    with open(filepath, 'r') as f:
        lines = f.readlines()

    # Skip first 2 lines (header comment and column names)
    for line in lines[2:]:
        fields = line.strip().split('\t')
        if len(fields) < 7:
            continue

        # Remove quotes from fields
        hgvsc = fields[1].strip('"')
        hgvsp = fields[3].strip('"')
        prior = fields[6].strip('"')

        variants.append({
            'hgvsc': hgvsc,
            'hgvsp': hgvsp,
            'prior': prior,
        })

    return variants

def process_gene(gene, gene_info, db, stats):
    """Process all variants for a gene and assembly"""
    transcript = gene_info['transcript']
    filepath = os.path.join(OUTPUT_DIR, gene_info['file'])

    # Get transcript info
    tx = get_transcript_info(db, transcript)
    cds_regions = build_cds_regions(tx)
    chrom = tx['chrom']
    strand = tx['strand']

    # Parse data file
    variants = parse_data_file(filepath)

    bed_entries = []
    for var in variants:
        hgvsc = var['hgvsc']
        hgvsp = var['hgvsp']
        prior = var['prior']

        # Classify variant
        acmg_code = classify_variant(prior)
        if acmg_code is None:
            stats['excluded_middle'] += 1
            continue

        # Parse HGVSc to get CDS position
        cds_pos = parse_hgvsc(hgvsc)
        if cds_pos is None:
            stats['parse_failed'] += 1
            continue

        # Convert to genomic coordinates
        genomic_start, genomic_end = cds_pos_to_genomic(cds_pos, cds_regions, strand)
        if genomic_start is None:
            stats['coord_failed'] += 1
            continue

        # Build full HGVSc with transcript
        full_hgvsc = f"{transcript}:{hgvsc}"

        # Get color and rule
        color = COLORS[acmg_code]
        rule = RULES[acmg_code]
        rule_short = RULES_SHORT[acmg_code]

        # Build mouseOver (HTML)
        mouse_over = f"<b>HGVSc:</b> {full_hgvsc}</br><b>HGVSp:</b> {hgvsp}</br><b>ACMG code:</b> {acmg_code}</br><b>MAPP/PP2 Prior:</b> {prior}</br><b>Rule:</b> {rule_short}"

        # Build BED line
        bed_line = f"{chrom}\t{genomic_start}\t{genomic_end}\t{full_hgvsc}\t0\t.\t{genomic_start}\t{genomic_end}\t{color}\t{hgvsp}\t{prior}\t{acmg_code}\t{rule}\t{mouse_over}"
        bed_entries.append(bed_line)
        stats['included'] += 1

    return bed_entries

def create_track(db):
    """Create BED and bigBed files for a given genome assembly"""
    print(f"\n{'='*70}")
    print(f"Processing {db}")
    print(f"{'='*70}")

    stats = {
        'included': 0,
        'excluded_middle': 0,
        'parse_failed': 0,
        'coord_failed': 0,
    }

    all_bed_entries = []
    for gene, gene_info in GENES.items():
        print(f"\n  Processing {gene}...")
        entries = process_gene(gene, gene_info, db, stats)
        print(f"    Added {len(entries)} variants")
        all_bed_entries.extend(entries)

    # Write BED file
    bed_file = os.path.join(OUTPUT_DIR, f"InSiGHTHCIPriors_{db}.bed")
    print(f"\nWriting BED file: {bed_file}")
    with open(bed_file, 'w') as f:
        f.write('\n'.join(all_bed_entries) + '\n')

    # Sort BED file
    print("Sorting BED file...")
    bash(f"sort -k1,1 -k2,2n {bed_file} -o {bed_file}")

    # Create bigBed
    as_file = os.path.join(OUTPUT_DIR, "InSiGHTHCIPriors.as")
    bb_file = os.path.join(OUTPUT_DIR, f"InSiGHTHCIPriors{db.capitalize()}.bb")
    chrom_sizes = f"/cluster/data/{db}/chrom.sizes"

    print(f"Creating bigBed file: {bb_file}")
    try:
        bash(f"bedToBigBed -as={as_file} -type=bed9+5 -tab {bed_file} {chrom_sizes} {bb_file}")
        print(f"  Successfully created: {bb_file}")
    except Exception as e:
        print(f"  ERROR creating bigBed: {e}")

    # Print stats
    print(f"\n  Statistics for {db}:")
    print(f"    Included: {stats['included']}")
    print(f"    Excluded (prior 0.11-0.68): {stats['excluded_middle']}")
    print(f"    Parse failed: {stats['parse_failed']}")
    print(f"    Coordinate failed: {stats['coord_failed']}")

    return bed_file, bb_file

def main():
    print("=" * 70)
    print("InSiGHT VCEP HCI Priors Track Generator")
    print("=" * 70)

    # Create output directory if needed
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    # Write AutoSql file
    as_file = os.path.join(OUTPUT_DIR, "InSiGHTHCIPriors.as")
    print(f"\nWriting AutoSql file: {as_file}")
    with open(as_file, 'w') as f:
        f.write(AUTOSQL)

    # Process both assemblies
    output_files = {}
    for db in ['hg38', 'hg19']:
        try:
            bed_file, bb_file = create_track(db)
            output_files[db] = {'bed': bed_file, 'bb': bb_file}
        except Exception as e:
            print(f"\nERROR processing {db}: {e}")
            import traceback
            traceback.print_exc()

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
    print("Done!")
    print("=" * 70)

if __name__ == "__main__":
    main()
