#!/usr/bin/env python3
"""
This script processes HGMD data for hg38/hg19 and year.
It performs the following:

1. Converts HGMD TSV files to BED format with variant classifications
2. Creates BigBed files
3. Creates symlinks and database links for the BigBed files
4. Extracts transcript IDs from HGMD data
5. Filters curated gene predictions from ncbiRefSeq
6. Loads gene predictions into the database
"""

import argparse
import subprocess
import sys
import os
import glob

def run_command(cmd, description):
    """
    Run a shell command and handle errors
    
    Args:
        cmd (str): Shell command to execute
        description (str): Human-readable description of the command
        
    Returns:
        bool: True if command succeeded, False otherwise
    """
    result = subprocess.run(
        cmd,
        shell=True,
        executable='/bin/bash',
        capture_output=True,
        text=True
    )
    
    if result.returncode != 0:
        print("Error: {} failed".format(description), file=sys.stderr)
        print("stderr: {}".format(result.stderr), file=sys.stderr)
        return False
    
    return True

def check_input_file_exists(year, db):
    """
    Check if the HGMD input file exists
    
    Args:
        year (str): Year of HGMD data file
        db (str): Database/genome build (hg38 or hg19)
        
    Returns:
        bool: True if file exists, False otherwise
    """
    input_file = "/hive/data/outside/hgmd/{}.4-hgmd-public_{}.tsv".format(year, db)
    if not os.path.isfile(input_file):
        print("Error: Input file {} does not exist".format(input_file), file=sys.stderr)
        return False
    return True

def find_latest_ncbirefseq_dir(db, year):
    """
    Find the latest ncbiRefSeq directory for the given year
    
    Searches for directories matching the pattern:
    - hg38: /hive/data/genomes/hg38/bed/ncbiRefSeq.p14.{year}-*
    - hg19: /hive/data/genomes/hg19/bed/ncbiRefSeq.p13.{year}-*
    
    If no directories match the given year, tries previous years (up to 5 years back)
    
    Args:
        db (str): Database/genome build (hg38 or hg19)
        year (str): Year to search for
        
    Returns:
        str: Path to the latest matching directory, or None if not found
    """
    # Determine ncbiRefSeq version based on database
    # hg19 uses p13, hg38 uses p14
    version = "p13" if db == "hg19" else "p14"
    
    base_path = "/hive/data/genomes/{}/bed".format(db)
    
    # Try the provided year first, then fall back to previous years
    current_year = int(year)
    max_attempts = 5  # Try up to 5 years back
    
    for attempt in range(max_attempts):
        pattern = os.path.join(base_path, "ncbiRefSeq.{}.{}-*".format(version, current_year))
        matching_dirs = glob.glob(pattern)
        
        if matching_dirs:
            # Found matching directories, return the latest
            latest_dir = sorted(matching_dirs)[-1]
            if current_year != int(year):
                print("Note: Using {} directory (year {} not found)".format(
                    os.path.basename(latest_dir), year))
            return latest_dir
        
        # No match, try previous year
        current_year -= 1
    
    # No directories found after trying multiple years
    print("Error: No directories matching pattern ncbiRefSeq.{}.* found (tried years {} to {})".format(
        version, year, int(year) - max_attempts + 1), file=sys.stderr)
    return None

def process_database(year, db, working_dir):
    """
    Process HGMD data for a single database
    
    This function performs the following steps:
    1. Reads HGMD TSV file and creates BED file with variant annotations
    2. Converts BED to BigBed format
    3. Creates symlink in /gbdb/{db}/bbi/
    4. Runs hgBbiDbLink to register the BigBed in the database
    
    Args:
        year (str): Year of HGMD data file
        db (str): Database/genome build (hg38 or hg19)
        working_dir (str): Working directory path
        
    Returns:
        bool: True if all steps succeeded, False otherwise
    """
    
    # ========================================================================
    # Step 1: Process TSV file and create BED file
    # ========================================================================
    # Read HGMD TSV, filter comments, classify variants, and sort by position
    # Variant types: I=insertion, D=deletion, M=substitution, X=indel, R=regulatory, S=splicing
    bed_file = "hgmd.bed"
    process_cmd = """cat /hive/data/outside/hgmd/{}.4-hgmd-public_{}.tsv | \
grep -v \\# | \
tawk '{{if ($5=="I") {{start=$4-1; end=$4+1; col="100,100,100"}} else if ($5=="D") {{start=$4-1; end=$4; col="170,170,170"}} else {{start=$4-1; end=$4; col="0,0,0"}}; print "chr"$3,start,end,$2":"$1,0,".",start,end,col,$2,$1,$5}}' | \
sed -e 's/M$/substitution/' | \
sed -e 's/I$/insertion (between the two basepairs, sequence not provided by HGMD)/' | \
sed -e 's/D$/deletion (endpoint not provided by HGMD)/' | \
sed -e 's/X$/insertion-deletion (endpoint not provided by HGMD)/' | \
sed -e 's/R$/regulatory variant/' | \
sed -e 's/S$/splicing variant/' | \
sort -k1,1 -k2,2n > {}""".format(year, db, bed_file)
    
    if not run_command(process_cmd, "Processing TSV and creating BED file for {}".format(db)):
        return False
    
    # ========================================================================
    # Step 2: Convert BED to BigBed
    # ========================================================================
    bb_file = "hgmd.bb"
    bigbed_cmd = """bedToBigBed {} /hive/data/genomes/{}/chrom.sizes {} \
-type=bed9+ \
-as=/hive/data/genomes/{}/bed/hgmd/hgmd.as \
-tab""".format(bed_file, db, bb_file, db)
    
    if not run_command(bigbed_cmd, "Converting BED to BigBed for {}".format(db)):
        return False
    
    # ========================================================================
    # Step 3: Create symlink
    # ========================================================================
    # Create symlink from /gbdb/{db}/bbi/hgmd.bb to the actual BigBed file
    symlink_dir = "/gbdb/{}/bbi".format(db)
    symlink_path = os.path.join(symlink_dir, "hgmd.bb")
    bb_path = os.path.join(working_dir, bb_file)
    
    # Create symlink directory if it doesn't exist
    os.makedirs(symlink_dir, exist_ok=True)
    
    # Remove existing symlink if it exists (for updates)
    if os.path.islink(symlink_path) or os.path.exists(symlink_path):
        os.remove(symlink_path)
    
    # Create new symlink
    symlink_cmd = "ln -s {} {}".format(bb_path, symlink_path)
    if not run_command(symlink_cmd, "Creating symlink for {}".format(db)):
        return False
    
    # ========================================================================
    # Step 4: Run hgBbiDbLink
    # ========================================================================
    hgbbidblink_cmd = "hgBbiDbLink {} hgmd {}".format(db, symlink_path)
    if not run_command(hgbbidblink_cmd, "Running hgBbiDbLink for {}".format(db)):
        return False
    
    # Output summary
    bed_path = os.path.join(working_dir, bed_file)
    print("✓ {} BigBed completed successfully!".format(db))
    print("Output files: {}, {}".format(bed_path, bb_path))
    print("Symlink created: {}".format(symlink_path))
    print("hgBbiDbLink run: hgBbiDbLink {} hgmd {}".format(db, symlink_path))
    
    return True

def process_transcripts(year, db, ncbirefseq_source_dir, output_dir):
    """
    Extract HGMD transcripts and filter gene predictions
    
    NOTE: Always uses hg38 HGMD file for transcript extraction because
    only the hg38 file contains transcript IDs (column 7). The hg19 file
    only has 6 columns without transcript information.
    
    This function performs the following steps:
    1. Creates output directory matching the ncbiRefSeq date
    2. Extracts unique transcript IDs from HGMD hg38 data
    3. Filters gene predictions to only include HGMD transcripts
    4. Loads filtered gene predictions into the database
    
    Args:
        year (str): Year of HGMD data file
        db (str): Database/genome build (hg38 or hg19)
        ncbirefseq_source_dir (str): Path to source ncbiRefSeq directory
        output_dir (str): Base output directory path
        
    Returns:
        bool: True if all steps succeeded, False otherwise
    """
    
    # ========================================================================
    # Extract date and create output directory
    # ========================================================================
    # Extract the date suffix and version from the ncbirefseq source directory
    # Example: /hive/data/genomes/hg38/bed/ncbiRefSeq.p14.2025-08-13
    #          -> version="p14", date_suffix="2025-08-13"
    dir_basename = os.path.basename(ncbirefseq_source_dir)
    
    # Determine version (p13 or p14)
    if dir_basename.startswith("ncbiRefSeq.p13."):
        version = "p13"
        date_suffix = dir_basename.replace("ncbiRefSeq.p13.", "")
    else:
        version = "p14"
        date_suffix = dir_basename.replace("ncbiRefSeq.p14.", "")
    
    # Create output directory with same version and date pattern
    # Example: /hive/data/genomes/hg38/bed/hgmd/ncbiRefSeq.p14.2025-08-13/
    transcript_output_dir = os.path.join(output_dir, "ncbiRefSeq.{}.{}".format(version, date_suffix))
    os.makedirs(transcript_output_dir, exist_ok=True)
    
    # Change to output directory for file creation
    os.chdir(transcript_output_dir)
    
    # ========================================================================
    # Step 1: Extract transcript IDs from HGMD
    # ========================================================================
    # IMPORTANT: Always use hg38 file because it has transcript IDs in column 7
    # The hg19 file only has 6 columns without transcript information
    # 
    # hg38 file format (7 columns):
    # CM1720767 A1CF 10 50814012 M GRCh38 NM_001198819.2
    #
    # hg19 file format (6 columns):  
    # CM1720767 A1CF 10 52573772 M GRCh37
    #
    # Extract column 7 (transcript IDs), remove version numbers (.1, .2, etc),
    # get unique IDs, and add back the dot for matching
    transcript_file = "hgmdTranscripts.txt"
    extract_cmd = """cat /hive/data/outside/hgmd/{}.4-hgmd-public_hg38.tsv | \
cut -f7 | \
cut -d. -f1 | \
sort -u | \
awk '{{print $1"."}}' > {}""".format(year, transcript_file)
    
    if not run_command(extract_cmd, "Extracting HGMD transcripts for {}".format(db)):
        return False
    
    # ========================================================================
    # Step 2: Filter gene predictions
    # ========================================================================
    # Extract only the gene predictions for transcripts found in HGMD
    # This creates a subset of ncbiRefSeq focused on disease-related genes
    output_file = "hgmd.curated.gp"
    gp_path = "/hive/data/genomes/{}/bed/ncbiRefSeq.{}.{}/process/{}.curated.gp.gz".format(
        db, version, date_suffix, db)
    
    filter_cmd = """zcat {} | fgrep -f {} - > {}""".format(
        gp_path, transcript_file, output_file)
    
    if not run_command(filter_cmd, "Filtering gene predictions for {}".format(db)):
        return False
    
    # ========================================================================
    # Step 3: Load gene predictions into database
    # ========================================================================
    # Load the filtered gene predictions into a database table
    # Table name: ncbiRefSeqHgmd
    # Format: genePredExt (extended gene prediction format)
    hgloadgenepred_cmd = "hgLoadGenePred -genePredExt {} ncbiRefSeqHgmd {}".format(db, output_file)
    if not run_command(hgloadgenepred_cmd, "Loading gene predictions for {}".format(db)):
        return False
    
    # Output summary
    transcript_path = os.path.join(transcript_output_dir, transcript_file)
    gp_output_path = os.path.join(transcript_output_dir, output_file)
    print("✓ {} transcript processing completed!".format(db))
    print("Output files: {}, {}".format(transcript_path, gp_output_path))
    print("hgLoadGenePred run: hgLoadGenePred -genePredExt {} ncbiRefSeqHgmd {}".format(db, output_file))
    
    return True

def main():
    """
    Main function to orchestrate HGMD data processing
    
    Processing flow:
    1. Parse command-line arguments
    2. Validate input files exist
    3. Process BigBed files for each database
    4. Find latest ncbiRefSeq directories
    5. Process transcript data for each database
    """
    
    # ========================================================================
    # Parse command-line arguments
    # ========================================================================
    parser = argparse.ArgumentParser(
        description='Process HGMD data and convert to BigBed format'
    )
    parser.add_argument(
        '--year',
        required=True,
        help='Year for HGMD data file (e.g., 2024)'
    )
    parser.add_argument(
        '--db',
        required=True,
        nargs='+',
        choices=['hg38', 'hg19'],
        help='Database/genome build (hg38 and/or hg19, can specify multiple)'
    )
    
    args = parser.parse_args()
    year = args.year
    databases = args.db
    
    # ========================================================================
    # Validate input files
    # ========================================================================
    # Check that HGMD input files exist before starting any processing
    for db in databases:
        if not check_input_file_exists(year, db):
            sys.exit(1)
    
    # ========================================================================
    # Process BigBed files for each database
    # ========================================================================
    # Create BED files, convert to BigBed, create symlinks, and register in database
    success_count = 0
    for db in databases:
        # Create and change to database-specific working directory
        # Directory structure: /hive/data/genomes/{db}/bed/hgmd/
        working_dir = "/hive/data/genomes/{}/bed/hgmd".format(db)
        os.makedirs(working_dir, exist_ok=True)
        
        try:
            os.chdir(working_dir)
        except FileNotFoundError:
            print("Error: Directory {} not found".format(working_dir), file=sys.stderr)
            sys.exit(1)
        
        if process_database(year, db, working_dir):
            success_count += 1
        else:
            print("\n✗ Failed to process BigBed for {}".format(db), file=sys.stderr)
    
    # Exit if any BigBed processing failed
    if success_count < len(databases):
        print("\nSome BigBed processing failed. Exiting.", file=sys.stderr)
        sys.exit(1)
    
    # ========================================================================
    # Process transcripts for each database
    # ========================================================================
    # Extract HGMD transcripts and filter gene predictions from ncbiRefSeq
    transcript_success_count = 0
    for db in databases:
        # Find the latest ncbiRefSeq directory for this database
        # hg38: /hive/data/genomes/hg38/bed/ncbiRefSeq.p14.{year}-{date}/
        # hg19: /hive/data/genomes/hg19/bed/ncbiRefSeq.p13.{year}-{date}/
        # Falls back to previous years if specified year not found
        ncbirefseq_source_dir = find_latest_ncbirefseq_dir(db, year)
        if not ncbirefseq_source_dir:
            print("\n✗ Failed to find ncbiRefSeq directory for {}".format(db), file=sys.stderr)
            continue
        
        # Output to hgmd directory
        # Files will be created in: /hive/data/genomes/{db}/bed/hgmd/ncbiRefSeq.p{13|14}.{date}/
        output_dir = "/hive/data/genomes/{}/bed/hgmd".format(db)
        
        if process_transcripts(year, db, ncbirefseq_source_dir, output_dir):
            transcript_success_count += 1
        else:
            print("\n✗ Failed to process transcripts for {}".format(db), file=sys.stderr)
    
    # Exit if any transcript processing failed
    if transcript_success_count < len(databases):
        sys.exit(1)

if __name__ == '__main__':
    main()

# Sample output:

# python3 process_hgmd.py --year 2025 --db hg38
#  hg38 BigBed completed successfully!
# Output files: /hive/data/genomes/hg38/bed/hgmd/hgmd.bed, /hive/data/genomes/hg38/bed/hgmd/hgmd.bb
# Symlink created: /gbdb/hg38/bbi/hgmd.bb
# hgBbiDbLink run: hgBbiDbLink hg38 hgmd /gbdb/hg38/bbi/hgmd.bb
# ✓ hg38 transcript processing completed!
# Output files: /hive/data/genomes/hg38/bed/hgmd/ncbiRefSeq.p14.2025-08-13/hgmdTranscripts.txt, /hive/data/genomes/hg38/bed/hgmd/ncbiRefSeq.p14.2025-08-13/hgmd.curated.gp
# hgLoadGenePred run: hgLoadGenePred -genePredExt hg38 ncbiRefSeqHgmd hgmd.curated.gp

# python3 process_hgmd.py --year 2025 --db hg19
# Note: Using ncbiRefSeq.p13.2024-12-15 directory (year 2025 not found)
#  hg19 BigBed completed successfully!
# Output files: /hive/data/genomes/hg19/bed/hgmd/hgmd.bed, /hive/data/genomes/hg19/bed/hgmd/hgmd.bb
# Symlink created: /gbdb/hg19/bbi/hgmd.bb
# hgBbiDbLink run: hgBbiDbLink hg19 hgmd /gbdb/hg19/bbi/hgmd.bb
# ✓ hg19 transcript processing completed!
# Output files: /hive/data/genomes/hg19/bed/hgmd/ncbiRefSeq.p13.2024-12-15/hgmdTranscripts.txt, /hive/data/genomes/hg19/bed/hgmd/ncbiRefSeq.p13.2024-12-15/hgmd.curated.gp
# hgLoadGenePred run: hgLoadGenePred -genePredExt hg19 ncbiRefSeqHgmd hgmd.curated.gp
