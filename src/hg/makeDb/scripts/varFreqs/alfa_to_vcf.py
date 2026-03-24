#!/usr/bin/env python3
"""
Convert NCBI ALFA bigBed files to VCF format.

- Merges all 12 population files into single VCF
- Normalizes multi-allelic variants (one ALT per line)
- Filters out variants with AF=0 in all populations
- Omits population AF fields when AF=0 (assumes missing = 0)
"""

import subprocess
import sys
import re
import argparse
from collections import defaultdict

POPULATIONS = ['GLB', 'EUR', 'AFA', 'AFO', 'AFR', 'EAS', 'SAS', 'OAS', 'ASN', 'LAC', 'LEN', 'OTR']
POP_DESCRIPTIONS = {
    'GLB': 'Global (all populations)',
    'EUR': 'European',
    'AFA': 'African American',
    'AFO': 'African Others',
    'AFR': 'African',
    'EAS': 'East Asian',
    'SAS': 'South Asian',
    'OAS': 'Other Asian',
    'ASN': 'Asian',
    'LAC': 'Latin American 1',
    'LEN': 'Latin American 2',
    'OTR': 'Other'
}

BB_DIR = '/hive/data/genomes/hg38/bed/varFreqs/alfa'

# hg38 chromosome sizes
CHROM_SIZES = {
    'chr1': 248956422, 'chr2': 242193529, 'chr3': 198295559, 'chr4': 190214555,
    'chr5': 181538259, 'chr6': 170805979, 'chr7': 159345973, 'chr8': 145138636,
    'chr9': 138394717, 'chr10': 133797422, 'chr11': 135086622, 'chr12': 133275309,
    'chr13': 114364328, 'chr14': 107043718, 'chr15': 101991189, 'chr16': 90338345,
    'chr17': 83257441, 'chr18': 80373285, 'chr19': 58617616, 'chr20': 64444167,
    'chr21': 46709983, 'chr22': 50818468, 'chrX': 156040895, 'chrY': 57227415,
    'chrM': 16569
}

def parse_frequency_field(freq_str):
    """
    Parse frequency field like:
    'chr1:10230,REF_AF(AC)=0.6048;ALT_AF(A)=0.3952'
    'chr1:10249,REF_AF(A)=0.6091;ALT_AF(C,G,T)=0.3909,0.0000,0.0000'

    Returns: (ref_allele, [alt_alleles], [alt_freqs])
    """
    # Extract REF_AF(allele)=freq
    ref_match = re.search(r'REF_AF\(([^)]+)\)=([0-9.]+)', freq_str)
    if not ref_match:
        return None, [], []
    ref_allele = ref_match.group(1)

    # Extract ALT_AF(allele1,allele2,...)=freq1,freq2,...
    alt_match = re.search(r'ALT_AF\(([^)]+)\)=([0-9.,]+)', freq_str)
    if not alt_match:
        return ref_allele, [], []

    alt_alleles_str = alt_match.group(1)
    alt_freqs_str = alt_match.group(2)

    alt_alleles = alt_alleles_str.split(',')
    alt_freqs = [float(f) for f in alt_freqs_str.split(',')]

    return ref_allele, alt_alleles, alt_freqs


def extract_region(bb_file, chrom, start, end):
    """Extract data from bigBed file for a region."""
    cmd = ['bigBedToBed', bb_file, 'stdout', '-chrom=' + chrom,
           '-start=' + str(start), '-end=' + str(end)]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        sys.stderr.write(f"Error extracting from {bb_file}: {result.stderr}\n")
        return []

    records = []
    for line in result.stdout.strip().split('\n'):
        if not line:
            continue
        fields = line.split('\t')
        if len(fields) < 10:
            continue
        chrom = fields[0]
        start = int(fields[1])
        end = int(fields[2])
        rsid = fields[3]
        freq_field = fields[9]

        ref, alts, alt_freqs = parse_frequency_field(freq_field)
        if ref is None:
            continue

        records.append({
            'chrom': chrom,
            'start': start,
            'end': end,
            'rsid': rsid,
            'ref': ref,
            'alts': alts,
            'alt_freqs': alt_freqs
        })

    return records


def write_vcf_header(outfile):
    """Write VCF header."""
    outfile.write("##fileformat=VCFv4.2\n")
    outfile.write("##source=NCBI_ALFA\n")
    outfile.write("##reference=GRCh38\n")

    # Contig headers
    for chrom in ['chr1', 'chr2', 'chr3', 'chr4', 'chr5', 'chr6', 'chr7', 'chr8',
                  'chr9', 'chr10', 'chr11', 'chr12', 'chr13', 'chr14', 'chr15',
                  'chr16', 'chr17', 'chr18', 'chr19', 'chr20', 'chr21', 'chr22',
                  'chrX', 'chrY', 'chrM']:
        outfile.write(f"##contig=<ID={chrom},length={CHROM_SIZES[chrom]},assembly=GRCh38>\n")

    # INFO headers
    for pop in POPULATIONS:
        desc = POP_DESCRIPTIONS[pop]
        outfile.write(f'##INFO=<ID=AF_{pop},Number=A,Type=Float,Description="Allele frequency in {desc}">\n')

    outfile.write("#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO\n")


def process_region(chrom, start, end, outfile, zerofile=None):
    """
    Process a genomic region: extract from all bigBeds, merge, write VCF.
    Returns (variant_count, zero_af_count)
    """
    # Extract data from all population files
    pop_data = {}
    for pop in POPULATIONS:
        bb_file = f"{BB_DIR}/ALFA_{pop}.bb"
        pop_data[pop] = extract_region(bb_file, chrom, start, end)

    # Index GLB data by (chrom, start, rsid) for merging
    # All populations should have same variants, so use GLB as reference
    glb_index = {}
    for rec in pop_data['GLB']:
        key = (rec['chrom'], rec['start'], rec['rsid'])
        glb_index[key] = rec

    # Build index for other populations
    other_indices = {}
    for pop in POPULATIONS:
        if pop == 'GLB':
            continue
        other_indices[pop] = {}
        for rec in pop_data[pop]:
            key = (rec['chrom'], rec['start'], rec['rsid'])
            other_indices[pop][key] = rec

    variant_count = 0
    zero_af_count = 0

    # Process each variant from GLB
    for key, glb_rec in sorted(glb_index.items(), key=lambda x: (x[0][0], x[0][1])):
        chrom = glb_rec['chrom']
        pos = glb_rec['start'] + 1  # Convert to 1-based VCF
        rsid = glb_rec['rsid']
        ref = glb_rec['ref']
        alts = glb_rec['alts']

        # Collect frequencies for each ALT allele from all populations
        # Structure: alt_idx -> pop -> freq
        alt_pop_freqs = []
        for i, alt in enumerate(alts):
            pop_freqs = {}
            # GLB frequency
            if i < len(glb_rec['alt_freqs']):
                pop_freqs['GLB'] = glb_rec['alt_freqs'][i]

            # Other populations
            for pop in POPULATIONS:
                if pop == 'GLB':
                    continue
                if key in other_indices[pop]:
                    other_rec = other_indices[pop][key]
                    if i < len(other_rec['alt_freqs']):
                        pop_freqs[pop] = other_rec['alt_freqs'][i]

            alt_pop_freqs.append(pop_freqs)

        # Normalize: write one line per ALT allele
        for i, alt in enumerate(alts):
            pop_freqs = alt_pop_freqs[i]

            # Check if all frequencies are 0
            all_zero = all(f == 0.0 for f in pop_freqs.values())

            if all_zero:
                zero_af_count += 1
                if zerofile:
                    zerofile.write(f"{chrom}\t{pos}\t{rsid}\t{ref}\t{alt}\n")
                continue

            # Build INFO field - only include populations with AF > 0
            info_parts = []
            for pop in POPULATIONS:
                if pop in pop_freqs and pop_freqs[pop] > 0:
                    info_parts.append(f"AF_{pop}={pop_freqs[pop]:.4f}")

            if not info_parts:
                # Shouldn't happen if not all_zero, but safety check
                continue

            info_str = ';'.join(info_parts)

            # Write VCF line
            outfile.write(f"{chrom}\t{pos}\t{rsid}\t{ref}\t{alt}\t.\t.\t{info_str}\n")
            variant_count += 1

    return variant_count, zero_af_count


def process_chromosome(chrom, outfile, zerofile=None, chunk_size=10000000):
    """Process an entire chromosome in chunks."""
    chrom_size = CHROM_SIZES.get(chrom)
    if not chrom_size:
        sys.stderr.write(f"Unknown chromosome: {chrom}\n")
        return 0, 0

    total_variants = 0
    total_zero = 0

    for start in range(0, chrom_size, chunk_size):
        end = min(start + chunk_size, chrom_size)
        sys.stderr.write(f"  Processing {chrom}:{start}-{end}...\n")
        v, z = process_region(chrom, start, end, outfile, zerofile)
        total_variants += v
        total_zero += z

    return total_variants, total_zero


def main():
    parser = argparse.ArgumentParser(description='Convert ALFA bigBed to VCF')
    parser.add_argument('--chrom', nargs='+', help='Chromosome(s) to process (e.g., chr1 chr2). If not specified, process all.')
    parser.add_argument('--start', type=int, help='Start position (0-based)')
    parser.add_argument('--end', type=int, help='End position')
    parser.add_argument('--out', required=True, help='Output VCF file')
    parser.add_argument('--zero-af-file', help='File to write variants with all-zero AF')
    parser.add_argument('--chunk-size', type=int, default=10000000, help='Chunk size for processing (default: 10Mb)')
    parser.add_argument('--no-header', action='store_true', help='Skip VCF header (for merging)')
    args = parser.parse_args()

    with open(args.out, 'w') as outfile:
        if not args.no_header:
            write_vcf_header(outfile)

        zerofile = None
        if args.zero_af_file:
            zerofile = open(args.zero_af_file, 'w')
            if not args.no_header:
                zerofile.write("#CHROM\tPOS\tID\tREF\tALT\n")

        try:
            if args.chrom and len(args.chrom) == 1 and args.start is not None and args.end is not None:
                # Process single region
                variant_count, zero_count = process_region(
                    args.chrom[0], args.start, args.end, outfile, zerofile)
                sys.stderr.write(f"Wrote {variant_count} variants, filtered {zero_count} with all-zero AF\n")

            elif args.chrom:
                # Process specified chromosome(s)
                total_variants = 0
                total_zero = 0
                for chrom in args.chrom:
                    sys.stderr.write(f"Processing {chrom}...\n")
                    v, z = process_chromosome(chrom, outfile, zerofile, args.chunk_size)
                    total_variants += v
                    total_zero += z
                    sys.stderr.write(f"  {chrom}: {v} variants, {z} filtered\n")
                sys.stderr.write(f"Total: wrote {total_variants} variants, filtered {total_zero} with all-zero AF\n")

            else:
                # Process all chromosomes
                total_variants = 0
                total_zero = 0
                chroms = ['chr1', 'chr2', 'chr3', 'chr4', 'chr5', 'chr6', 'chr7', 'chr8',
                          'chr9', 'chr10', 'chr11', 'chr12', 'chr13', 'chr14', 'chr15',
                          'chr16', 'chr17', 'chr18', 'chr19', 'chr20', 'chr21', 'chr22',
                          'chrX', 'chrY', 'chrM']

                for chrom in chroms:
                    sys.stderr.write(f"Processing {chrom}...\n")
                    v, z = process_chromosome(chrom, outfile, zerofile, args.chunk_size)
                    total_variants += v
                    total_zero += z
                    sys.stderr.write(f"  {chrom}: {v} variants, {z} filtered\n")

                sys.stderr.write(f"\nTotal: wrote {total_variants} variants, filtered {total_zero} with all-zero AF\n")

        finally:
            if zerofile:
                zerofile.close()


if __name__ == '__main__':
    main()
