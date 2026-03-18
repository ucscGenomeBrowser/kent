#!/usr/bin/env python3
"""
Convert TSV variant file to VCF format with proper indel formatting using pyfaidx
"""

import gzip
import sys
import argparse
from datetime import datetime
import pyfaidx
from collections import defaultdict

def open_file(filename):
    """Open regular or gzipped file"""
    if filename.endswith('.gz'):
        return gzip.open(filename, 'rt')
    else:
        return open(filename, 'r')

def format_indel(chrom, pos, ref, alt, fasta):
    """
    Format indels for VCF by adding flanking base
    VCF requires the preceding base for indels
    """
    # Convert position to 0-based for pyfaidx (TSV appears to be 1-based)
    pos_0based = pos - 1
    
    # Get the preceding base from reference
    try:
        if pos_0based > 0:
            preceding_base = fasta[chrom][pos_0based - 1:pos_0based].seq.upper()
        else:
            # Edge case: indel at position 1
            preceding_base = 'N'
    except:
        # If chromosome not found or other error, use N
        preceding_base = 'N'
    
    # Handle deletions (Alt is '-')
    if alt == '-':
        # For deletion: REF = preceding_base + deleted_sequence, ALT = preceding_base
        vcf_ref = preceding_base + ref
        vcf_alt = preceding_base
        vcf_pos = pos - 1  # Shift position back by 1
    # Handle insertions (Ref is '-')
    elif ref == '-':
        # For insertion: REF = preceding_base, ALT = preceding_base + inserted_sequence
        vcf_ref = preceding_base
        vcf_alt = preceding_base + alt
        vcf_pos = pos - 1  # Shift position back by 1
    else:
        # Not an indel, shouldn't happen but handle gracefully
        vcf_ref = ref
        vcf_alt = alt
        vcf_pos = pos
    
    return vcf_pos, vcf_ref, vcf_alt

def natural_sort_key(chrom):
    """Sort chromosomes naturally (1-22, X, Y, M, others)"""
    chrom = chrom.replace('chr', '')
    if chrom.isdigit():
        return (0, int(chrom))
    elif chrom == 'X':
        return (1, 23)
    elif chrom == 'Y':
        return (1, 24)
    elif chrom in ['M', 'MT']:
        return (1, 25)
    else:
        return (2, chrom)

def tsv_to_vcf(tsv_file, vcf_file, reference_fasta):
    """Convert TSV file to VCF format"""
    
    # Load reference genome
    print(f"Loading reference genome from {reference_fasta}...")
    fasta = pyfaidx.Fasta(reference_fasta)
    
    # Store all variants in memory for sorting
    variants = []
    
    # Process TSV file
    with open_file(tsv_file) as f:
        # Read header
        header = f.readline().strip().split('\t')
        
        # Find column indices
        chr_idx = header.index('Chr')
        start_idx = header.index('Start')
        ref_idx = header.index('Ref')
        alt_idx = header.index('Alt')
        
        # Optional fields to include in INFO
        info_field_indices = {}
        if 'PredictedFunc.refGene' in header:
            info_field_indices['Function'] = header.index('PredictedFunc.refGene')
        
        if 'Gene.refGene' in header:
            info_field_indices['Gene'] = header.index('Gene.refGene')
        
        if 'avsnp150' in header:
            rsid_idx = header.index('avsnp150')
        else:
            rsid_idx = None
        
        if 'Frequencies' in header:
            info_field_indices['AF'] = header.index('Frequencies')
        
        if 'FILTER' in header:
            filter_idx = header.index('FILTER')
        else:
            filter_idx = None
        
        # Process each variant
        for line_num, line in enumerate(f, start=2):
            if not line.strip():
                continue
            
            fields = line.strip().split('\t')
            
            try:
                chrom = fields[chr_idx]
                if not chrom.startswith('chr'):
                    chrom = 'chr' + chrom
                
                pos = int(fields[start_idx])
                ref = fields[ref_idx]
                alt = fields[alt_idx]
                
                # Handle indels
                if ref == '-' or alt == '-':
                    vcf_pos, vcf_ref, vcf_alt = format_indel(chrom, pos, ref, alt, fasta)
                else:
                    # SNP or MNP - no modification needed
                    vcf_pos = pos
                    vcf_ref = ref
                    vcf_alt = alt
                
                # Get ID (rsID if available)
                if rsid_idx is not None and fields[rsid_idx] and fields[rsid_idx] not in ['', '.']:
                    variant_id = fields[rsid_idx]
                else:
                    variant_id = '.'
                
                # Get FILTER
                if filter_idx is not None:
                    filter_val = fields[filter_idx] if fields[filter_idx] else 'PASS'
                else:
                    filter_val = 'PASS'
                
                # Build INFO field
                info_parts = []
                for info_name, idx in info_field_indices.items():
                    if fields[idx] and fields[idx] not in ['', '.']:
                        # Clean up the value (remove special characters if needed)
                        value = fields[idx].replace(';', ',').replace(' ', '_')
                        info_parts.append(f"{info_name}={value}")
                
                info_field = ';'.join(info_parts) if info_parts else '.'
                
                # Store variant for sorting
                variants.append({
                    'chrom': chrom,
                    'pos': vcf_pos,
                    'id': variant_id,
                    'ref': vcf_ref,
                    'alt': vcf_alt,
                    'filter': filter_val,
                    'info': info_field,
                    'original_line': line_num
                })
                
            except Exception as e:
                print(f"Error processing line {line_num}: {e}", file=sys.stderr)
                print(f"Line content: {line}", file=sys.stderr)
                continue
    
    # Sort variants by chromosome and position
    print(f"Sorting {len(variants)} variants...")
    variants.sort(key=lambda x: (natural_sort_key(x['chrom']), x['pos'], x['ref'], x['alt']))
    
    # Check for duplicates at same position and warn
    prev_variant = None
    for variant in variants:
        if prev_variant and prev_variant['chrom'] == variant['chrom'] and prev_variant['pos'] == variant['pos']:
            print(f"Warning: Multiple variants at {variant['chrom']}:{variant['pos']} (original lines {prev_variant['original_line']}, {variant['original_line']})", 
                  file=sys.stderr)
        prev_variant = variant
    
    # Open output VCF file
    out_handle = gzip.open(vcf_file, 'wt') if vcf_file.endswith('.gz') else open(vcf_file, 'w')
    
    # Write VCF header
    out_handle.write("##fileformat=VCFv4.2\n")
    out_handle.write(f"##fileDate={datetime.now().strftime('%Y%m%d')}\n")
    out_handle.write(f"##source=TSV_to_VCF_converter\n")
    out_handle.write(f"##reference={reference_fasta}\n")
    
    # Write INFO field descriptions
    if 'Function' in info_field_indices:
        out_handle.write('##INFO=<ID=Function,Number=1,Type=String,Description="Predicted function from refGene">\n')
    if 'Gene' in info_field_indices:
        out_handle.write('##INFO=<ID=Gene,Number=1,Type=String,Description="Gene from refGene">\n')
    if 'AF' in info_field_indices:
        out_handle.write('##INFO=<ID=AF,Number=A,Type=Float,Description="Allele frequency">\n')
    
    # Write contigs in order (optional but good practice)
    seen_chroms = sorted(set(v['chrom'] for v in variants), key=natural_sort_key)
    for chrom in seen_chroms:
        try:
            length = len(fasta[chrom])
            out_handle.write(f"##contig=<ID={chrom},length={length}>\n")
        except:
            # If we can't get the length, just write the contig ID
            out_handle.write(f"##contig=<ID={chrom}>\n")
    
    # Write VCF column headers
    out_handle.write("#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO\n")
    
    # Write sorted variants
    for variant in variants:
        out_handle.write(f"{variant['chrom']}\t{variant['pos']}\t{variant['id']}\t"
                        f"{variant['ref']}\t{variant['alt']}\t.\t{variant['filter']}\t{variant['info']}\n")
    
    out_handle.close()
    fasta.close()
    print(f"VCF file written to {vcf_file}")
    print(f"Total variants written: {len(variants)}")

def main():
    parser = argparse.ArgumentParser(description='Convert TSV variant file to VCF format')
    parser.add_argument('tsv_file', help='Input TSV file (can be gzipped)')
    parser.add_argument('vcf_file', help='Output VCF file (use .gz extension for compressed output)')
    parser.add_argument('reference_fasta', help='Reference genome FASTA file (must be indexed with samtools faidx)')
    
    args = parser.parse_args()
    
    tsv_to_vcf(args.tsv_file, args.vcf_file, args.reference_fasta)

if __name__ == '__main__':
    main()
