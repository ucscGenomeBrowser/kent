#!/usr/bin/env python3
"""
B.7 - Cardiomyopathy VCEP EvRepo curated variants track builder.

Renders ClinGen Cardiomyopathy VCEP variants from EvRepo (the authoritative source for
VCEP-curated classifications with full ACMG/AMP rule applications). Currently 25 variants,
all MYH7. Otto cron at Phase E re-pulls weekly to surface new curations.

EvRepo JSON schema (per A.2):
  variantInterpretations: [{
    caid: 'CAR:CA012732',
    condition: {label, @id (MONDO)},
    gene: {label, NCBI_id},
    hgvs: [list of HGVS strings - includes NC_000NN.11 (hg38) and NC_000NN.10/.8 (hg19)],
    publishedDate,
    variationId (ClinVar VCV),
    guidelines: [{
      outcome: {label: 'Pathogenic'|'Likely Pathogenic'|'Uncertain Significance'|...},
      agents: [{evidenceCodes: [{label, status: 'Met'|'Not Met'}]}]
    }]
  }]

Outputs:
  cmpVCEPEvRepo/cmpVCEPEvRepo.as
  cmpVCEPEvRepo/cmpVCEPEvRepoHg38.bed + .bb
  cmpVCEPEvRepo/cmpVCEPEvRepoHg19.bed + .bb

Usage:
  python3 cmpVCEPEvRepo.py --db hg38 --db hg19 \
      --output-dir /hive/users/lrnassar/claude/RM37446
"""

import argparse, json, os, re, subprocess, sys

EVREPO_JSON = '/hive/users/lrnassar/claude/RM37446/cmp_downloads/erepo/cardiomyopathyVCEP_classifications.json'

# NC_000xxx accession -> (chromosome, assembly) - covers our 7 chromosomes of interest
NC_ACCESSIONS = {
    # hg38 (GRCh38)
    'NC_000001.11':  ('chr1',  'hg38'),
    'NC_000003.12':  ('chr3',  'hg38'),
    'NC_000011.10':  ('chr11', 'hg38'),
    'NC_000012.12':  ('chr12', 'hg38'),
    'NC_000014.9':   ('chr14', 'hg38'),
    'NC_000015.10':  ('chr15', 'hg38'),
    'NC_000019.10':  ('chr19', 'hg38'),
    # hg19 (GRCh37)
    'NC_000001.10':  ('chr1',  'hg19'),
    'NC_000003.11':  ('chr3',  'hg19'),
    'NC_000011.9':   ('chr11', 'hg19'),
    'NC_000012.11':  ('chr12', 'hg19'),
    'NC_000014.8':   ('chr14', 'hg19'),
    'NC_000015.9':   ('chr15', 'hg19'),
    'NC_000019.9':   ('chr19', 'hg19'),
}

# MANE Select RefSeq accession per gene (for HGVS transcript-version normalization, item F).
MANE_TSV = '/hive/users/lrnassar/claude/RM37446/cmp_downloads/mane_8genes.tsv'
HGVSTOVCF = '/cluster/bin/x86_64/hgvsToVcf'


def load_mane_acc():
    acc = {}
    for line in open(MANE_TSV):
        f = line.rstrip('\n').split('\t')
        if len(f) < 7 or f[0] == 'geneSym':
            continue
        acc[f[0]] = f[6]   # geneSym -> refSeqAcc (e.g. NM_000257.4)
    return acc

MANE_ACC = load_mane_acc()


def normalize_hgvs_version(hgvs, gene):
    """Item F: rewrite the displayed transcript version to the gene's MANE Select version
    (e.g. NM_000257.3(...) -> NM_000257.4(...)). Coordinate-neutral display fix."""
    mane = MANE_ACC.get(gene)
    if not mane or '.' not in mane or not hgvs:
        return hgvs
    base = mane.rsplit('.', 1)[0]
    return re.sub(re.escape(base) + r'\.\d+', mane, hgvs)


def coords_via_hgvstovcf(hgvs_list):
    """Item K fallback: when an entry has NO parseable NC_ genomic HGVS (e.g. repeat-notation
    deletions like c.2785_2787GAG[2]), derive coords from the NM_ c.HGVS via hgvsToVcf.
    Rewrites repeat notation 'c.A_BXXX[n]' -> 'c.A_Bdel' (single-unit contraction).
    Returns {'hg38': (chrom,s,e) or None, 'hg19': (...)}."""
    cterm = next((h.split('(')[0].split(' ')[0] for h in hgvs_list
                  if h.startswith('NM_') and ':c.' in h and '[' in h), '')
    if not cterm:
        cterm = next((h.split('(')[0].split(' ')[0] for h in hgvs_list
                      if h.startswith('NM_') and ':c.' in h), '')
    if not cterm:
        return {'hg38': None, 'hg19': None}
    # repeat notation -> explicit deletion of the unit range
    cterm = re.sub(r'(:c\.\d+_\d+)[ACGT]+\[\d+\]', r'\1del', cterm)
    cterm = re.sub(r'(:c\.)(\d+)([ACGT]+)\[\d+\]', r'\1\2_del', cterm)  # rare single-pos form
    out = {'hg38': None, 'hg19': None}
    for db in ('hg38', 'hg19'):
        try:
            res = subprocess.run([HGVSTOVCF, db, '/dev/stdin', 'stdout'], input=cterm + '\n',
                                 text=True, capture_output=True, check=True)
        except subprocess.CalledProcessError:
            continue
        for line in res.stdout.splitlines():
            if line.startswith('#'):
                continue
            f = line.split('\t')
            if len(f) < 5 or not f[1].isdigit():
                continue
            pos, ref = int(f[1]), f[3]
            # deletion span in BED half-open: drop the anchor base
            out[db] = (f[0], pos, pos + len(ref) - 1)
            break
    return out

# ACMG color palette (matches TP53 + InSiGHT convention)
COLORS = {
    'Pathogenic':              '210,0,0',     # dark red
    'Likely Pathogenic':       '245,152,152', # light pink
    'Uncertain Significance':  '0,0,136',     # dark blue
    'Likely Benign':           '213,247,213', # lime green
    'Benign':                  '0,210,0',     # green
}
DEFAULT_COLOR = '136,136,136'

# Filter values for trackDb filterValues.codeMet field - generated from data at build time.

CHROM_SIZES = {
    'hg38': '/cluster/data/hg38/chrom.sizes',
    'hg19': '/cluster/data/hg19/chrom.sizes',
}

AUTOSQL = """table cmpVCEPEvRepo
"ClinGen Cardiomyopathy VCEP curated variants from EvRepo (Final, with ACMG codes)"
    (
    string  chrom;          "Chromosome"
    uint    chromStart;     "Start position"
    uint    chromEnd;       "End position"
    string  name;           "Display name (gene + protein change)"
    uint    score;          "Score (always 0)"
    char[1] strand;         "Strand (always +)"
    uint    thickStart;     "Same as chromStart"
    uint    thickEnd;       "Same as chromEnd"
    uint    itemRgb;        "Display color"
    string  gene;           "Gene symbol"
    string  hgvsCdna;       "HGVS cDNA (canonical NM_)"
    string  hgvsProtein;    "HGVS protein (p.)"
    string  classification; "VCEP classification"
    string  condition;      "Disease condition (HCM, DCM, etc.)"
    string  codesMet;       "ACMG codes Met (semicolon-separated)"
    string  codesAll;       "ACMG codes considered (Met + Not Met, semicolon-separated)"
    string  publishedDate;  "Publication date in EvRepo"
    string  caid;           "ClinGen Canonical Allele ID"
    string  variationId;    "ClinVar VariationID"
    string  evrepoUrl;      "Link to EvRepo entry"
    lstring _mouseOver;     "Tooltip HTML"
    )
"""

# HGVS regex (per InSiGHT lesson - catch DOCX-style typos like trailing 'g')
HGVS_GENOMIC_RE = re.compile(r'^(NC_\d+\.\d+):g\.(\d+)([ACGT]+)>([ACGT]+)$')
HGVS_GENOMIC_DEL_RE = re.compile(r'^(NC_\d+\.\d+):g\.(\d+)(?:_(\d+))?del')
HGVS_GENOMIC_INS_RE = re.compile(r'^(NC_\d+\.\d+):g\.(\d+)(?:_(\d+))?(?:ins|dup)([ACGT]*)')


def parse_genomic_hgvs(hgvs):
    """Parse NC_000NN.XX:g.POS{REF>ALT,del,ins,...} -> (chrom, assembly, start_bed, end_bed).
    Returns None if not parseable or assembly not in NC_ACCESSIONS.
    Uses BED half-open semantics (0-based start, exclusive end).
    """
    m = HGVS_GENOMIC_RE.match(hgvs)
    if m:
        acc, pos, ref, alt = m.group(1), int(m.group(2)), m.group(3), m.group(4)
        if acc not in NC_ACCESSIONS:
            return None
        chrom, asm = NC_ACCESSIONS[acc]
        return chrom, asm, pos - 1, pos - 1 + len(ref)
    m = HGVS_GENOMIC_DEL_RE.match(hgvs)
    if m:
        acc = m.group(1)
        if acc not in NC_ACCESSIONS:
            return None
        chrom, asm = NC_ACCESSIONS[acc]
        s = int(m.group(2)) - 1
        e = int(m.group(3)) if m.group(3) else int(m.group(2))
        return chrom, asm, s, e
    m = HGVS_GENOMIC_INS_RE.match(hgvs)
    if m:
        acc = m.group(1)
        if acc not in NC_ACCESSIONS:
            return None
        chrom, asm = NC_ACCESSIONS[acc]
        s = int(m.group(2)) - 1
        e = s + 1  # insertion point
        return chrom, asm, s, e
    return None


def extract_record(v):
    """Parse one variantInterpretation into a dict per assembly."""
    gene = v['gene']['label']
    classification = v['guidelines'][0]['outcome']['label']
    condition = v['condition']['label']
    caid = v['caid']
    variation_id = v.get('variationId', '')
    published = v.get('publishedDate', '')

    # Evidence codes (filter to Met). Per D.1 audit fix:
    # Some legacy EvRepo entries (pre-2019) lack Not-Met enumeration in source JSON; for those,
    # codes_all == codes_met. Detect and label accordingly so the field is interpretable.
    all_codes = v['guidelines'][0]['agents'][0]['evidenceCodes']
    codes_met = sorted([c['label'] for c in all_codes if c.get('status') == 'Met'])
    not_met = sorted([c['label'] for c in all_codes if c.get('status') == 'Not Met'])
    if not_met:
        codes_all = sorted(codes_met + not_met)
    else:
        # Legacy entry - only Met codes in source; flag explicitly
        codes_all = codes_met + ['(Not-Met codes not enumerated in source)']

    # Pick canonical HGVS strings
    hgvs_list = v['hgvs']
    # cDNA: prefer 'NM_xxx(GENE):c.xxx (p.xxx)' form; fallback to first NM_ string
    hgvs_cdna = next((h for h in hgvs_list if h.startswith('NM_') and '(' in h), '')
    if not hgvs_cdna:
        hgvs_cdna = next((h for h in hgvs_list if h.startswith('NM_') and ':c.' in h), '')
    # Extract p.XXX from the parenthetical or any string with p.
    hgvs_protein = ''
    pmatch = re.search(r'\(p\.[^)]+\)', hgvs_cdna)
    if pmatch:
        hgvs_protein = pmatch.group(0)[1:-1]  # strip parens
    if not hgvs_protein:
        hgvs_protein = next((h.split(':p.')[1] if ':p.' in h else '' for h in hgvs_list if ':p.' in h), '')
        if hgvs_protein:
            hgvs_protein = 'p.' + hgvs_protein

    # Item F: normalize displayed transcript version to MANE Select
    hgvs_cdna = normalize_hgvs_version(hgvs_cdna, gene)

    # Find genomic HGVS for hg38 + hg19
    coords = {'hg38': None, 'hg19': None}
    for h in hgvs_list:
        parsed = parse_genomic_hgvs(h)
        if parsed:
            chrom, asm, s, e = parsed
            if coords[asm] is None:
                coords[asm] = (chrom, s, e)

    # Item K: no genomic NC_ HGVS (e.g. repeat-notation deletions) - derive via hgvsToVcf
    if coords['hg38'] is None or coords['hg19'] is None:
        fb = coords_via_hgvstovcf(hgvs_list)
        for asm in ('hg38', 'hg19'):
            if coords[asm] is None and fb[asm] is not None:
                coords[asm] = fb[asm]

    return {
        'gene': gene,
        'classification': classification,
        'condition': condition,
        'caid': caid,
        'variation_id': variation_id,
        'published': published,
        'codes_met': codes_met,
        'codes_all': codes_all,
        'hgvs_cdna': hgvs_cdna,
        'hgvs_protein': hgvs_protein,
        'coords': coords,
        'hgvs_list': hgvs_list,
    }


def emit_bed(records, db, out_path):
    """Write BED file for one assembly."""
    rows = []
    skipped = 0
    for r in records:
        coords = r['coords'].get(db)
        if coords is None:
            skipped += 1
            print(f'  SKIP (no {db} coords): {r["gene"]} {r["hgvs_protein"]}', file=sys.stderr)
            continue
        chrom, start, end = coords
        protein_short = r['hgvs_protein'].replace('p.', '') if r['hgvs_protein'] else 'unknown'
        name = f'{r["gene"]}_{protein_short}_{r["classification"][0]}'
        color = COLORS.get(r['classification'], DEFAULT_COLOR)
        codes_met_str = ';'.join(r['codes_met']) or '-'
        codes_all_str = ';'.join(r['codes_all']) or '-'
        evrepo_url = f'https://erepo.genome.network/evrepo/api/interpretation/{r["caid"].replace("CAR:", "")}/MONDO:0005045/002'

        mouseover = (
            f'<b>Final</b> - VCEP EvRepo submission<br>'
            f'{r["gene"]} {r["hgvs_protein"] or r["hgvs_cdna"]}<br>'
            f'<b>Classification:</b> {r["classification"]}<br>'
            f'<b>Condition:</b> {r["condition"]}<br>'
            f'<b>Codes met:</b> {codes_met_str}<br>'
            f'<b>Date:</b> {r["published"]}<br>'
            f'<b>CAID:</b> {r["caid"]}'
        )

        rows.append('\t'.join([
            chrom, str(start), str(end),
            name, '0', '+',
            str(start), str(end), color,
            r['gene'],
            r['hgvs_cdna'],
            r['hgvs_protein'],
            r['classification'],
            r['condition'],
            codes_met_str,
            codes_all_str,
            r['published'],
            r['caid'],
            r['variation_id'],
            evrepo_url,
            mouseover,
        ]))

    rows.sort(key=lambda l: (l.split('\t')[0], int(l.split('\t')[1])))
    with open(out_path, 'w') as f:
        for line in rows:
            f.write(line + '\n')
    print(f'  wrote {len(rows)} BED features -> {out_path} (skipped {skipped})')
    return len(rows)


def make_bigbed(bed_path, db, as_path, bb_path):
    cmd = ['bedToBigBed', '-tab', '-type=bed9+11', '-as=' + as_path,
           bed_path, CHROM_SIZES[db], bb_path]
    print(f'  $ {" ".join(cmd)}')
    subprocess.run(cmd, check=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--db', action='append', required=True, choices=['hg38', 'hg19'])
    ap.add_argument('--output-dir', required=True)
    args = ap.parse_args()

    out_dir = os.path.join(args.output_dir, 'cmpVCEPEvRepo')
    os.makedirs(out_dir, exist_ok=True)

    print('  [B.7 EvRepo curated variants]')

    # Load JSON
    data = json.load(open(EVREPO_JSON))
    interpretations = data['variantInterpretations']
    print(f'  loaded {len(interpretations)} interpretations from {EVREPO_JSON}')

    # Parse all records
    records = [extract_record(v) for v in interpretations]
    print(f'  parsed {len(records)} records')

    # Sanity: count classifications
    from collections import Counter
    class_counts = Counter(r['classification'] for r in records)
    print(f'  classifications: {dict(class_counts)}')

    # Sanity: gene distribution
    gene_counts = Counter(r['gene'] for r in records)
    print(f'  genes: {dict(gene_counts)}')

    # Verify worked-example presence
    for ex in ['p.Arg870His', 'p.Arg870Cys']:
        for r in records:
            if r['hgvs_protein'] == ex:
                print(f'  worked example {ex}: {r["classification"]}, codes met: {",".join(r["codes_met"])}')
                break
        else:
            print(f'  WARNING: worked example {ex} not found', file=sys.stderr)

    # Write autoSql + emit per-assembly BEDs + bigBeds
    as_path = os.path.join(out_dir, 'cmpVCEPEvRepo.as')
    with open(as_path, 'w') as f:
        f.write(AUTOSQL)

    counts = {}
    for db in args.db:
        bed_path = os.path.join(out_dir, f'cmpVCEPEvRepoHg{"38" if db=="hg38" else "19"}.bed')
        bb_path  = os.path.join(out_dir, f'cmpVCEPEvRepoHg{"38" if db=="hg38" else "19"}.bb')
        n = emit_bed(records, db, bed_path)
        make_bigbed(bed_path, db, as_path, bb_path)
        counts[db] = n
        print(f'  {db} bigBed: {bb_path}')

    # Cross-assembly parity
    if 'hg38' in counts and 'hg19' in counts:
        if counts['hg38'] == counts['hg19']:
            print(f'  cross-assembly parity OK: {counts["hg38"]} features each')
        else:
            print(f'  WARNING: parity FAILED - hg38={counts["hg38"]} hg19={counts["hg19"]}',
                  file=sys.stderr)


if __name__ == '__main__':
    main()
