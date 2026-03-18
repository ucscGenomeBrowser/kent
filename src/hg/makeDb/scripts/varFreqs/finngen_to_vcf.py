#!/usr/bin/env python3
"""
Convert FinnGen R12 annotated variants TSV to VCF format.
Written by Claude Opus 4.5

Usage:
    python finngen_to_vcf.py finnge_R12_annotated_variants_v1.gz finnge_R12_annotated_variants_v1.vcf
"""

import sys
import gzip
from collections import OrderedDict


def open_file(filename, mode='rt'):
    """Open regular or gzipped file."""
    if filename.endswith('.gz'):
        return gzip.open(filename, mode)
    return open(filename, mode)


def write_file(filename, mode='wt'):
    """Open regular or gzipped file for writing."""
    if filename.endswith('.gz'):
        return gzip.open(filename, mode)
    return open(filename, mode)


def sanitize_info_key(key):
    """Sanitize column name for VCF INFO field ID (alphanumeric and underscore only)."""
    # Replace problematic characters
    key = key.replace('.vcf.gz', '').replace('.', '_').replace('-', '_')
    return key


def get_info_definitions(columns):
    """Generate VCF INFO header definitions based on column names."""
    info_defs = OrderedDict()

    # Core fields with known types
    core_fields = {
        'AC': ('A', 'Integer', 'Allele count in genotypes'),
        'AC_Het': ('A', 'Integer', 'Heterozygous allele count'),
        'AC_Hom': ('A', 'Integer', 'Homozygous alternate allele count'),
        'AF': ('A', 'Float', 'Allele frequency'),
        'AN': ('1', 'Integer', 'Total number of alleles in called genotypes'),
        'NS': ('1', 'Integer', 'Number of samples with data'),
        'INFO': ('A', 'Float', 'Imputation INFO score'),
        'gene_most_severe': ('1', 'String', 'Gene with most severe consequence'),
        'most_severe': ('1', 'String', 'Most severe VEP consequence'),
        'rsid': ('1', 'String', 'dbSNP rsID'),
        'b37_coord': ('1', 'String', 'GRCh37/hg19 coordinate'),
    }

    # Patterns for batch-specific fields
    batch_patterns = {
        'AC_Het_': ('A', 'Integer', 'Heterozygous count for batch {}'),
        'AC_Hom_': ('A', 'Integer', 'Homozygous count for batch {}'),
        'AF_': ('A', 'Float', 'Allele frequency for batch {}'),
        'AN_': ('1', 'Integer', 'Allele number for batch {}'),
        'NS_': ('1', 'Integer', 'Sample count for batch {}'),
        'CHIP_': ('1', 'Integer', 'Chip indicator for batch {}'),
        'HWE_': ('A', 'Float', 'Hardy-Weinberg equilibrium p-value for batch {}'),
        'INFO_': ('A', 'Float', 'Imputation INFO score for batch {}'),
    }

    # Enrichment and external AF fields
    enrichment_patterns = {
        'EXOME_enrichment_': ('A', 'Float', 'Finnish enrichment vs {} in gnomAD exomes'),
        'EXOME_AF_': ('A', 'Float', 'Allele frequency for {} in gnomAD exomes'),
        'GENOME_enrichment_': ('A', 'Float', 'Finnish enrichment vs {} in gnomAD genomes'),
        'GENOME_AF_': ('A', 'Float', 'Allele frequency for {} in gnomAD genomes'),
    }

    skip_cols = {'#variant', 'variant', 'chr', 'pos', 'ref', 'alt'}

    for col in columns:
        if col.lower() in [s.lower() for s in skip_cols]:
            continue

        sanitized = sanitize_info_key(col)

        # Check core fields first
        if col in core_fields:
            number, vtype, desc = core_fields[col]
            info_defs[sanitized] = (number, vtype, desc)
            continue

        # Check batch patterns
        matched = False
        for prefix, (number, vtype, desc_template) in batch_patterns.items():
            if col.startswith(prefix):
                batch_name = col[len(prefix):]
                desc = desc_template.format(batch_name)
                info_defs[sanitized] = (number, vtype, desc)
                matched = True
                break

        if matched:
            continue

        # Check enrichment patterns
        for prefix, (number, vtype, desc_template) in enrichment_patterns.items():
            if col.startswith(prefix):
                pop_name = col[len(prefix):]
                desc = desc_template.format(pop_name)
                info_defs[sanitized] = (number, vtype, desc)
                matched = True
                break

        if matched:
            continue

        # Default: treat as string
        info_defs[sanitized] = ('.', 'String', f'FinnGen field: {col}')

    return info_defs


def write_vcf_header(outf, info_defs, input_file):
    """Write VCF header."""
    outf.write('##fileformat=VCFv4.2\n')
    outf.write(f'##source=FinnGen_R12_converted_from_{input_file}\n')
    outf.write('##reference=GRCh38\n')

    # Contig definitions for hg38
    chroms = [str(i) for i in range(1, 23)] + ['X', 'Y', 'M']
    for chrom in chroms:
        outf.write(f'##contig=<ID={chrom}>\n')

    # INFO field definitions
    for field_id, (number, vtype, desc) in info_defs.items():
        # Escape quotes in description
        desc_escaped = desc.replace('"', '\\"')
        outf.write(f'##INFO=<ID={field_id},Number={number},Type={vtype},Description="{desc_escaped}">\n')

    # Column header
    outf.write('#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO\n')


def convert_value(value, vtype):
    """Convert value to appropriate type and format for VCF."""
    if value == '' or value == '.' or value == 'NA' or value == 'nan':
        return None

    try:
        if vtype == 'Integer':
            # Handle float strings that should be integers
            return str(int(float(value)))
        elif vtype == 'Float':
            return str(float(value))
        else:
            # String: escape special characters
            return value.replace(';', '%3B').replace('=', '%3D').replace(' ', '_')
    except (ValueError, TypeError):
        return None


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <input.tsv.gz> <output.vcf.gz>", file=sys.stderr)
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2]

    print(f"Reading {input_file}...", file=sys.stderr)

    with open_file(input_file) as inf:
        # Read header
        header_line = inf.readline().strip()
        columns = header_line.split('\t')

        # Find column indices
        col_indices = {col: i for i, col in enumerate(columns)}

        # Determine variant columns
        variant_col = col_indices.get('#variant', col_indices.get('variant'))
        chr_col = col_indices.get('chr')
        pos_col = col_indices.get('pos')
        ref_col = col_indices.get('ref')
        alt_col = col_indices.get('alt')
        rsid_col = col_indices.get('rsid')

        # Get INFO field definitions
        info_defs = get_info_definitions(columns)

        # Create mapping from original column to sanitized INFO key
        col_to_info = {}
        skip_cols = {'#variant', 'variant', 'chr', 'pos', 'ref', 'alt'}
        for col in columns:
            if col.lower() not in [s.lower() for s in skip_cols]:
                col_to_info[col] = sanitize_info_key(col)

        print(f"Found {len(columns)} columns, {len(info_defs)} INFO fields", file=sys.stderr)

        # Write output
        with write_file(output_file) as outf:
            write_vcf_header(outf, info_defs, input_file)

            line_count = 0
            for line in inf:
                fields = line.strip().split('\t')

                # Get basic variant info
                chrom = fields[chr_col]
                pos = fields[pos_col]
                ref = fields[ref_col]
                alt = fields[alt_col]

                # Get rsID if available, otherwise use '.'
                if rsid_col is not None and rsid_col < len(fields):
                    rsid = fields[rsid_col]
                    if not rsid or rsid == 'NA' or rsid == '.':
                        rsid = '.'
                else:
                    rsid = '.'

                # Build INFO field
                info_parts = []
                for col, info_key in col_to_info.items():
                    if col == 'rsid':
                        continue  # rsid goes in ID column

                    idx = col_indices.get(col)
                    if idx is None or idx >= len(fields):
                        continue

                    value = fields[idx]
                    if info_key in info_defs:
                        _, vtype, _ = info_defs[info_key]
                        converted = convert_value(value, vtype)
                        if converted is not None:
                            info_parts.append(f"{info_key}={converted}")

                info_str = ';'.join(info_parts) if info_parts else '.'

                # Write VCF line
                outf.write(f"{chrom}\t{pos}\t{rsid}\t{ref}\t{alt}\t.\t.\t{info_str}\n")

                line_count += 1
                if line_count % 1000000 == 0:
                    print(f"Processed {line_count:,} variants...", file=sys.stderr)

            print(f"Done. Wrote {line_count:,} variants to {output_file}", file=sys.stderr)


if __name__ == '__main__':
    main()
