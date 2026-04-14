#!/usr/bin/env python3
"""
InSiGHT VCEP Functional Assays Track Generator

Generates UCSC Genome Browser tracks (bigBed format) for functional assay evidence
from published studies on Lynch syndrome MMR genes. Combines data from:

  Paper 1: Drost et al. 2018 (PMID:30504929) - MLH1/MSH2 CIMRA assay
  Paper 2: Drost et al. 2020 (PMID:31965077) - MSH6 CIMRA assay
  Paper 3: Jia et al. 2021 (PMID:33357406)   - MSH2 deep mutational scanning
  Paper 4: Rath et al. 2022 (PMID:36054288)  - MLH1 cell-based functional assay

Each variant is classified according to ACMG PS3/BS3 criteria based on
Tavtigian OddsPath thresholds or Jia LOF score thresholds.

Output: BED9+7 bigBed files for hg38 and hg19.

Supplementary data files (place in same directory as this script):
  drost2018_supplement.pdf - https://pmc.ncbi.nlm.nih.gov/articles/instance/7901556/bin/NIHMS1010100-supplement-CIMRA.pdf
  drost2020_supplement.docx - https://static-content.springer.com/esm/art%3A10.1038%2Fs41436-019-0736-2/MediaObjects/41436_2019_736_MOESM2_ESM.docx
  mmc2.xlsx - https://pmc.ncbi.nlm.nih.gov/articles/instance/7820803/bin/mmc2.xlsx
  rath2022_supplement.pdf - https://pmc.ncbi.nlm.nih.gov/articles/instance/9772141/bin/NIHMS1834139-supplement-supinfo.pdf

Author: Generated for InSiGHT VCEP
Date: 2025
"""

import subprocess
import os
import re
import sys
import zipfile
import xml.etree.ElementTree as ET

try:
    import openpyxl
except ImportError:
    print("ERROR: openpyxl is required. Install with: pip install openpyxl")
    sys.exit(1)

# ============================================================================
# Configuration
# ============================================================================
OUTPUT_DIR = "/hive/users/lrnassar/insightHub/functionalAssays"

# Transcripts for coordinate mapping (current MANE versions in hgsql)
TRANSCRIPTS = {
    'MLH1': 'NM_000249.4',
    'MSH2': 'NM_000251.3',
    'MSH6': 'NM_000179.3',
}

# Paper-specific transcript versions (used in the name field per paper's notation)
PAPER_TRANSCRIPTS = {
    'drost2018': {'MLH1': 'NM_000249.3', 'MSH2': 'NM_000251.2'},
    'drost2020': {'MSH6': 'NM_000179.2'},
    'rath2022':  {'MLH1': 'NM_000249.4'},
}

# Classification colors (RGB) from insight.html
COLORS = {
    'PS3_Strong':     '180,0,0',       # Dark Red
    'PS3_Moderate':   '210,0,0',       # Red
    'PS3_Supporting': '245,152,152',   # Pink
    'Indeterminate':  '128,128,128',   # Gray
    'BS3_Supporting': '213,247,213',   # Lime Green
    'BS3_Strong':     '0,210,0',       # Green
}

# Paper references and PMIDs
PAPERS = {
    'drost2018': {'ref': 'Drost et al., 2018', 'pmid': '30504929'},
    'drost2020': {'ref': 'Drost et al., 2020', 'pmid': '31965077'},
    'jia2021':   {'ref': 'Jia et al., 2021',   'pmid': '33357406'},
    'rath2022':  {'ref': 'Rath et al., 2022',  'pmid': '36054288'},
}

# AutoSQL definition
AUTOSQL = """table insightFunctionalAssays
"Functional assay evidence for Lynch syndrome MMR gene variant classification"
   (
   string chrom;            "Reference sequence chromosome or scaffold"
   uint   chromStart;       "Start position in chromosome"
   uint   chromEnd;         "End position in chromosome"
   string name;             "Variant name (HGVSc or HGVSp notation)"
   uint   score;            "Not used, set to 0"
   char[1] strand;          "Not used, set to ."
   uint   thickStart;       "Same as chromStart"
   uint   thickEnd;         "Same as chromEnd"
   uint   reserved;         "RGB color value"
   string gene;             "Gene symbol"
   string protein;          "Protein change (HGVSp notation)"
   string classification;   "ACMG evidence classification (PS3/BS3)"
   string clinVarId;        "ClinVar variation ID, if available"
   string score_value;      "Functional score (OddsPath or LOF)"
   string paperRef;         "Publication reference"
   lstring _mouseOver;      "HTML mouseover text"
   )
"""

# ============================================================================
# Utility functions
# ============================================================================

def bash(cmd):
    """Run the cmd in bash subprocess"""
    try:
        rawBashOutput = subprocess.run(cmd, check=True, shell=True,
                                       stdout=subprocess.PIPE, universal_newlines=True, stderr=subprocess.STDOUT)
        bashStdoutt = rawBashOutput.stdout
    except subprocess.CalledProcessError as e:
        raise RuntimeError("command '{}' return with error (code {}): {}".format(e.cmd, e.returncode, e.output))
    return(bashStdoutt)

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

def aa_to_genomic(aa_pos, cds_regions):
    """
    Convert a 1-based amino acid position to 0-based genomic coordinates
    for the 3-nucleotide codon span. Only handles + strand genes
    (MLH1, MSH2, MSH6 are all on + strand).
    Returns (genomic_start, genomic_end) or (None, None).
    """
    nt_start = (aa_pos - 1) * 3 + 1  # 1-based CDS nucleotide position
    nt_end = aa_pos * 3

    # Find the genomic positions for the first and last nucleotide of the codon
    first_start, first_end = cds_pos_to_genomic(nt_start, cds_regions, '+')
    last_start, last_end = cds_pos_to_genomic(nt_end, cds_regions, '+')

    if first_start is not None and last_start is not None:
        return first_start, last_end

    return None, None

def parse_hgvsc_pos(hgvsc):
    """
    Parse HGVSc notation to extract the CDS position and span.
    Returns (start_pos, end_pos) as 1-based CDS positions.
    For simple substitutions, start == end.

    Examples:
    - c.4T>A -> (4, 4)
    - c.100G>C -> (100, 100)
    - c.104_105delinsAC -> (104, 105)
    - c.1852_1853delinsGC -> (1852, 1853)
    """
    hgvsc = hgvsc.strip()
    # Simple substitution: c.100G>C
    match = re.match(r'c\.(\d+)[ACGT]>[ACGT]', hgvsc)
    if match:
        pos = int(match.group(1))
        return pos, pos
    # Delins spanning positions: c.104_105delinsAC
    match = re.match(r'c\.(\d+)_(\d+)delins', hgvsc)
    if match:
        return int(match.group(1)), int(match.group(2))
    return None, None

def parse_protein_pos(prot):
    """
    Parse amino acid position from protein notation.
    Handles both p.X###Y and X###Y formats.

    Examples:
    - p.W372R -> 372
    - p.K13T -> 13
    - M1A -> 1
    """
    prot = prot.strip()
    if prot.startswith('p.'):
        prot = prot[2:]
    match = re.match(r'[A-Z*](\d+)', prot)
    if match:
        return int(match.group(1))
    return None

# ============================================================================
# Classification functions
# ============================================================================

def classify_tavtigian(odds_path):
    """
    Classify variant using Tavtigian OddsPath thresholds.
    Used for Papers 1, 2, 4 (CIMRA assays).

    Thresholds:
      PS3_Strong:     > 18.7
      PS3_Moderate:   > 4.3 and <= 18.7
      PS3_Supporting: > 2.08 and <= 4.3
      Indeterminate:  > 0.48 and <= 2.08
      BS3_Supporting: > 0.05 and <= 0.48
      BS3_Strong:     <= 0.05
    """
    try:
        val = float(odds_path)
    except (ValueError, TypeError):
        return None

    if val > 18.7:
        return 'PS3_Strong'
    elif val > 4.3:
        return 'PS3_Moderate'
    elif val > 2.08:
        return 'PS3_Supporting'
    elif val > 0.48:
        return 'Indeterminate'
    elif val > 0.05:
        return 'BS3_Supporting'
    else:
        return 'BS3_Strong'

def classify_jia_lof(lof_score):
    """
    Classify variant using Jia LOF score thresholds.
    Used for Paper 3 (deep mutational scanning).

    Thresholds:
      PS3_Strong:    >= 0.4
      Indeterminate: >= 0 and < 0.4
      BS3_Strong:    < 0
    """
    try:
        val = float(lof_score)
    except (ValueError, TypeError):
        return None

    if val >= 0.4:
        return 'PS3_Strong'
    elif val >= 0:
        return 'Indeterminate'
    else:
        return 'BS3_Strong'

# ============================================================================
# MouseOver builder
# ============================================================================

def build_mouseover(name, protein, classification, clinvar_id, score_val, score_label, paper_key):
    """Build HTML mouseover text for a variant"""
    paper_info = PAPERS[paper_key]
    parts = []
    parts.append(f"<b>HGVSc/HGVSp:</b> {name}")
    parts.append(f"<b>Protein:</b> {protein}")
    parts.append(f"<b>Classification:</b> {classification}")

    if clinvar_id and clinvar_id != 'NA' and clinvar_id != '':
        parts.append(f'<b>ClinVar ID:</b> <a href="https://www.ncbi.nlm.nih.gov/clinvar/variation/{clinvar_id}/" target="_blank">{clinvar_id}</a>')
    else:
        parts.append("<b>ClinVar ID:</b> N/A")

    parts.append(f"<b>{score_label}:</b> {score_val}")
    parts.append(f'<b>Paper:</b> <a href="https://pubmed.ncbi.nlm.nih.gov/{paper_info["pmid"]}/" target="_blank">{paper_info["ref"]}</a>')

    return "</br>".join(parts)

# ============================================================================
# BED entry builder
# ============================================================================

def make_bed_entry(chrom, start, end, name, color, gene, protein,
                   classification, clinvar_id, score_val, paper_ref, mouseover):
    """Create a BED9+7 line"""
    return (f"{chrom}\t{start}\t{end}\t{name}\t0\t.\t{start}\t{end}\t{color}"
            f"\t{gene}\t{protein}\t{classification}\t{clinvar_id}"
            f"\t{score_val}\t{paper_ref}\t{mouseover}")

# ============================================================================
# Data parsing: Paper 1 - Drost et al. 2018
# ============================================================================

def parse_drost2018(filepath):
    """
    Drost et al. 2018 (PMID:30504929) - Supplementary Table S2.
    Source: NIHMS1010100-supplement-CIMRA.pdf
    Genes: MLH1 (NM_000249.3), MSH2 (NM_000251.2)

    Data hardcoded from published supplement for reliability.
    OddsPath values are the CIMRA-based "Odds for Pathogenicity" column
    (Classification Prior-P + CIMRA section). European decimal commas
    converted to periods.

    ClinVar ID exceptions:
      - V470G (c.1409T>G): Paper listed ClinVar 90658, but that ID now resolves
        to V470E (c.1409T>A) — a different nucleotide change at the same position.
        ClinVar appears to have reassigned the ID since publication. Removed to
        avoid linking to the wrong variant.

    Returns list of variant dicts.
    """
    # (gene, dna, protein, clinvar_id, odds_path_cimra)
    DATA = [
        # MLH1 variants (39)
        ('MLH1', 'c.62C>A',              'p.A21E',  '90297',  '23.621'),
        ('MLH1', 'c.62C>T',              'p.A21V',  '90298',  '29.238'),
        ('MLH1', 'c.73A>T',              'p.I25F',  '90343',  '23.919'),
        ('MLH1', 'c.104_105delinsAC',    'p.M35N',  '17105',  '20.835'),
        ('MLH1', 'c.143A>C',             'p.Q48P',  '89739',  '31.923'),
        ('MLH1', 'c.146T>A',             'p.V49E',  '89749',  '7.829'),
        ('MLH1', 'c.191A>G',             'p.N64S',  '89947',  '0.483'),
        ('MLH1', 'c.203T>A',             'p.I68N',  '90008',  '29.607'),
        ('MLH1', 'c.244A>G',             'p.T82A',  '90116',  '47.103'),
        ('MLH1', 'c.245C>T',             'p.T82I',  '90118',  '33.147'),
        ('MLH1', 'c.301G>A',             'p.G101S', '90137',  '44.238'),
        ('MLH1', 'c.332C>T',             'p.A111V', '90171',  '21.908'),
        ('MLH1', 'c.346A>C',             'p.T116P', '',       '17.699'),
        ('MLH1', 'c.382G>C',             'p.A128P', '90199',  '32.734'),
        ('MLH1', 'c.394G>C',             'p.D132H', '17096',  '0.002'),
        ('MLH1', 'c.779T>G',             'p.L260R', '90350',  '18.378'),
        ('MLH1', 'c.793C>A',             'p.R265S', '90380',  '10.580'),
        ('MLH1', 'c.803A>G',             'p.E268G', '90382',  '0.070'),
        ('MLH1', 'c.911A>T',             'p.D304V', '90439',  '33.990'),
        ('MLH1', 'c.918T>A',             'p.N306K', '90441',  '51.428'),
        ('MLH1', 'c.1321G>A',            'p.A441T', '89696',  '0.001'),
        ('MLH1', 'c.1664T>C',            'p.L555P', '89822',  '57.577'),
        ('MLH1', 'c.1733A>G',            'p.E578G', '17090',  '0.003'),
        ('MLH1', 'c.1742C>T',            'p.P581L', '41635',  '0.002'),
        ('MLH1', 'c.1745T>C',            'p.L582P', '89871',  '28.874'),
        ('MLH1', 'c.1799A>G',            'p.E600G', '89890',  '0.002'),
        ('MLH1', 'c.1808C>G',            'p.P603R', '41636',  '0.002'),
        ('MLH1', 'c.1823C>A',            'p.A608D', '89897',  '2.905'),
        ('MLH1', 'c.1853A>C',            'p.K618T', '89906',  '3.377'),
        ('MLH1', 'c.1855G>C',            'p.A619P', '89909',  '2.833'),
        ('MLH1', 'c.1865T>A',            'p.L622H', '29657',  '6.486'),
        ('MLH1', 'c.1943C>T',            'p.P648L', '89953',  '0.521'),
        ('MLH1', 'c.1942C>T',            'p.P648S', '17097',  '3.507'),
        ('MLH1', 'c.1976G>T',            'p.R659L', '89966',  '4.507'),
        ('MLH1', 'c.2038T>C',            'p.C680R', '90006',  '37.579'),
        ('MLH1', 'c.2041G>A',            'p.A681T', '17099',  '0.031'),
        ('MLH1', 'c.2101C>A',            'p.Q701K', '90042',  '0.034'),
        ('MLH1', 'c.2128A>G',            'p.N710D', '',       '0.160'),
        ('MLH1', 'c.2263A>G',            'p.R755G', '219292', '68.636'),
        # MSH2 variants (35)
        ('MSH2', 'c.23C>T',              'p.T8M',   '90964',  '0.166'),
        ('MSH2', 'c.277C>T',             'p.L93F',  '91044',  '0.554'),
        ('MSH2', 'c.287G>A',             'p.R96H',  '91053',  '0.051'),
        ('MSH2', 'c.317G>A',             'p.R106K', '91062',  '0.013'),
        ('MSH2', 'c.488T>A',             'p.V163D', '91107',  '89.331'),
        ('MSH2', 'c.488T>G',             'p.V163G', '91108',  '79.790'),
        ('MSH2', 'c.493T>G',             'p.Y165D', '91111',  '81.818'),
        ('MSH2', 'c.505A>G',             'p.I169V', '91115',  '0.000'),
        ('MSH2', 'c.560T>G',             'p.L187R', '91135',  '59.040'),
        ('MSH2', 'c.595T>C',             'p.C199R', '91146',  '105.160'),
        ('MSH2', 'c.599T>A',             'p.V200D', '91148',  '89.331'),
        ('MSH2', 'c.929T>C',             'p.L310P', '91245',  '55.450'),
        ('MSH2', 'c.989T>C',             'p.L330P', '91270',  '81.818'),
        ('MSH2', 'c.991A>G',             'p.N331D', '91271',  '0.125'),
        ('MSH2', 'c.1022T>C',            'p.L341P', '90507',  '42.073'),
        ('MSH2', 'c.1046C>G',            'p.P349R', '90513',  '80.798'),
        ('MSH2', 'c.1077A>T',            'p.R359S', '90539',  '62.080'),
        ('MSH2', 'c.1168C>T',            'p.L390F', '41641',  '0.003'),
        ('MSH2', 'c.1255C>A',            'p.Q419K', '90583',  '0.002'),
        ('MSH2', 'c.1319T>C',            'p.L440P', '90625',  '33.990'),
        ('MSH2', 'c.1409T>G',            'p.V470G', '',       '83.898'),  # Paper listed ClinVar 90658, but that ID now points to V470E (c.1409T>A); removed
        ('MSH2', 'c.1571G>T',            'p.R524L', '90701',  '0.179'),
        ('MSH2', 'c.1690A>G',            'p.T564A', '90750',  '0.001'),
        ('MSH2', 'c.1730T>C',            'p.I577T', '41644',  '0.323'),
        ('MSH2', 'c.1808A>G',            'p.D603G', '90791',  '66.934'),
        ('MSH2', 'c.1955C>A',            'p.P652H', '90823',  '0.035'),
        ('MSH2', 'c.1963G>A',            'p.V655I', '127637', '0.002'),
        ('MSH2', 'c.2021G>A',            'p.G674D', '90861',  '50.787'),
        ('MSH2', 'c.2023A>G',            'p.K675E', '408453', '54.075'),
        ('MSH2', 'c.2047G>A',            'p.G683R', '90868',  '74.003'),
        ('MSH2', 'c.2063T>G',            'p.M688R', '90874',  '77.813'),
        ('MSH2', 'c.2087C>T',            'p.P696L', '90881',  '18.845'),
        ('MSH2', 'c.2276G>A',            'p.G759E', '41866',  '2.661'),
        ('MSH2', 'c.2287G>C',            'p.A763P', '',       '35.294'),
        ('MSH2', 'c.2500G>A',            'p.A834T', '90986',  '0.004'),
    ]

    print(f"  Loading Drost 2018 data (hardcoded from Supplementary Table S2)...")
    variants = []
    for gene, dna, protein, clinvar_id, odds_path in DATA:
        transcript = PAPER_TRANSCRIPTS['drost2018'][gene]
        variants.append({
            'gene': gene,
            'dna': dna,
            'protein': protein,
            'clinvar_id': clinvar_id,
            'odds_path': odds_path,
            'name': f"{transcript}:{dna}",
            'paper': 'drost2018',
            'coord_type': 'cds',
        })

    print(f"    Loaded {len(variants)} variants from Drost 2018")
    return variants

# ============================================================================
# Data parsing: Paper 2 - Drost et al. 2020
# ============================================================================

def get_docx_tables(docx_path):
    """Extract tables from a DOCX file using XML parsing"""
    ns = {'w': 'http://schemas.openxmlformats.org/wordprocessingml/2006/main'}

    with zipfile.ZipFile(docx_path, 'r') as z:
        xml_content = z.read('word/document.xml')

    root = ET.fromstring(xml_content)
    tables = root.findall('.//w:tbl', ns)

    result = []
    for table in tables:
        rows = table.findall('.//w:tr', ns)
        table_data = []
        for row in rows:
            cells = row.findall('.//w:tc', ns)
            row_data = []
            for cell in cells:
                texts = []
                for p in cell.findall('.//w:t', ns):
                    if p.text:
                        texts.append(p.text)
                row_data.append(''.join(texts))
            table_data.append(row_data)
        result.append(table_data)

    return result

def parse_drost2020(filepath):
    """
    Parse Drost 2020 supplementary DOCX (Tables S1, S3, S5).
    Gene: MSH6 (NM_000179.2 for S1/S3, protein-only for S5)
    Returns list of variant dicts.

    Table structure (from DOCX XML):
      Table 0 = S1 (31 calibration variants): DNA col 0, Protein col 1, ClinVar col 2, OddsPath col 10
      Table 2 = S3 (18 additional variants):  DNA col 0, Protein col 1, ClinVar col 2, OddsPath col 8
      Table 4 = S5 (38 screen variants):      Protein col 0, OddsPath col 4

    Errata corrections:
      - p.V509Ag (c.1526T>C) in S1: Trailing 'g' is a typo in the published
        DOCX. ClinVar 41588 confirms the correct notation is p.Val509Ala
        (p.V509A). Corrected during parsing.
    """
    if not os.path.exists(filepath):
        print(f"  WARNING: {filepath} not found, skipping Drost 2020")
        return []

    print(f"  Parsing {filepath}...")
    tables = get_docx_tables(filepath)
    transcript = PAPER_TRANSCRIPTS['drost2020']['MSH6']

    variants = []

    # Table 0 = S1: 31 calibration variants (rows 2-32, skipping header rows 0-1)
    if len(tables) > 0:
        table_s1 = tables[0]
        for row in table_s1[2:]:  # Skip 2 header rows
            if len(row) < 11:
                continue
            dna = row[0].strip()
            protein = row[1].strip()
            clinvar_id = row[2].strip()
            odds_path = row[10].strip()

            if not dna.startswith('c.'):
                continue
            if clinvar_id == 'NA':
                clinvar_id = ''
            # Fix known typos in published DOCX: trailing 'g' on protein names
            # is a formatting artifact (e.g., p.V509Ag, p.E220Dg, p.E1163Vg, p.R1304Kg)
            # ClinVar confirms correct forms without trailing 'g'
            protein = re.sub(r'^(p\.[A-Z]\d+[A-Z])g$', r'\1', protein)

            variants.append({
                'gene': 'MSH6',
                'dna': dna,
                'protein': protein,
                'clinvar_id': clinvar_id,
                'odds_path': odds_path,
                'name': f"{transcript}:{dna}",
                'paper': 'drost2020',
                'coord_type': 'cds',
            })
        print(f"    S1: {len([v for v in variants if v['coord_type'] == 'cds'])} variants")

    # Table 2 = S3: 18 additional variants (rows 2-19, skipping header rows 0-1)
    s3_count = 0
    if len(tables) > 2:
        table_s3 = tables[2]
        for row in table_s3[2:]:  # Skip 2 header rows
            if len(row) < 9:
                continue
            dna = row[0].strip()
            protein = row[1].strip()
            clinvar_id = row[2].strip()
            odds_path = row[8].strip()

            if not dna.startswith('c.'):
                continue
            if clinvar_id == 'NA':
                clinvar_id = ''

            variants.append({
                'gene': 'MSH6',
                'dna': dna,
                'protein': protein,
                'clinvar_id': clinvar_id,
                'odds_path': odds_path,
                'name': f"{transcript}:{dna}",
                'paper': 'drost2020',
                'coord_type': 'cds',
            })
            s3_count += 1
        print(f"    S3: {s3_count} variants")

    # Table 4 = S5: 38 genetic screen variants (rows 2-39, skipping header rows 0-1)
    s5_count = 0
    if len(tables) > 4:
        table_s5 = tables[4]
        for row in table_s5[2:]:  # Skip 2 header rows
            if len(row) < 5:
                continue
            protein = row[0].strip()
            odds_path = row[4].strip()

            if not protein.startswith('p.'):
                continue

            variants.append({
                'gene': 'MSH6',
                'dna': '',
                'protein': protein,
                'clinvar_id': '',
                'odds_path': odds_path,
                'name': protein,
                'paper': 'drost2020',
                'coord_type': 'protein',
            })
            s5_count += 1
        print(f"    S5: {s5_count} variants")

    print(f"    Total: {len(variants)} variants from Drost 2020")
    return variants

# ============================================================================
# Data parsing: Paper 3 - Jia et al. 2021
# ============================================================================

def parse_jia2021(filepath):
    """
    Parse Jia 2021 supplementary XLSX (TableS5_AllVariants).
    Gene: MSH2 (NM_000251, mapped using NM_000251.3)
    Returns list of variant dicts.

    TableS5 columns: Variant (col 0), Position (col 1), LOF score (col 2)
    Variants in compact notation like M1A, need p. prefix.
    Skip variants with LOF score = None.
    """
    if not os.path.exists(filepath):
        print(f"  WARNING: {filepath} not found, skipping Jia 2021")
        return []

    print(f"  Parsing {filepath}...")
    wb = openpyxl.load_workbook(filepath, read_only=True)

    variants = []
    ws = wb['TableS5_AllVariants']

    skipped_none = 0
    for i, row in enumerate(ws.iter_rows(min_row=2, values_only=True)):
        vals = list(row)
        if len(vals) < 3:
            continue

        variant_name = vals[0]
        position = vals[1]
        lof_score = vals[2]

        if variant_name is None:
            continue
        if lof_score is None:
            skipped_none += 1
            continue

        variant_str = str(variant_name).strip()
        protein = f"p.{variant_str}"

        # Use Position column if available, otherwise parse from variant name
        if position is not None:
            aa_pos = int(position)
        else:
            aa_pos = parse_protein_pos(variant_str)

        if aa_pos is None:
            continue

        variants.append({
            'gene': 'MSH2',
            'dna': '',
            'protein': protein,
            'clinvar_id': '',
            'lof_score': str(lof_score),
            'name': protein,
            'paper': 'jia2021',
            'coord_type': 'protein',
            'aa_pos': aa_pos,
        })

    wb.close()
    print(f"    Parsed {len(variants)} variants from Jia 2021 (skipped {skipped_none} with no LOF score)")
    return variants

# ============================================================================
# Data parsing: Paper 4 - Rath et al. 2022
# ============================================================================

def parse_rath2022(filepath):
    """
    Rath et al. 2022 (PMID:36054288) - Supplementary Table S1.
    Source: NIHMS1834139-supplement-supinfo.pdf
    Gene: MLH1 (NM_000249.4 - MANE transcript)

    Data hardcoded from published supplement for reliability.
    OddsPath_Non-functional column. Variants with "-" (no OddsPath) are
    excluded. Scientific notation values (e.g. 1.08E-7) preserved as strings.
    "1,450" = 1450 (American thousands separator, not European decimal).

    ClinVar ID exceptions:
      - K618A (c.1852_1853delinsGC): Paper listed ClinVar 32128, which is a
        ClinVar measure_id (allele-level), not a variation_id (VCV). The
        /clinvar/variation/32128/ URL returns 404. Updated to VCV 17089, which
        is the current ClinVar variation record for this variant
        (NM_000249.4:c.1852_1853delinsGC, p.Lys618Ala).

    Returns list of variant dicts.
    """
    # (protein_short, dna, clinvar_id, odds_path_non_functional)
    # Protein short is the cell line name = protein change without p. prefix
    DATA = [
        # Benign (9 with OddsPath, 2 skipped: I219V, H718Y have "-")
        ('D132H', 'c.394G>C',             '17096',  '1.08E-7'),
        ('V213M', 'c.637G>A',             '41640',  '5.76E-4'),
        ('E268G', 'c.803A>G',             '90382',  '2.24E-4'),
        ('S406N', 'c.1217G>A',            '41634',  '1.49E-10'),
        ('E578G', 'c.1733A>G',            '17090',  '1.44E-6'),
        ('K618A', 'c.1852_1853delinsGC',  '17089',  '2.90E-13'),  # Paper listed measure_id 32128 (404); updated to VCV 17089
        ('Q689R', 'c.2066A>G',            '90016',  '1.11E-4'),
        ('S692P', 'c.2074T>C',            '127621', '4.02E-4'),
        ('V716M', 'c.2146G>A',            '41639',  '4.07E-5'),
        # VUS (6 with OddsPath, 15 skipped with "-")
        ('C39R',  'c.115T>C',             '89655',  '71.4'),
        ('D63N',  'c.187G>A',             '89920',  '0.00'),
        ('N64S',  'c.191A>G',             '89947',  '5.00E-2'),
        ('L73P',  'c.218T>C',             '90086',  '13.7'),
        ('G454R', 'c.1360G>C',            '89705',  '1.11E-2'),
        ('R474P', 'c.1421G>C',            '',       '0.489'),
        # Pathogenic (11 with OddsPath)
        ('I19F',  'c.55A>T',              '90274',  '346'),
        ('D41H',  'c.121G>C',             '89682',  '3.99E+4'),
        ('S44F',  'c.131C>T',             '17079',  '2.46E+4'),
        ('G67R',  'c.199G>A',             '89992',  '4.27E+4'),
        ('R100P', 'c.299G>C',             '90133',  '1450'),
        ('I107R', 'c.320T>G',             '90167',  '210'),
        ('T117M', 'c.350C>T',             '17094',  '1.33E+18'),
        ('S247P', 'c.739T>C',             '90342',  '105'),
        ('R265S', 'c.793C>A',             '90380',  '37.2'),
        ('I276R', 'c.827T>G',             '520540', '319'),
        ('R687W', 'c.2059C>T',            '90014',  '1.74E+4'),
    ]

    print(f"  Loading Rath 2022 data (hardcoded from Supplementary Table S1)...")
    transcript = PAPER_TRANSCRIPTS['rath2022']['MLH1']

    variants = []
    for protein_short, dna, clinvar_id, odds_path in DATA:
        protein = f"p.{protein_short}"
        variants.append({
            'gene': 'MLH1',
            'dna': dna,
            'protein': protein,
            'clinvar_id': clinvar_id,
            'odds_path': odds_path,
            'name': f"{transcript}:{dna}",
            'paper': 'rath2022',
            'coord_type': 'cds',
        })

    print(f"    Loaded {len(variants)} variants from Rath 2022")
    return variants

# ============================================================================
# BED generation
# ============================================================================

def process_variants(variants, db, transcript_cache, stats):
    """
    Convert parsed variants to BED entries for a given genome assembly.
    Returns list of BED line strings.
    """
    bed_entries = []

    for var in variants:
        gene = var['gene']
        paper = var['paper']
        paper_info = PAPERS[paper]

        # Get transcript info (cached)
        tx_accession = TRANSCRIPTS[gene]
        cache_key = f"{db}_{tx_accession}"
        if cache_key not in transcript_cache:
            transcript_cache[cache_key] = get_transcript_info(db, tx_accession)
            transcript_cache[f"{cache_key}_cds"] = build_cds_regions(transcript_cache[cache_key])

        tx = transcript_cache[cache_key]
        cds_regions = transcript_cache[f"{cache_key}_cds"]
        chrom = tx['chrom']
        strand = tx['strand']

        # Determine genomic coordinates based on coordinate type
        if var['coord_type'] == 'cds':
            cds_start, cds_end = parse_hgvsc_pos(var['dna'])
            if cds_start is None:
                stats['parse_failed'] += 1
                continue
            genomic_start, _ = cds_pos_to_genomic(cds_start, cds_regions, strand)
            _, genomic_end = cds_pos_to_genomic(cds_end, cds_regions, strand)
        elif var['coord_type'] == 'protein':
            aa_pos = var.get('aa_pos') or parse_protein_pos(var['protein'])
            if aa_pos is None:
                stats['parse_failed'] += 1
                continue
            genomic_start, genomic_end = aa_to_genomic(aa_pos, cds_regions)
        else:
            stats['parse_failed'] += 1
            continue

        if genomic_start is None:
            stats['coord_failed'] += 1
            continue

        # Classify variant
        if paper == 'jia2021':
            classification = classify_jia_lof(var['lof_score'])
            score_val = var['lof_score']
            score_label = 'LOF'
        else:
            classification = classify_tavtigian(var['odds_path'])
            score_val = var['odds_path']
            score_label = 'OddsPath'

        if classification is None:
            stats['classify_failed'] += 1
            continue

        color = COLORS[classification]
        name = var['name']
        protein = var['protein']
        clinvar_id = var.get('clinvar_id', '')
        paper_ref = paper_info['ref']

        # Build mouseover
        mouseover = build_mouseover(name, protein, classification,
                                     clinvar_id, score_val, score_label, paper)

        bed_line = make_bed_entry(chrom, genomic_start, genomic_end, name,
                                  color, gene, protein, classification,
                                  clinvar_id, score_val, paper_ref, mouseover)
        bed_entries.append(bed_line)
        stats['included'] += 1
        stats[f'included_{paper}'] = stats.get(f'included_{paper}', 0) + 1

    return bed_entries

def create_track(db):
    """Create BED and bigBed files for a given genome assembly"""
    print(f"\n{'='*70}")
    print(f"Processing {db}")
    print(f"{'='*70}")

    stats = {
        'included': 0,
        'parse_failed': 0,
        'coord_failed': 0,
        'classify_failed': 0,
    }

    transcript_cache = {}

    # Parse all papers
    all_variants = []

    # Paper 1: Drost 2018
    print("\n  Paper 1: Drost et al. 2018")
    drost2018_file = os.path.join(OUTPUT_DIR, "drost2018_supplement.pdf")
    all_variants.extend(parse_drost2018(drost2018_file))

    # Paper 2: Drost 2020
    print("\n  Paper 2: Drost et al. 2020")
    drost2020_file = os.path.join(OUTPUT_DIR, "drost2020_supplement.docx")
    all_variants.extend(parse_drost2020(drost2020_file))

    # Paper 3: Jia 2021
    print("\n  Paper 3: Jia et al. 2021")
    jia2021_file = os.path.join(OUTPUT_DIR, "mmc2.xlsx")
    all_variants.extend(parse_jia2021(jia2021_file))

    # Paper 4: Rath 2022
    print("\n  Paper 4: Rath et al. 2022")
    rath2022_file = os.path.join(OUTPUT_DIR, "rath2022_supplement.pdf")
    all_variants.extend(parse_rath2022(rath2022_file))

    print(f"\n  Total variants to process: {len(all_variants)}")

    # Convert to BED entries
    print(f"\n  Querying transcript coordinates from {db}...")
    bed_entries = process_variants(all_variants, db, transcript_cache, stats)

    # Write BED file
    bed_file = os.path.join(OUTPUT_DIR, f"insightFunctionalAssays_{db}.bed")
    print(f"\n  Writing BED file: {bed_file}")
    with open(bed_file, 'w') as f:
        f.write('\n'.join(bed_entries) + '\n')

    # Sort BED file
    print("  Sorting BED file...")
    bash(f"sort -k1,1 -k2,2n {bed_file} -o {bed_file}")

    # Create bigBed
    as_file = os.path.join(OUTPUT_DIR, "insightFunctionalAssays.as")
    db_label = db.replace('hg', 'Hg')
    bb_file = os.path.join(OUTPUT_DIR, f"insightFunctionalAssays{db_label}.bb")
    chrom_sizes = f"/cluster/data/{db}/chrom.sizes"

    print(f"  Creating bigBed file: {bb_file}")
    try:
        bash(f"bedToBigBed -as={as_file} -type=bed9+7 -tab {bed_file} {chrom_sizes} {bb_file}")
        print(f"    Successfully created: {bb_file}")
    except Exception as e:
        print(f"    ERROR creating bigBed: {e}")

    # Print stats
    print(f"\n  Statistics for {db}:")
    print(f"    Total included: {stats['included']}")
    for paper_key in ['drost2018', 'drost2020', 'jia2021', 'rath2022']:
        count = stats.get(f'included_{paper_key}', 0)
        if count > 0:
            print(f"      {PAPERS[paper_key]['ref']}: {count}")
    print(f"    Parse failed: {stats['parse_failed']}")
    print(f"    Coordinate failed: {stats['coord_failed']}")
    print(f"    Classify failed: {stats['classify_failed']}")

    return bed_file, bb_file

# ============================================================================
# Main
# ============================================================================

def main():
    print("=" * 70)
    print("InSiGHT VCEP Functional Assays Track Generator")
    print("=" * 70)

    # Create output directory if needed
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    # Write AutoSql file
    as_file = os.path.join(OUTPUT_DIR, "insightFunctionalAssays.as")
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
