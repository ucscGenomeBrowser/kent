#!/usr/bin/env python3
"""
Convert KOVA biobank TSV.gz files to VCF format.
Handles field renaming and preserves all information in INFO field.
"""

import gzip
import sys
import re
from datetime import datetime
import argparse

def clean_value(value):
    """Clean values for VCF INFO field."""
    if value == 'NA' or value == '.' or value == '':
        return '.'
    # Replace spaces and special characters that might cause issues
    value = value.replace(' ', '_')
    value = value.replace(';', ',')
    return value

def format_info_value(value):
    """Format a value for the INFO field."""
    if value == 'NA' or value == '.' or value == '':
        return None  # Will be skipped in INFO field
    return clean_value(value)

def convert_tsv_to_vcf(input_file, output_file):
    """Convert KOVA TSV.gz file to VCF format."""
    
    # Field name mappings
    field_mappings = {
        'kova_AC': 'AC',
        'kova_AN': 'AN', 
        'kova_AF': 'AF'
    }
    
    # Fields to exclude from INFO (already used in other VCF columns)
    exclude_from_info = {'chrom', 'pos', 'ref_allele', 'alt_allele', 'rsid', 'qual', 'filters'}
    
    with gzip.open(input_file, 'rt') if input_file.endswith('.gz') else open(input_file, 'r') as infile:
        # Read header line
        header_line = infile.readline().strip()
        headers = header_line.split('\t')
        
        # Prepare output file
        outfile = gzip.open(output_file, 'wt') if output_file.endswith('.gz') else open(output_file, 'w')
        
        # Write VCF header
        outfile.write("##fileformat=VCFv4.3\n")
        outfile.write(f"##fileDate={datetime.now().strftime('%Y%m%d')}\n")
        outfile.write(f"##source=KOVA_biobank_converter\n")
        outfile.write(f"##reference=GRCh38\n")
        
        # Write INFO field descriptions
        # Map field names and write descriptions
        info_fields = []
        for field in headers:
            if field not in exclude_from_info:
                # Apply field mapping if exists
                info_field = field_mappings.get(field, field)
                # Clean field name for VCF compatibility
                info_field = re.sub(r'[^A-Za-z0-9_]', '_', info_field)
                info_fields.append((field, info_field))
                
                # Determine field type based on name
                field_type = "String"  # Default
                if 'AC' in info_field or 'AN' in info_field or 'count' in field.lower():
                    field_type = "Integer"
                elif 'AF' in info_field or 'pvalue' in field.lower() or 'percentile' in field.lower():
                    field_type = "Float"
                
                outfile.write(f'##INFO=<ID={info_field},Number=.,Type={field_type},Description="{field}">\n')
        
        # Write VCF column header
        outfile.write("#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO\n")
        
        # Process data lines
        for line_num, line in enumerate(infile, start=2):
            fields = line.strip().split('\t')
            
            if len(fields) != len(headers):
                print(f"Warning: Line {line_num} has {len(fields)} fields, expected {len(headers)}")
                continue
            
            # Create dictionary of field values
            data = dict(zip(headers, fields))
            
            # Extract main VCF fields
            chrom = data.get('chrom', '.')
            # Remove 'chr' prefix if present for standard VCF
            if chrom.startswith('chr'):
                chrom = chrom[3:]
            
            # Position is 1-based in both TSV and VCF, no conversion needed
            pos = data.get('pos', '.')
            
            # ID field (using rsid)
            id_field = data.get('rsid', '.')
            if id_field == '.' or id_field == 'NA' or id_field == '':
                id_field = '.'
            
            ref = data.get('ref_allele', '.')
            alt = data.get('alt_allele', '.')
            
            # QUAL field
            qual = data.get('qual', '.')
            if qual == '.' or qual == 'NA' or qual == '':
                qual = '.'
            
            # FILTER field
            filter_field = data.get('filters', '.')
            if filter_field == '.' or filter_field == 'NA' or filter_field == '':
                filter_field = 'PASS'
            
            # Build INFO field
            info_parts = []
            for orig_field, info_field in info_fields:
                value = data.get(orig_field, '.')
                formatted_value = format_info_value(value)
                if formatted_value is not None:
                    # For flag fields (present/absent), just include the key
                    if formatted_value == 'true' or formatted_value == 'True':
                        info_parts.append(info_field)
                    else:
                        info_parts.append(f"{info_field}={formatted_value}")
            
            info = ';'.join(info_parts) if info_parts else '.'
            
            # Write VCF line
            vcf_line = f"{chrom}\t{pos}\t{id_field}\t{ref}\t{alt}\t{qual}\t{filter_field}\t{info}\n"
            outfile.write(vcf_line)
        
        outfile.close()
    
    print(f"Conversion complete. VCF file written to: {output_file}")

def main():
    parser = argparse.ArgumentParser(description='Convert KOVA biobank TSV.gz files to VCF format')
    parser.add_argument('input', help='Input TSV or TSV.gz file')
    parser.add_argument('output', help='Output VCF file (use .vcf.gz for compressed output)')
    
    args = parser.parse_args()
    
    convert_tsv_to_vcf(args.input, args.output)

if __name__ == "__main__":
    main()
