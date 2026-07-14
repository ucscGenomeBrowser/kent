#!/usr/bin/env python3
"""
B.8 - Cardiomyopathy VCEP ClinVar submitter 506161 track builder.

Renders all 199 ClinVar submissions from "ClinGen Cardiomyopathy Variant Curation
Expert Panel" - verified at A.3 to be 100% "reviewed by expert panel" (no relabel
branch needed).

Input:
  /hive/users/lrnassar/claude/RM37446/cmp_downloads/clinvar/cardiomyopathyVCEP_submissions.tsv
    (199 submission rows, schema = ClinVar submission_summary.txt.gz)
  /hive/data/outside/otto/clinvar/downloads/2026-05-30/variant_summary.txt.gz
    (per-VariationID hg19 + hg38 coords; joined by col 31 = VariationID)

Outputs:
  cmpVCEPClinVar506161/cmpVCEPClinVar506161.as
  cmpVCEPClinVar506161/cmpVCEPClinVar506161Hg38.bed + .bb
  cmpVCEPClinVar506161/cmpVCEPClinVar506161Hg19.bed + .bb

Usage:
  python3 cmpVCEPClinVar506161.py --db hg38 --db hg19 \
      --output-dir /hive/users/lrnassar/claude/RM37446
"""

import argparse, gzip, json, os, subprocess, sys

SUBMISSIONS_TSV = '/hive/users/lrnassar/claude/RM37446/cmp_downloads/clinvar/cardiomyopathyVCEP_submissions.tsv'
VARIANT_SUMMARY = '/hive/data/outside/otto/clinvar/downloads/2026-05-30/variant_summary.txt.gz'

# Standard 5-tier ACMG ramp (same as EvRepo / Walsh 2019); the "with codes vs no codes"
# distinction is carried by the track name and mouseover, not by color.
COLORS = {
    'Pathogenic':              '210,0,0',
    'Likely pathogenic':       '245,152,152',
    'Uncertain significance':  '0,0,136',
    'Likely benign':           '213,247,213',
    'Benign':                  '0,210,0',
}
DEFAULT_COLOR = '136,136,136'

# Per-gene EvRepo VariationIDs - built once at build-time from EvRepo JSON
EVREPO_VAR_IDS = set()
EVREPO_JSON = '/hive/users/lrnassar/claude/RM37446/cmp_downloads/erepo/cardiomyopathyVCEP_classifications.json'
try:
    _erepo = json.load(open(EVREPO_JSON))
    EVREPO_VAR_IDS = set(v.get('variationId', '') for v in _erepo['variantInterpretations'] if v.get('variationId'))
    print(f'  loaded {len(EVREPO_VAR_IDS)} EvRepo VariationIDs (will tag overlapping rows)')
except Exception as e:
    print(f'  WARNING: could not load EvRepo JSON: {e}', file=sys.stderr)

CHROM_SIZES = {
    'hg38': '/cluster/data/hg38/chrom.sizes',
    'hg19': '/cluster/data/hg19/chrom.sizes',
}

AUTOSQL = """table cmpVCEPClinVar506161
"ClinVar submitter 506161 (ClinGen Cardiomyopathy VCEP) - Final, no evidence codes attached"
    (
    string  chrom;          "Chromosome"
    uint    chromStart;     "Start position (BED 0-based)"
    uint    chromEnd;       "End position (BED half-open)"
    string  name;           "Display name (gene + hgvsP + classification initial)"
    uint    score;          "Always 0"
    char[1] strand;         "Always +"
    uint    thickStart;     "Same as chromStart"
    uint    thickEnd;       "Same as chromEnd"
    uint    itemRgb;        "Display color (desaturated ACMG palette)"
    string  gene;           "Gene symbol"
    string  hgvsName;       "Canonical HGVS Name (NM_xx(GENE):c.x>y (p.xxx))"
    string  classification; "VCEP classification"
    string  reviewStatus;   "ClinVar review status (verified all-expert-panel at A.3)"
    string  lastEvaluated;  "Date last evaluated"
    string  scvAccession;   "SCV submission accession"
    string  variationId;    "ClinVar VariationID"
    string  rcvAccession;   "ClinVar RCV accession"
    string  inEvRepo;       "yes if also in EvRepo (variant is in both sources)"
    string  clinvarUrl;     "ClinVar variant URL"
    lstring _mouseOver;     "Tooltip HTML"
    )
"""


def load_submissions():
    """Parse 199-row TSV. Returns dict by VariationID (string) -> submission record."""
    submissions = {}
    header = None
    for line in open(SUBMISSIONS_TSV):
        line = line.rstrip('\n')
        if line.startswith('##'):
            continue
        if line.startswith('#'):
            header = line.lstrip('#').split('\t')
            continue
        f = line.split('\t')
        if len(f) < 12:
            continue
        var_id = f[0]
        submissions[var_id] = {
            'classification': f[1],
            'last_evaluated': f[2],
            'description':    f[3],
            'review_status':  f[6],
            'submitter':      f[9],
            'scv':            f[10],
            'gene':           f[11] if len(f) > 11 else '',
        }
    print(f'  loaded {len(submissions)} ClinVar submitter-506161 records')
    return submissions


def load_variant_summary_coords(submissions):
    """Stream variant_summary.txt.gz, picking rows whose VariationID (col 31) is in our set.
    Returns dict: var_id -> {assembly: {chrom, start_bed, end_bed, ref, alt, hgvs_name, rcv}}."""
    var_ids = set(submissions.keys())
    coords = {vid: {} for vid in var_ids}

    # Schema (1-based positions in awk; 0-based here):
    #  3=Name, 5=GeneSymbol, 12=RCVaccession, 17=Assembly, 18=ChromosomeAccession,
    #  19=Chromosome, 20=Start, 21=Stop, 22=ReferenceAllele, 23=AlternateAllele, 31=VariationID
    n_rows_seen = 0
    with gzip.open(VARIANT_SUMMARY, 'rt') as fh:
        for line in fh:
            if line.startswith('#'):
                continue
            f = line.rstrip('\n').split('\t')
            if len(f) < 31:
                continue
            var_id = f[30]  # VariationID (1-based col 31 -> 0-based index 30)
            if var_id not in var_ids:
                continue
            n_rows_seen += 1
            assembly = f[16]      # GRCh38 / GRCh37
            chrom_num = f[18]      # 1, 2, ..., X
            try:
                start1 = int(f[19])  # 1-based start
                stop1  = int(f[20])  # 1-based stop (inclusive)
            except ValueError:
                continue
            ref = f[21]
            alt = f[22]
            chrom = f'chr{chrom_num}'
            db = 'hg38' if assembly == 'GRCh38' else ('hg19' if assembly == 'GRCh37' else None)
            if db is None:
                continue
            # BED is 0-based half-open
            start_bed = start1 - 1
            end_bed   = stop1
            coords[var_id][db] = {
                'chrom': chrom,
                'start': start_bed,
                'end':   end_bed,
                'ref':   ref,
                'alt':   alt,
                'hgvs_name': f[2],
                'gene':   f[4],
                'rcv':    f[11],
            }
    print(f'  matched {n_rows_seen} variant_summary rows for {len(var_ids)} VariationIDs')
    return coords


def emit_bed(submissions, coords, db, out_path):
    rows = []
    skipped = 0
    for var_id, sub in submissions.items():
        c = coords.get(var_id, {}).get(db)
        if c is None:
            skipped += 1
            continue
        # Extract HGVS protein from Name (NM_xxx(GENE):c.xxx (p.yyy))
        hgvs_name = c['hgvs_name']
        protein_short = ''
        if '(p.' in hgvs_name:
            try:
                protein_short = hgvs_name.split('(p.')[1].rstrip(')')
            except IndexError:
                pass
        if not protein_short:
            # fallback to first 12 chars of HGVS Name after the colon
            protein_short = hgvs_name.split(':', 1)[-1][:20].replace(' ', '_')

        gene = c['gene'] or sub['gene']
        cls = sub['classification']
        color = COLORS.get(cls, DEFAULT_COLOR)
        in_evrepo = 'yes' if var_id in EVREPO_VAR_IDS else 'no'
        clinvar_url = f'https://www.ncbi.nlm.nih.gov/clinvar/variation/{var_id}/'

        name = f'{gene}_{protein_short}_{cls.split()[0][:1]}'
        mouseover = (
            f'<b>Final</b> - ClinVar submitter 506161 (no evidence codes)<br>'
            f'{gene} {hgvs_name}<br>'
            f'<b>Classification:</b> {cls}<br>'
            f'<b>Review:</b> {sub["review_status"]}<br>'
            f'<b>Last evaluated:</b> {sub["last_evaluated"]}<br>'
            f'<b>In EvRepo:</b> {in_evrepo}'
        )

        rows.append('\t'.join([
            c['chrom'], str(c['start']), str(c['end']),
            name, '0', '+',
            str(c['start']), str(c['end']), color,
            gene,
            hgvs_name,
            cls,
            sub['review_status'],
            sub['last_evaluated'],
            sub['scv'],
            var_id,
            c['rcv'],
            in_evrepo,
            clinvar_url,
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

    out_dir = os.path.join(args.output_dir, 'cmpVCEPClinVar506161')
    os.makedirs(out_dir, exist_ok=True)

    print('  [B.8 ClinVar submitter 506161]')

    submissions = load_submissions()
    coords = load_variant_summary_coords(submissions)

    # Distribution sanity
    from collections import Counter
    review = Counter(s['review_status'] for s in submissions.values())
    print(f'  review-status: {dict(review)} (expecting 199 expert-panel-reviewed)')
    classes = Counter(s['classification'] for s in submissions.values())
    print(f'  classifications: {dict(classes)}')
    n_in_erepo = sum(1 for v in submissions if v in EVREPO_VAR_IDS)
    print(f'  overlap with EvRepo: {n_in_erepo} of {len(submissions)}')

    as_path = os.path.join(out_dir, 'cmpVCEPClinVar506161.as')
    with open(as_path, 'w') as f:
        f.write(AUTOSQL)

    counts = {}
    for db in args.db:
        bed_path = os.path.join(out_dir, f'cmpVCEPClinVar506161Hg{"38" if db=="hg38" else "19"}.bed')
        bb_path  = os.path.join(out_dir, f'cmpVCEPClinVar506161Hg{"38" if db=="hg38" else "19"}.bb')
        n = emit_bed(submissions, coords, db, bed_path)
        make_bigbed(bed_path, db, as_path, bb_path)
        counts[db] = n
        print(f'  {db} bigBed: {bb_path}')

    if 'hg38' in counts and 'hg19' in counts:
        if counts['hg38'] == counts['hg19']:
            print(f'  cross-assembly parity OK: {counts["hg38"]} features each')
        else:
            print(f'  WARNING: parity FAILED - hg38={counts["hg38"]} hg19={counts["hg19"]}',
                  file=sys.stderr)


if __name__ == '__main__':
    main()
