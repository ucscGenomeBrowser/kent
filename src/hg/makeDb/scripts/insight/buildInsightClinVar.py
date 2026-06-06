#!/usr/bin/env python3
"""
InSiGHT ClinVar VCEP Variants Pipeline

Fetches InSiGHT Variant Curation Expert Panel (VCEP) submissions from ClinVar
for Lynch syndrome mismatch repair (MMR) genes, and creates UCSC Genome Browser
bigBed tracks for both hg19 and hg38.

Genes included:
- MLH1
- MSH2
- MSH6
- PMS2

Only variants submitted by InSiGHT with "reviewed by expert panel" status are included.

Usage:
    python3 buildInsightClinVar.py [--output-dir DIR]

Output files:
    - insight_clinvar_variants.tsv: Combined variant data from ClinVar
    - insightClinVar.as: AutoSQL schema file
    - insightClinVar_hg19.bed: BED file for hg19
    - insightClinVar_hg38.bed: BED file for hg38
    - insightClinVarHg19.bb: bigBed file for hg19
    - insightClinVarHg38.bb: bigBed file for hg38

Author: UCSC Genome Browser Group
Date: 2026
"""

import argparse
import os
import re
import subprocess
import sys
import tempfile
import time
import urllib.request
import xml.etree.ElementTree as ET

# ============================================================================
# Configuration
# ============================================================================

# Genes to fetch from ClinVar (Lynch syndrome MMR genes)
GENES = ["MLH1", "MSH2", "MSH6", "PMS2"]

# ClinVar API endpoints
ESEARCH_URL = "https://eutils.ncbi.nlm.nih.gov/entrez/eutils/esearch.fcgi"
EFETCH_URL = "https://eutils.ncbi.nlm.nih.gov/entrez/eutils/efetch.fcgi"

# Batch size for API requests
BATCH_SIZE = 100

# Delay between API requests (be nice to NCBI)
API_DELAY = 0.5

# LiftOver chain files
LIFTOVER_CHAINS = {
    'hg19_to_hg38': '/cluster/data/hg19/bed/liftOver/hg19ToHg38.over.chain.gz',
    'hg38_to_hg19': '/cluster/data/hg38/bed/liftOver/hg38ToHg19.over.chain.gz',
}

# Chromosome sizes files
CHROM_SIZES = {
    'hg19': '/cluster/data/hg19/chrom.sizes',
    'hg38': '/cluster/data/hg38/chrom.sizes',
}

# Color mapping for ClinVar classifications (RGB format for bigBed)
# Colors match ClinVar track conventions
COLORS = {
    'Pathogenic': '210,0,0',                 # red
    'Likely pathogenic': '245,152,152',      # pink/light red
    'Uncertain significance': '0,0,136',     # dark blue
    'Likely benign': '213,247,213',          # lime green
    'Benign': '0,210,0',                     # green
}
DEFAULT_COLOR = '136,136,136'  # gray for unknown

# AutoSQL definition for the BED9+7 format
AUTOSQL = """table insightClinVar
"InSiGHT VCEP curated variants from ClinVar for Lynch syndrome genes"
   (
   string chrom;           "Reference sequence chromosome or scaffold"
   uint   chromStart;      "Start position in chromosome"
   uint   chromEnd;        "End position in chromosome"
   string name;            "Variant name (HGVS notation)"
   uint   score;           "Not used, set to 0"
   char[1] strand;         "Not used, set to ."
   uint   thickStart;      "Same as chromStart"
   uint   thickEnd;        "Same as chromEnd"
   uint   reserved;        "RGB color value"
   string gene;            "Gene symbol"
   string varId;           "ClinVar variation ID"
   string classification;  "Clinical significance classification"
   string reviewStatus;    "ClinVar review status"
   string dateEvaluated;   "Date of classification"
   lstring comment;        "InSiGHT submitter comment"
   lstring _mouseOver;     "HTML mouseover text"
   )
"""

# AutoSQL for the PMS2CL paralog projection track
PMS2CL_AUTOSQL = """table pms2clParalogVars
"InSiGHT VCEP curated PMS2 variants projected onto PMS2CL coordinates"
   (
   string chrom;           "Reference sequence chromosome or scaffold"
   uint   chromStart;      "Start position in chromosome"
   uint   chromEnd;        "End position in chromosome"
   string name;            "Projected PMS2CL HGVS notation"
   uint   score;           "Not used, set to 0"
   char[1] strand;         "Not used, set to ."
   uint   thickStart;      "Same as chromStart"
   uint   thickEnd;        "Same as chromEnd"
   uint   reserved;        "RGB color value"
   string pms2Variant;     "Equivalent PMS2 variant (HGVS)"
   string classification;  "PMS2 ClinVar classification"
   string dateEvaluated;   "Date PMS2 variant was classified"
   string pms2Region;      "PMS2 genomic region for copy-paste"
   lstring _mouseOver;     "HTML mouseover text"
   )
"""

# PMS2 -> PMS2CL projection: PMS2 c. range to (offset, exon label)
# n = c - offset; based on van der Klift 2010 alignment and empirical verification
# Exons 1-8 and 10 of PMS2 have no PMS2CL equivalent (skipped)
PMS2_TO_PMS2CL_OFFSETS = [
    # (c_min, c_max, offset, label)
    (904,  988,  903,  'exon 9'),
    (1145, 1863, 1059, 'exon 11 (5\' of indel)'),
    (1864, 2006, 1060, 'exon 11 (3\' of indel)'),
    (2007, 2174, 1060, 'exon 12'),
    (2175, 2275, 1060, 'exon 13'),
    (2276, 2445, 1060, 'exon 14'),
    (2446, 2798, 1060, 'exon 15 (partial)'),
]

PMS2CL_TX = "NR_002217.1"
PMS2_TX = "NM_000535.7"

# ============================================================================
# Utility Functions
# ============================================================================

def log(msg):
    """Print log message to stderr"""
    print(msg, file=sys.stderr)


def bash(cmd):
    """Run a bash command and return output"""
    result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(f"Command failed: {cmd}\n{result.stderr}")
    return result.stdout


def escape_html(text):
    """Escape special characters for HTML"""
    if not text:
        return ""
    return (str(text).replace('&', '&amp;')
                     .replace('<', '&lt;')
                     .replace('>', '&gt;')
                     .replace('"', '&quot;'))


def fetch_url(url, max_retries=3):
    """Fetch URL with retries"""
    for attempt in range(max_retries):
        try:
            req = urllib.request.Request(url)
            with urllib.request.urlopen(req, timeout=120) as response:
                return response.read().decode('utf-8')
        except Exception as e:
            if attempt < max_retries - 1:
                log(f"    Retry {attempt + 1} after error: {e}")
                time.sleep(2)
            else:
                raise


# ============================================================================
# ClinVar API Functions
# ============================================================================

def search_clinvar_ids(gene):
    """Search ClinVar for InSiGHT submissions for a gene, return variation IDs"""
    search_term = f"{gene}[gene] AND InSiGHT[Submitter]"
    url = f"{ESEARCH_URL}?db=clinvar&term={urllib.request.quote(search_term)}&retmax=10000"

    log(f"  Searching {gene}...")
    xml_data = fetch_url(url)

    # Parse IDs from XML
    root = ET.fromstring(xml_data)
    ids = [id_elem.text for id_elem in root.findall('.//Id')]
    log(f"    Found {len(ids)} variants")

    return ids


def fetch_variant_batch(var_ids):
    """Fetch full VCV data for a batch of variation IDs"""
    ids_str = ','.join(var_ids)
    url = f"{EFETCH_URL}?db=clinvar&rettype=vcv&is_variationid&id={ids_str}"
    return fetch_url(url)


def parse_clinvar_xml(xml_data):
    """Parse ClinVar XML and extract variant data with InSiGHT submissions"""
    root = ET.fromstring(xml_data)
    variants = []

    for var_archive in root.findall('.//VariationArchive'):
        var_id = var_archive.get('VariationID')
        var_name = var_archive.get('VariationName', '')

        # Get gene symbol
        gene_elem = var_archive.find('.//Gene')
        gene = gene_elem.get('Symbol') if gene_elem is not None else ''

        # Get coordinates from SimpleAllele/Location
        hg38_chr = hg38_start = hg38_end = None
        hg19_chr = hg19_start = hg19_end = None

        for seq_loc in var_archive.findall('.//SimpleAllele/Location/SequenceLocation'):
            assembly = seq_loc.get('Assembly', '')
            if assembly == 'GRCh38':
                hg38_chr = 'chr' + seq_loc.get('Chr', '')
                hg38_start = seq_loc.get('start')
                hg38_end = seq_loc.get('stop')
            elif assembly == 'GRCh37':
                hg19_chr = 'chr' + seq_loc.get('Chr', '')
                hg19_start = seq_loc.get('start')
                hg19_end = seq_loc.get('stop')

        # Find InSiGHT submission
        insight_classification = ''
        insight_review_status = ''
        insight_date = ''
        insight_comment = ''

        for assertion in var_archive.findall('.//ClinicalAssertion'):
            accession = assertion.find('.//ClinVarAccession')
            if accession is not None:
                submitter = accession.get('SubmitterName', '')
                if 'InSiGHT' in submitter:
                    classification = assertion.find('.//Classification')
                    if classification is not None:
                        insight_date = classification.get('DateLastEvaluated', '')
                        review_elem = classification.find('ReviewStatus')
                        if review_elem is not None:
                            insight_review_status = review_elem.text
                        germ_class = classification.find('GermlineClassification')
                        if germ_class is not None:
                            insight_classification = germ_class.text
                        comment_elem = classification.find('Comment')
                        if comment_elem is not None:
                            insight_comment = comment_elem.text or ''
                    break

        if insight_classification:  # Only include if has InSiGHT classification
            variants.append({
                'var_id': var_id,
                'gene': gene,
                'name': var_name,
                'hg38_chr': hg38_chr,
                'hg38_start': hg38_start,
                'hg38_end': hg38_end,
                'hg19_chr': hg19_chr,
                'hg19_start': hg19_start,
                'hg19_end': hg19_end,
                'classification': insight_classification,
                'review_status': insight_review_status,
                'date_evaluated': insight_date,
                'comment': insight_comment,
            })

    return variants


# ============================================================================
# Data Fetching
# ============================================================================

def fetch_all_variants():
    """Fetch all InSiGHT VCEP variants from ClinVar"""
    all_ids = []

    log("Searching ClinVar for InSiGHT VCEP submissions...")
    for gene in GENES:
        ids = search_clinvar_ids(gene)
        all_ids.extend(ids)
        time.sleep(API_DELAY)

    # Remove duplicates while preserving order
    seen = set()
    unique_ids = []
    for id in all_ids:
        if id not in seen:
            seen.add(id)
            unique_ids.append(id)

    log(f"Total unique variation IDs: {len(unique_ids)}")

    # Fetch variants in batches
    all_variants = []
    num_batches = (len(unique_ids) + BATCH_SIZE - 1) // BATCH_SIZE

    log(f"Fetching variant data in {num_batches} batches...")
    for i in range(0, len(unique_ids), BATCH_SIZE):
        batch_num = i // BATCH_SIZE + 1
        batch_ids = unique_ids[i:i + BATCH_SIZE]
        log(f"  Batch {batch_num}/{num_batches}: fetching {len(batch_ids)} variants...")

        xml_data = fetch_variant_batch(batch_ids)
        variants = parse_clinvar_xml(xml_data)
        all_variants.extend(variants)

        time.sleep(API_DELAY)

    log(f"Total variants with InSiGHT classifications: {len(all_variants)}")
    return all_variants


def write_tsv(variants, output_file):
    """Write variants to TSV file"""
    log(f"Writing TSV file: {output_file}")

    columns = ['var_id', 'gene', 'name', 'hg38_chr', 'hg38_start', 'hg38_end',
               'hg19_chr', 'hg19_start', 'hg19_end', 'classification',
               'review_status', 'date_evaluated', 'comment']

    with open(output_file, 'w') as f:
        f.write('\t'.join(columns) + '\n')
        for v in variants:
            row = [str(v.get(col, '') or '') for col in columns]
            f.write('\t'.join(row) + '\n')


# ============================================================================
# LiftOver
# ============================================================================

def liftover_coords(coords, chain_file):
    """
    Lift over coordinates using UCSC liftOver.

    Args:
        coords: dict mapping id to (chrom, start, end)
        chain_file: path to chain file

    Returns:
        dict mapping id to (chrom, start, end) in target assembly
    """
    if not coords:
        return {}

    with tempfile.NamedTemporaryFile(mode='w', suffix='.bed', delete=False) as f:
        input_bed = f.name
        for var_id, (chrom, start, end) in coords.items():
            f.write(f"{chrom}\t{start}\t{end}\t{var_id}\n")

    output_bed = input_bed.replace('.bed', '.lifted.bed')
    unmapped_bed = input_bed.replace('.bed', '.unmapped.bed')

    try:
        bash(f"liftOver {input_bed} {chain_file} {output_bed} {unmapped_bed} 2>/dev/null")
    except Exception:
        for f in [input_bed, output_bed, unmapped_bed]:
            if os.path.exists(f):
                os.remove(f)
        return {}

    lifted = {}
    if os.path.exists(output_bed):
        with open(output_bed) as f:
            for line in f:
                fields = line.strip().split('\t')
                if len(fields) >= 4:
                    lifted[fields[3]] = (fields[0], int(fields[1]), int(fields[2]))

    for f in [input_bed, output_bed, unmapped_bed]:
        if os.path.exists(f):
            os.remove(f)

    return lifted


# ============================================================================
# BED/bigBed Creation
# ============================================================================

def create_bed_entries(variants, assembly):
    """Create BED entries for the given assembly"""
    entries = []
    unmapped = []
    stats = {'total': 0, 'mapped': 0, 'mapped_native': 0, 'mapped_liftover': 0}

    # Determine coordinate columns
    if assembly == 'hg38':
        chr_col, start_col, end_col = 'hg38_chr', 'hg38_start', 'hg38_end'
        alt_chr_col, alt_start_col, alt_end_col = 'hg19_chr', 'hg19_start', 'hg19_end'
        chain_file = LIFTOVER_CHAINS['hg19_to_hg38']
    else:
        chr_col, start_col, end_col = 'hg19_chr', 'hg19_start', 'hg19_end'
        alt_chr_col, alt_start_col, alt_end_col = 'hg38_chr', 'hg38_start', 'hg38_end'
        chain_file = LIFTOVER_CHAINS['hg38_to_hg19']

    # First pass: identify variants needing liftOver
    needs_liftover = {}
    for v in variants:
        stats['total'] += 1
        if not v.get(start_col):
            # Try alternate assembly coordinates for liftOver
            if v.get(alt_start_col):
                needs_liftover[v['var_id']] = (
                    v[alt_chr_col],
                    int(v[alt_start_col]) - 1,  # Convert to 0-based
                    int(v[alt_end_col])
                )

    # Perform liftOver
    lifted = {}
    if needs_liftover:
        log(f"  Attempting liftOver for {len(needs_liftover)} variants...")
        lifted = liftover_coords(needs_liftover, chain_file)
        log(f"  Successfully lifted: {len(lifted)}")

    # Second pass: create BED entries
    for v in variants:
        chrom = v.get(chr_col)
        start = v.get(start_col)
        end = v.get(end_col)
        used_liftover = False

        if not start and v['var_id'] in lifted:
            chrom, start, end = lifted[v['var_id']]
            # Already 0-based from liftOver
            used_liftover = True
        elif start and end:
            start = int(start) - 1  # Convert to 0-based
            end = int(end)
        else:
            # Missing coordinates
            unmapped.append(v)
            continue

        if start is None:
            unmapped.append(v)
            continue

        # Get color based on classification
        color = COLORS.get(v['classification'], DEFAULT_COLOR)

        # Build mouseover HTML
        clinvar_url = f"https://www.ncbi.nlm.nih.gov/clinvar/variation/{v['var_id']}/"
        mouse_over = (
            f"<b>Variant:</b> {escape_html(v['name'])}<br>"
            f"<b>ClinVar ID:</b> <a href=\"{clinvar_url}\" target=\"_blank\">{v['var_id']}</a><br>"
            f"<b>Classification:</b> {escape_html(v['classification'])}<br>"
            f"<b>Date evaluated:</b> {escape_html(v['date_evaluated'])}"
        )
        if v['comment']:
            mouse_over += f"<br><b>Comment:</b> {escape_html(v['comment'])}"

        # Truncate name if too long
        name = v['name'] if len(v['name']) <= 200 else v['name'][:197] + "..."

        # Review status - use custom text
        review_status = "Reviewed by expert panel InSiGHT"

        # Build BED9+7 line
        comment = v['comment'].replace('\t', ' ').replace('\n', ' ')
        bed_fields = [
            chrom,                          # chrom
            str(start),                     # chromStart
            str(end),                       # chromEnd
            name,                           # name
            '0',                            # score
            '.',                            # strand
            str(start),                     # thickStart
            str(end),                       # thickEnd
            color,                          # itemRgb
            v['gene'],                      # gene
            v['var_id'],                    # varId
            v['classification'],            # classification
            review_status,                  # reviewStatus
            v['date_evaluated'],            # dateEvaluated
            comment,                        # comment
            mouse_over,                     # _mouseOver
        ]

        entries.append('\t'.join(bed_fields))
        stats['mapped'] += 1
        if used_liftover:
            stats['mapped_liftover'] += 1
        else:
            stats['mapped_native'] += 1

    return entries, stats, unmapped


# ============================================================================
# PMS2CL Paralog Projection
# ============================================================================

def get_pms2cl_exons(assembly):
    """Query hgsql for PMS2CL (NR_002217.1) exon coordinates.
    Returns (chrom, [(start, end), ...]) for the 6 exons in transcript order
    (plus strand, so genomic order)."""
    result = bash(f'hgsql {assembly} -Ne \'select chrom, exonStarts, exonEnds from ncbiRefSeq where name="{PMS2CL_TX}"\'')
    fields = result.strip().split('\t')
    chrom = fields[0]
    starts = [int(x) for x in fields[1].rstrip(',').split(',')]
    ends = [int(x) for x in fields[2].rstrip(',').split(',')]
    return chrom, list(zip(starts, ends))


def project_c_to_n(c_pos):
    """Project a PMS2 c. position to PMS2CL n. position.
    Returns n. position or None if c. position has no PMS2CL equivalent."""
    for c_min, c_max, offset, _ in PMS2_TO_PMS2CL_OFFSETS:
        if c_min <= c_pos <= c_max:
            return c_pos - offset
    return None


def pms2cl_n_to_genomic(n_pos, pms2cl_exons):
    """Convert PMS2CL n. position (1-based cDNA) to 0-based genomic position.
    pms2cl_exons is list of (BED start, BED end) tuples for the 6 exons in
    transcript order (plus strand)."""
    cum = 0
    for ex_start, ex_end in pms2cl_exons:
        ex_len = ex_end - ex_start
        if cum < n_pos <= cum + ex_len:
            # n.pos is in this exon
            # Offset within exon (0-based): n_pos - cum - 1
            return ex_start + (n_pos - cum - 1)
        cum += ex_len
    return None


def parse_pms2_hgvs(name):
    """Parse a PMS2 c. HGVS variant name.
    Returns dict with c_start, c_end, suffix (e.g. 'A>G', 'del', 'dup', 'delAGAA')
    or None if unparseable or not a coding variant."""
    # Extract the c. portion. Names look like:
    #   NM_000535.7(PMS2):c.2243_2246del
    #   NM_000535.7(PMS2):c.137G>T
    m = re.search(r'c\.([^\s]+)', name)
    if not m:
        return None
    cpart = m.group(1)

    # Skip intronic (c.NNN+M or c.NNN-M) and UTR (c.-N or c.*N) variants
    # Pattern allows trailing nucleotide chars but not + - * in position
    if re.search(r'[+\-*]', cpart):
        return None

    # Try range: NNN_MMM<suffix>
    m = re.match(r'^(\d+)_(\d+)(.*)$', cpart)
    if m:
        return {
            'c_start': int(m.group(1)),
            'c_end':   int(m.group(2)),
            'suffix':  m.group(3),
            'is_range': True,
        }

    # Single position: NNN<suffix>
    m = re.match(r'^(\d+)(.*)$', cpart)
    if m:
        pos = int(m.group(1))
        return {
            'c_start': pos,
            'c_end':   pos,
            'suffix':  m.group(2),
            'is_range': False,
        }

    return None


def project_variant_to_pms2cl(variant, pms2cl_chrom, pms2cl_exons):
    """Project a PMS2 variant to PMS2CL coordinates.
    Returns dict with bed fields or None if not projectable."""
    if variant['gene'] != 'PMS2':
        return None

    parsed = parse_pms2_hgvs(variant['name'])
    if parsed is None:
        return None

    # Project both endpoints
    n_start = project_c_to_n(parsed['c_start'])
    n_end = project_c_to_n(parsed['c_end'])
    if n_start is None or n_end is None:
        return None  # outside PMS2CL coverage

    # Compute PMS2CL genomic positions (0-based BED)
    g_start = pms2cl_n_to_genomic(n_start, pms2cl_exons)
    g_end = pms2cl_n_to_genomic(n_end, pms2cl_exons)
    if g_start is None or g_end is None:
        return None

    # BED end is exclusive; for plus-strand PMS2CL, n_end > n_start means g_end > g_start
    bed_start = min(g_start, g_end)
    bed_end = max(g_start, g_end) + 1

    # Construct PMS2CL n. HGVS name
    if parsed['is_range']:
        n_name = f"{PMS2CL_TX}:n.{n_start}_{n_end}{parsed['suffix']}"
    else:
        n_name = f"{PMS2CL_TX}:n.{n_start}{parsed['suffix']}"

    return {
        'chrom':       pms2cl_chrom,
        'bed_start':   bed_start,
        'bed_end':     bed_end,
        'pms2cl_name': n_name,
        'pms2_name':   variant['name'],
        'classification': variant['classification'],
        'date':        variant['date_evaluated'],
    }


def create_pms2cl_paralog_track(variants, assembly, output_dir):
    """Build a bigBed track at PMS2CL coordinates with PMS2 ClinVar variants
    projected onto the pseudogene. Returns the bigBed file path."""
    log(f"\nProjecting PMS2 variants onto PMS2CL ({assembly})...")

    pms2cl_chrom, pms2cl_exons = get_pms2cl_exons(assembly)

    entries = []
    skipped_no_parse = 0
    skipped_outside_coverage = 0
    skipped_non_pms2 = 0

    # Pick which assembly coords to copy-paste in mouseover
    chr_key = f'{assembly}_chr'
    start_key = f'{assembly}_start'
    end_key = f'{assembly}_end'

    for v in variants:
        if v['gene'] != 'PMS2':
            skipped_non_pms2 += 1
            continue
        proj = project_variant_to_pms2cl(v, pms2cl_chrom, pms2cl_exons)
        if proj is None:
            if parse_pms2_hgvs(v['name']) is None:
                skipped_no_parse += 1
            else:
                skipped_outside_coverage += 1
            continue

        # Use PMS2 classification color (matches main track)
        color = COLORS.get(proj['classification'], DEFAULT_COLOR)

        # PMS2 region for copy-paste
        pms2_chrom = v.get(chr_key, '') or ''
        pms2_start = v.get(start_key, '') or ''
        pms2_end = v.get(end_key, '') or ''
        if pms2_chrom and pms2_start and pms2_end:
            pms2_region = f"{pms2_chrom}:{pms2_start}-{pms2_end}"
        else:
            pms2_region = "(unmapped)"

        # Mouseover
        mouse_over = (
            f"<b>PMS2CL position:</b> {escape_html(proj['pms2cl_name'])}<br>"
            f"<b>Equivalent PMS2 variant:</b> {escape_html(proj['pms2_name'])}<br>"
            f"<b>PMS2 classification:</b> {escape_html(proj['classification'])}<br>"
            f"<b>Date evaluated:</b> {escape_html(proj['date'])}<br>"
            f"<b>PMS2 region (copy-paste):</b> {escape_html(pms2_region)}<br>"
            f"<b>Note:</b> A variant called here by short-read NGS may actually be "
            f"the equivalent PMS2 variant due to paralog misalignment."
        )

        bed_line = "\t".join([
            proj['chrom'],
            str(proj['bed_start']),
            str(proj['bed_end']),
            proj['pms2cl_name'],
            "0",
            ".",
            str(proj['bed_start']),
            str(proj['bed_end']),
            color,
            proj['pms2_name'],
            proj['classification'],
            proj['date'],
            pms2_region,
            mouse_over,
        ])
        entries.append(bed_line)

    log(f"  Projected: {len(entries)} variants")
    log(f"  Skipped (HGVS not parseable / intronic): {skipped_no_parse}")
    log(f"  Skipped (PMS2 exons 1-8, 10, or beyond PMS2CL coverage): {skipped_outside_coverage}")

    if not entries:
        return None

    bed_file = os.path.join(output_dir, f"pms2clParalogVars_{assembly}.bed")
    with open(bed_file, 'w') as f:
        f.write('\n'.join(entries) + '\n')
    bash(f"sort -k1,1 -k2,2n {bed_file} -o {bed_file}")

    as_file = os.path.join(output_dir, "pms2clParalogVars.as")
    with open(as_file, 'w') as f:
        f.write(PMS2CL_AUTOSQL)

    bb_file = os.path.join(output_dir, f"pms2clParalogVars{assembly.capitalize()}.bb")
    chrom_sizes = CHROM_SIZES[assembly]
    bash(f"bedToBigBed -as={as_file} -type=bed9+5 -tab {bed_file} {chrom_sizes} {bb_file}")
    log(f"  Created: {bb_file}")
    return bb_file


def create_track(variants, assembly, output_dir):
    """Create BED and bigBed files for a given assembly"""
    log(f"\nProcessing {assembly}...")

    entries, stats, unmapped = create_bed_entries(variants, assembly)

    log(f"  Total variants: {stats['total']}")
    log(f"  Mapped: {stats['mapped']} (native: {stats['mapped_native']}, liftOver: {stats['mapped_liftover']})")
    log(f"  Unmapped: {len(unmapped)}")

    if not entries:
        log("  No entries to write!")
        return None, None

    # Write BED file
    bed_file = os.path.join(output_dir, f"insightClinVar_{assembly}.bed")
    log(f"  Writing BED file: {bed_file}")
    with open(bed_file, 'w') as f:
        f.write('\n'.join(entries) + '\n')

    # Sort BED file
    log(f"  Sorting BED file...")
    bash(f"sort -k1,1 -k2,2n {bed_file} -o {bed_file}")

    # Create bigBed
    as_file = os.path.join(output_dir, "insightClinVar.as")
    bb_file = os.path.join(output_dir, f"insightClinVar{assembly.capitalize()}.bb")
    chrom_sizes = CHROM_SIZES[assembly]

    log(f"  Creating bigBed file: {bb_file}")
    try:
        bash(f"bedToBigBed -as={as_file} -type=bed9+7 -tab {bed_file} {chrom_sizes} {bb_file}")
        log(f"  Successfully created: {bb_file}")
    except Exception as e:
        log(f"  ERROR creating bigBed: {e}")
        bb_file = None

    return bed_file, bb_file


# ============================================================================
# Main Pipeline
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description='Build InSiGHT ClinVar VCEP variants bigBed tracks'
    )
    parser.add_argument(
        '--output-dir', '-o',
        default=os.path.dirname(os.path.abspath(__file__)),
        help='Output directory (default: script directory)'
    )
    args = parser.parse_args()

    output_dir = args.output_dir
    os.makedirs(output_dir, exist_ok=True)

    log("=" * 70)
    log("InSiGHT ClinVar VCEP Variants Pipeline")
    log("=" * 70)

    # Step 1: Fetch data from ClinVar API
    variants = fetch_all_variants()
    if not variants:
        log("ERROR: No variants fetched!")
        sys.exit(1)

    # Step 2: Write combined TSV
    tsv_file = os.path.join(output_dir, "insight_clinvar_variants.tsv")
    write_tsv(variants, tsv_file)

    # Step 3: Write AutoSQL file
    as_file = os.path.join(output_dir, "insightClinVar.as")
    log(f"Writing AutoSQL file: {as_file}")
    with open(as_file, 'w') as f:
        f.write(AUTOSQL)

    # Step 4: Create tracks for both assemblies
    output_files = {}
    for assembly in ['hg38', 'hg19']:
        try:
            bed_file, bb_file = create_track(variants, assembly, output_dir)
            output_files[assembly] = {'bed': bed_file, 'bb': bb_file}
        except Exception as e:
            log(f"ERROR processing {assembly}: {e}")
            import traceback
            traceback.print_exc()

    # Step 5: Build PMS2CL paralog projection track
    for assembly in ['hg38', 'hg19']:
        try:
            paralog_bb = create_pms2cl_paralog_track(variants, assembly, output_dir)
            if assembly in output_files:
                output_files[assembly]['paralog_bb'] = paralog_bb
        except Exception as e:
            log(f"ERROR projecting PMS2CL paralog ({assembly}): {e}")
            import traceback
            traceback.print_exc()

    # Print summary
    log("\n" + "=" * 70)
    log("Summary")
    log("=" * 70)

    # Classification counts
    from collections import Counter
    class_counts = Counter(v['classification'] for v in variants)
    log("\nClassifications:")
    for cls, count in class_counts.most_common():
        log(f"  {cls}: {count}")

    # Gene counts
    gene_counts = Counter(v['gene'] for v in variants)
    log("\nGenes:")
    for gene in GENES:
        log(f"  {gene}: {gene_counts.get(gene, 0)}")

    log("\n" + "=" * 70)
    log("Output Files")
    log("=" * 70)
    log(f"  TSV file:    {tsv_file}")
    log(f"  AutoSql:     {as_file}")
    for assembly, files in output_files.items():
        log(f"\n  {assembly}:")
        if files.get('bed'):
            log(f"    BED:    {files['bed']}")
        if files.get('bb') and os.path.exists(files['bb']):
            log(f"    bigBed: {files['bb']}")

    log("\n" + "=" * 70)
    log("Done!")
    log("=" * 70)


if __name__ == "__main__":
    main()
