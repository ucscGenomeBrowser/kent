#!/usr/bin/env python3
"""
InSiGHT VCEP AF Frequencies Track Generator

This script generates a UCSC Genome Browser track (bigBed format) that applies
ACMG classification guidelines based on gnomAD v4.1 exome AF_grpmax values
for Lynch syndrome genes: MLH1, MSH2, MSH6, and PMS2.

Coordinates are extracted from gnomAD v4.1 exomes bigBed, with detailed
annotations retrieved from the companion tab.gz file.

Author: Generated for InSiGHT VCEP
Date: 2025
"""

import subprocess
import os
import struct
import bisect
import gzip
import json

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
OUTPUT_DIR = "/hive/users/lrnassar/insightHub/afFrequencies"
GNOMAD_BB = "/gbdb/hg38/gnomAD/v4.1/exomes/exomes.bb"
GNOMAD_TAB = "/gbdb/hg38/gnomAD/v4.1/exomes/gnomad.v4.1.exomes.details.tab.gz"
GNOMAD_GZI = "/gbdb/hg38/gnomAD/v4.1/exomes/gnomad.v4.1.exomes.details.tab.gz.gzi"

# Gene to transcript mapping (coordinates will be queried from hgsql)
TRANSCRIPTS = {
    'MLH1': 'NM_000249.4',
    'MSH2': 'NM_000251.3',
    'MSH6': 'NM_000179.3',
    'PMS2': 'NM_000535.7',
}

# ACMG thresholds per gene
# Format: gene -> {'BA1': threshold, 'BS1': (lower, upper)}
# PM2_supporting is for AF < lowest BS1 threshold but > 0
THRESHOLDS = {
    'MLH1': {'BA1': 0.001, 'BS1_lower': 0.0001, 'BS1_upper': 0.001},
    'MSH2': {'BA1': 0.001, 'BS1_lower': 0.0001, 'BS1_upper': 0.001},
    'MSH6': {'BA1': 0.0022, 'BS1_lower': 0.00022, 'BS1_upper': 0.0022},
    'PMS2': {'BA1': 0.0028, 'BS1_lower': 0.00028, 'BS1_upper': 0.0028},
}

# Colors (RGB)
COLORS = {
    'PM2_supporting': '138,111,158',
    'BA1': '2,82,66',
    'BS1': '35,159,134',
}

# Rule text
RULES = {
    'MLH1': {
        'PM2_supporting': 'Absent/extremely rare (<1 in 50,000) in gnomADv4',
        'BA1': 'gnomADv4 Grpmax AF ≥ 0.001 (0.1%)',
        'BS1': 'gnomADv4 Grpmax AF ≥ 0.0001 and < 0.001 (0.01-0.1%)',
    },
    'MSH2': {
        'PM2_supporting': 'Absent/extremely rare (<1 in 50,000) in gnomADv4',
        'BA1': 'gnomADv4 Grpmax AF ≥ 0.001 (0.1%)',
        'BS1': 'gnomADv4 Grpmax AF ≥ 0.0001 and < 0.001 (0.01-0.1%)',
    },
    'MSH6': {
        'PM2_supporting': 'Absent/extremely rare (<1 in 50,000) in gnomADv4',
        'BA1': 'gnomADv4 Grpmax AF ≥ 0.0022 (0.22%)',
        'BS1': 'gnomADv4 Grpmax AF ≥ 0.00022 and < 0.0022 (0.022-0.22%)',
    },
    'PMS2': {
        'PM2_supporting': 'Absent/extremely rare (<1 in 50,000) in gnomADv4',
        'BA1': 'gnomADv4 Grpmax AF ≥ 0.0028 (0.28%)',
        'BS1': 'gnomADv4 Grpmax AF ≥ 0.00028 and < 0.0028 (0.028-0.28%)',
    },
}

# AutoSQL definition for the BED9+3 format
AUTOSQL = """table InSiGHTAF
"InSiGHT VCEP ACMG AF classifications for Lynch syndrome genes based on gnomAD v4.1 exomes"
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
   string acmgCode;    "ACMG classification code (PM2_supporting, BA1, BS1)"
   string rule;        "Classification rule/criteria"
   string _mouseOver;  "Field only used as mouseOver"
   )"""

# ============================================================================
# GZI Index Reader
# ============================================================================
class GziReader:
    """Read bgzip-compressed file using .gzi index"""

    def __init__(self, gz_file, gzi_file):
        self.gz_file = gz_file
        self.entries = []

        # Read gzi index
        with open(gzi_file, 'rb') as f:
            num_entries = struct.unpack('<Q', f.read(8))[0]
            for _ in range(num_entries):
                comp_off = struct.unpack('<Q', f.read(8))[0]
                uncomp_off = struct.unpack('<Q', f.read(8))[0]
                self.entries.append((comp_off, uncomp_off))

        self.uncomp_offsets = [e[1] for e in self.entries]
        self.gz_handle = open(gz_file, 'rb')

    def read_at_offset(self, offset, length):
        """Read data at specific uncompressed offset"""
        # Find starting block
        idx = bisect.bisect_right(self.uncomp_offsets, offset) - 1
        if idx < 0:
            idx = 0

        target_end = offset + length
        result = b''

        while True:
            if idx >= len(self.entries):
                break

            comp_offset = self.entries[idx][0]
            uncomp_block_start = self.entries[idx][1]

            # Get next block's compressed offset
            if idx + 1 < len(self.entries):
                next_comp_offset = self.entries[idx + 1][0]
            else:
                self.gz_handle.seek(0, 2)
                next_comp_offset = self.gz_handle.tell()

            block_size = next_comp_offset - comp_offset

            # Read and decompress this block
            self.gz_handle.seek(comp_offset)
            compressed_data = self.gz_handle.read(block_size)
            decompressed = gzip.decompress(compressed_data)

            if uncomp_block_start + len(decompressed) < offset:
                idx += 1
                continue

            # Add relevant portion to result
            start_in_block = max(0, offset - uncomp_block_start)
            end_in_block = min(len(decompressed), target_end - uncomp_block_start)

            if start_in_block < len(decompressed):
                result += decompressed[start_in_block:end_in_block]

            if uncomp_block_start + len(decompressed) >= target_end:
                break

            idx += 1

        return result.decode('utf-8', errors='replace')

    def close(self):
        self.gz_handle.close()

# ============================================================================
# Main Processing Functions
# ============================================================================

def get_transcript_info(accession):
    """Query hgsql to get transcript information from hg38.ncbiRefSeq"""
    query = f"SELECT name, chrom, strand, txStart, txEnd FROM ncbiRefSeq WHERE name='{accession}'"
    result = bash(f'hgsql hg38 -Ne "{query}"')

    if not result.strip():
        raise ValueError(f"Transcript {accession} not found in hg38.ncbiRefSeq")

    fields = result.strip().split('\t')

    return {
        'name': fields[0],
        'chrom': fields[1],
        'strand': fields[2],
        'txStart': int(fields[3]),
        'txEnd': int(fields[4]),
    }

def extract_variants(gene, gene_info):
    """Extract gnomAD variants for a gene region"""
    chrom = gene_info['chrom']
    start = gene_info['txStart']
    end = gene_info['txEnd']

    output_file = os.path.join(OUTPUT_DIR, f"gnomad_{gene}_raw.bed")
    bash(f"bigBedToBed {GNOMAD_BB} -chrom={chrom} -start={start} -end={end} {output_file}")

    variants = []
    with open(output_file, 'r') as f:
        for line in f:
            fields = line.strip().split('\t')
            variants.append(fields)

    return variants

def get_hgvsc_for_transcript(vep_json, transcript):
    """Extract HGVSc for specific transcript from VEP JSON"""
    try:
        vep_data = json.loads(vep_json)
        # Find gene that contains this transcript
        for gene_name, gene_data in vep_data.items():
            if 'hgvsc' in gene_data:
                for hgvsc in gene_data['hgvsc']:
                    if hgvsc.startswith(transcript + ':'):
                        return hgvsc
    except (json.JSONDecodeError, KeyError):
        pass
    return None

def classify_variant(af_grpmax, gene):
    """Classify variant based on AF_grpmax and gene-specific thresholds"""
    thresholds = THRESHOLDS[gene]

    if af_grpmax >= thresholds['BA1']:
        return 'BA1'
    elif af_grpmax >= thresholds['BS1_lower'] and af_grpmax < thresholds['BS1_upper']:
        return 'BS1'
    elif af_grpmax > 0 and af_grpmax < 0.00002:  # < 1 in 50,000
        return 'PM2_supporting'
    else:
        return None  # Does not meet any threshold criteria

def process_gene(gene, transcript, tx_info, gzi_reader, stats):
    """Process all variants for a gene"""
    print(f"\nProcessing {gene}...")

    # Extract variants from bigBed
    variants = extract_variants(gene, tx_info)
    print(f"  Found {len(variants)} total variants in region")

    bed_entries = []

    for variant in variants:
        chrom = variant[0]
        chromStart = int(variant[1])
        chromEnd = int(variant[2])
        md5_key = variant[3]
        af_grpmax_str = variant[26]  # AF_grpmax is field 27 (0-indexed: 26)
        data_offset = int(variant[29])  # _dataOffset
        data_len = int(variant[30])  # _dataLen

        # Check if AF_grpmax is valid (not N/A)
        if af_grpmax_str == 'N/A' or af_grpmax_str == '':
            stats['no_af_grpmax'] += 1
            continue

        try:
            af_grpmax = float(af_grpmax_str)
        except ValueError:
            stats['no_af_grpmax'] += 1
            continue

        # Get HGVSc from details file
        details_line = gzi_reader.read_at_offset(data_offset, data_len)
        details_fields = details_line.strip().split('\t')

        if len(details_fields) < 2:
            stats['no_hgvsc'] += 1
            continue

        vep_json = details_fields[1]  # _jsonVep
        hgvsc = get_hgvsc_for_transcript(vep_json, transcript)

        if hgvsc is None:
            stats['wrong_transcript'] += 1
            continue

        # Classify variant
        acmg_code = classify_variant(af_grpmax, gene)

        if acmg_code is None:
            stats['below_threshold'] += 1
            continue

        # Create BED entry
        color = COLORS[acmg_code]
        rule = RULES[gene][acmg_code]

        # HTML-encode special characters for mouseOver
        rule_html = rule.replace('≥', '&ge;').replace('≤', '&le;').replace('>', '&gt;').replace('<', '&lt;')

        mouse_over = f"<b>HGVSc:</b> {hgvsc}</br><b>ACMG code:</b> {acmg_code}</br><b>Rule:</b> {rule_html}"

        bed_line = f"{chrom}\t{chromStart}\t{chromEnd}\t{hgvsc}\t0\t.\t{chromStart}\t{chromEnd}\t{color}\t{acmg_code}\t{rule}\t{mouse_over}"
        bed_entries.append(bed_line)
        stats['included'] += 1

    print(f"  Included: {len(bed_entries)} variants")
    return bed_entries

def main():
    print("=" * 70)
    print("InSiGHT VCEP AF Frequencies Track Generator")
    print("=" * 70)

    # Create output directory if needed
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    # Write AutoSql file
    as_file = os.path.join(OUTPUT_DIR, "InSiGHTAF.as")
    print(f"\nWriting AutoSql file: {as_file}")
    with open(as_file, 'w') as f:
        f.write(AUTOSQL)

    # Initialize GZI reader
    print(f"\nInitializing gnomAD details reader...")
    gzi_reader = GziReader(GNOMAD_TAB, GNOMAD_GZI)

    # Track statistics
    stats = {
        'no_af_grpmax': 0,
        'no_hgvsc': 0,
        'wrong_transcript': 0,
        'below_threshold': 0,
        'included': 0,
    }

    # Query transcript coordinates from hgsql
    print(f"\nQuerying transcript coordinates from hg38.ncbiRefSeq...")
    transcript_info = {}
    for gene, accession in TRANSCRIPTS.items():
        tx_info = get_transcript_info(accession)
        print(f"  {gene}: {accession} ({tx_info['chrom']}:{tx_info['txStart']}-{tx_info['txEnd']})")
        transcript_info[gene] = {'transcript': accession, **tx_info}

    # Process each gene
    all_bed_entries = []
    for gene, accession in TRANSCRIPTS.items():
        tx_info = transcript_info[gene]
        entries = process_gene(gene, accession, tx_info, gzi_reader, stats)
        all_bed_entries.extend(entries)

    gzi_reader.close()

    # Write BED file
    bed_file = os.path.join(OUTPUT_DIR, "InSiGHTAF_hg38.bed")
    print(f"\nWriting BED file: {bed_file}")
    with open(bed_file, 'w') as f:
        f.write('\n'.join(all_bed_entries) + '\n')

    # Sort BED file
    print("Sorting BED file...")
    bash(f"sort -k1,1 -k2,2n {bed_file} -o {bed_file}")

    # Create bigBed for hg38
    bb_file = os.path.join(OUTPUT_DIR, "InSiGHTAFHg38.bb")
    chrom_sizes = "/cluster/data/hg38/chrom.sizes"

    print(f"\nCreating bigBed file: {bb_file}")
    try:
        bash(f"bedToBigBed -as={as_file} -type=bed9+3 -tab {bed_file} {chrom_sizes} {bb_file}")
        print(f"  Successfully created: {bb_file}")
    except Exception as e:
        print(f"  ERROR creating bigBed: {e}")

    # LiftOver to hg19
    # liftOver only handles basic BED, so we need to:
    # 1. Extract chrom, chromStart, chromEnd, name (for joining)
    # 2. Lift those coordinates
    # 3. Rejoin with the rest of the fields
    print(f"\nLifting over to hg19...")
    bed_file_hg19 = os.path.join(OUTPUT_DIR, "InSiGHTAF_hg19.bed")
    unmapped_file = os.path.join(OUTPUT_DIR, "InSiGHTAF_unmapped.bed")
    chain_file = "/cluster/data/hg38/bed/liftOver/hg38ToHg19.over.chain.gz"
    bb_file_hg19 = os.path.join(OUTPUT_DIR, "InSiGHTAFHg19.bb")

    try:
        # Create a BED4 file for liftOver (chrom, start, end, name)
        bed4_file = os.path.join(OUTPUT_DIR, "InSiGHTAF_hg38_bed4.bed")
        lifted_bed4 = os.path.join(OUTPUT_DIR, "InSiGHTAF_hg19_bed4.bed")

        # Extract BED4 and create lookup dict for extra fields
        extra_fields = {}  # name -> (fields 5-12)
        with open(bed_file, 'r') as fin, open(bed4_file, 'w') as fout:
            for line in fin:
                fields = line.strip().split('\t')
                name = fields[3]
                # Store extra fields (score, strand, thickStart, thickEnd, color, acmgCode, rule, mouseOver)
                extra_fields[name] = fields[4:]
                # Write BED4
                fout.write(f"{fields[0]}\t{fields[1]}\t{fields[2]}\t{fields[3]}\n")

        # Run liftOver on BED4
        bash(f"liftOver {bed4_file} {chain_file} {lifted_bed4} {unmapped_file}")
        unmapped_count = int(bash(f"wc -l < {unmapped_file}").strip()) // 2
        print(f"  Lifted over, {unmapped_count} variants could not be mapped")

        # Rejoin lifted coordinates with extra fields
        with open(lifted_bed4, 'r') as fin, open(bed_file_hg19, 'w') as fout:
            for line in fin:
                fields = line.strip().split('\t')
                chrom, start, end, name = fields[0], fields[1], fields[2], fields[3]
                if name in extra_fields:
                    extra = extra_fields[name]
                    # Update thickStart/thickEnd (fields 2 and 3 in extra, 0-indexed) to match new coords
                    extra[2] = start  # thickStart
                    extra[3] = end    # thickEnd
                    fout.write(f"{chrom}\t{start}\t{end}\t{name}\t" + "\t".join(extra) + "\n")

        # Sort hg19 BED
        bash(f"sort -k1,1 -k2,2n {bed_file_hg19} -o {bed_file_hg19}")

        # Create bigBed for hg19
        chrom_sizes_hg19 = "/cluster/data/hg19/chrom.sizes"

        bash(f"bedToBigBed -as={as_file} -type=bed9+3 -tab {bed_file_hg19} {chrom_sizes_hg19} {bb_file_hg19}")
        print(f"  Successfully created: {bb_file_hg19}")

        # Cleanup temp files
        os.remove(bed4_file)
        os.remove(lifted_bed4)
    except Exception as e:
        print(f"  ERROR during liftOver: {e}")

    # Print statistics
    print("\n" + "=" * 70)
    print("Statistics")
    print("=" * 70)
    print(f"  Variants included in track:        {stats['included']}")
    print(f"  Excluded - No AF_grpmax (N/A):     {stats['no_af_grpmax']}")
    print(f"  Excluded - No HGVSc in details:    {stats['no_hgvsc']}")
    print(f"  Excluded - Wrong transcript:       {stats['wrong_transcript']}")
    print(f"  Excluded - Below all thresholds:   {stats['below_threshold']}")
    print(f"  Total excluded:                    {stats['no_af_grpmax'] + stats['no_hgvsc'] + stats['wrong_transcript'] + stats['below_threshold']}")

    print("\n" + "=" * 70)
    print("Output Files")
    print("=" * 70)
    print(f"  AutoSql file: {as_file}")
    print(f"  hg38 BED:     {bed_file}")
    print(f"  hg38 bigBed:  {bb_file}")
    print(f"  hg19 BED:     {bed_file_hg19}")
    print(f"  hg19 bigBed:  {bb_file_hg19}")

    print("\n" + "=" * 70)
    print("Done!")
    print("=" * 70)

if __name__ == "__main__":
    main()
