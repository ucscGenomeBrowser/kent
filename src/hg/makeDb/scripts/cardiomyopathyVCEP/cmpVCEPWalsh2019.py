#!/usr/bin/env python3
"""
B.7c &#8212; Walsh 2019 Pre-EvRepo curated variants subtrack (folds into Curated Variants composite).

Renders 155 per-variant ACMG/AMP rule applications from Walsh 2019 Table S6 &#8212; pre-EvRepo
VCEP curations that document the original calibration cohort. Folds into the Curated Variants
composite track 4c, off by default.

Source: cmp_downloads/walsh/walsh2019_supplement.xlsx Table S6 (163 rows; filter to our 8 genes)
Coords: lookup against ClinVar variant_summary by (gene, c.notation); skip variants not in ClinVar
        (those are novel-to-Walsh and would need MANE CDS conversion &#8212; deferred).

Outputs:
  cmpVCEPWalsh2019/cmpVCEPWalsh2019.as
  cmpVCEPWalsh2019/cmpVCEPWalsh2019Hg{38,19}.bed + .bb

Usage:
  python3 cmpVCEPWalsh2019.py --db hg38 --db hg19 \
      --output-dir /hive/users/lrnassar/claude/RM37446
"""

import argparse, gzip, os, re, subprocess, sys, warnings
warnings.filterwarnings('ignore')

WALSH_XLSX = '/hive/users/lrnassar/claude/RM37446/cmp_downloads/walsh/walsh2019_supplement.xlsx'
VARIANT_SUMMARY = '/hive/data/outside/otto/clinvar/downloads/2026-05-30/variant_summary.txt.gz'

# Transcript Walsh 2019 used for c. numbering, per gene (item L: map ClinVar-unmatched entries
# via hgvsToVcf). NOTE TNNT2 is NOT MANE &#8212; Walsh used the classic cardiac transcript; MANE
# (NM_001276345.2) yields HgvsRefAssertedMismatch. Verified each gives FILTER=PASS.
HGVSTOVCF = '/cluster/bin/x86_64/hgvsToVcf'
WALSH_TX = {'MYBPC3': 'NM_000256.3', 'MYH7': 'NM_000257.4',
            'TNNI3': 'NM_000363.5', 'TNNT2': 'NM_001001430.2'}


LIFTOVER_HG38_HG19 = '/cluster/data/hg38/bed/liftOver/hg38ToHg19.over.chain.gz'


def _tool_coords(gene, cdna, db):
    """hgvsToVcf on the gene's Walsh transcript; coords only if FILTER==PASS."""
    tx = WALSH_TX.get(gene)
    if not tx:
        return None
    try:
        res = subprocess.run([HGVSTOVCF, db, '/dev/stdin', 'stdout'],
                             input=f'{tx}:{cdna}\n', text=True, capture_output=True, check=True)
    except subprocess.CalledProcessError:
        return None
    for line in res.stdout.splitlines():
        if line.startswith('#'):
            continue
        f = line.split('\t')
        if len(f) < 7 or not f[1].isdigit():
            continue
        if f[6] != 'PASS':           # reject HgvsRefAssertedMismatch etc.
            return None
        pos, ref, alt = int(f[1]), f[3], f[4]
        if len(ref) == len(alt):     # substitution / MNV
            start, end = pos - 1, pos - 1 + len(ref)
        else:                        # indel with VCF anchor base
            start, end = pos, pos - 1 + len(ref)
            if end <= start:
                end = start + 1
        return {'chrom': f[0], 'start': start, 'end': end, 'variation_id': '&#8212;'}
    return None


def _liftover_38_to_19(chrom, start, end):
    """liftOver a single hg38 interval to hg19; return (chrom,start,end) or None."""
    import tempfile
    with tempfile.TemporaryDirectory() as td:
        inb, outb, un = f'{td}/in.bed', f'{td}/out.bed', f'{td}/un.bed'
        with open(inb, 'w') as f:
            f.write(f'{chrom}\t{start}\t{end}\tx\n')
        try:
            subprocess.run(['liftOver', inb, LIFTOVER_HG38_HG19, outb, un],
                           capture_output=True, check=True)
        except subprocess.CalledProcessError:
            return None
        if os.path.getsize(outb) == 0:
            return None
        g = open(outb).readline().split('\t')
        return g[0], int(g[1]), int(g[2])


def walsh_coords_via_tool(gene, cdna, db):
    """Item L: coords for a Walsh entry absent from ClinVar. Map hg38 via hgvsToVcf (FILTER==PASS).
    For hg19, try hgvsToVcf directly; if the transcript has no hg19 alignment (e.g. TNNT2
    NM_001001430.2), liftOver the hg38 coords to hg19 so cross-assembly parity is preserved."""
    if db == 'hg38':
        return _tool_coords(gene, cdna, 'hg38')
    direct = _tool_coords(gene, cdna, 'hg19')
    if direct:
        return direct
    h38 = _tool_coords(gene, cdna, 'hg38')
    if h38:
        lifted = _liftover_38_to_19(h38['chrom'], h38['start'], h38['end'])
        if lifted:
            return {'chrom': lifted[0], 'start': lifted[1], 'end': lifted[2], 'variation_id': '&#8212;'}
    return None

OUR_GENES = {'MYH7', 'MYBPC3', 'TNNT2', 'TNNI3', 'TPM1', 'ACTC1', 'MYL2', 'MYL3'}

# Walsh classifications include "Likely Pathogenic *" (asterisk = upgraded by Walsh's new PM1 EF-based rule).
# Strip asterisk; preserve in mouseover.
COLORS = {
    'Pathogenic':              '210,0,0',
    'Likely Pathogenic':       '245,152,152',
    'Uncertain Significance':  '0,0,136',
    'Likely Benign':           '213,247,213',
    'Benign':                  '0,210,0',
}
DEFAULT_COLOR = '136,136,136'

CHROM_SIZES = {
    'hg38': '/cluster/data/hg38/chrom.sizes',
    'hg19': '/cluster/data/hg19/chrom.sizes',
}

AUTOSQL = """table cmpVCEPWalsh2019
"Walsh 2019 Pre-EvRepo VCEP curated variants - pre-dates the ClinGen Evidence Repository"
    (
    string  chrom;          "Chromosome"
    uint    chromStart;     "Start position"
    uint    chromEnd;       "End position"
    string  name;           "Display name"
    uint    score;          "Always 0"
    char[1] strand;         "Strand"
    uint    thickStart;     "Same as chromStart"
    uint    thickEnd;       "Same as chromEnd"
    uint    itemRgb;        "Display color"
    string  gene;           "Gene symbol"
    string  variantCdna;    "c. notation from Walsh 2019 Table S6"
    string  variantProtein; "p. notation from Walsh 2019 Table S6"
    string  variantType;    "missense, nonsense, frameshift, splice site, etc."
    string  classification; "VCEP classification (Pathogenic, Likely Pathogenic, VUS, etc.)"
    string  walshUpgraded;  "yes if Walsh upgraded via new EF-based PM1 rule"
    string  acmgRules;      "ACMG/AMP rules activated (e.g., PM2,PP3,PM1(s))"
    string  cases;          "Number of cases observed in Walsh 2019 cohort"
    string  exacFaf;        "ExAC filtering allele frequency"
    string  source;         "Source for curated evidence (PMID or ClinVar SCV, where applicable)"
    string  variationId;    "Matched ClinVar VariationID (if found)"
    lstring _mouseOver;     "Tooltip HTML"
    )
"""


def load_walsh_table_s6():
    """Parse Walsh 2019 Table S6 &#8594; list of dict records for our 8 genes."""
    import openpyxl
    wb = openpyxl.load_workbook(WALSH_XLSX, read_only=True, data_only=True)
    ws = wb['Table S6']
    rows = list(ws.iter_rows(values_only=True))

    header_idx = None
    for i, r in enumerate(rows):
        if r and r[0] == 'Gene':
            header_idx = i
            break
    if header_idx is None:
        sys.exit('Table S6: header not found')

    records = []
    for r in rows[header_idx + 1:]:
        if not r or not r[0]:
            continue
        if r[0] not in OUR_GENES:
            continue
        cls_raw = (r[5] or '').strip()
        upgraded = '*' in cls_raw
        cls = cls_raw.replace(' *', '').strip()
        # The rules column is r[6]; cases r[7]; source r[8]; ExAC FAF r[4]
        records.append({
            'gene':       r[0],
            'cdna':       r[1] or '',
            'protein':    r[2] or '',
            'vartype':    r[3] or '',
            'exac_faf':   r[4] if r[4] is not None else '',
            'classification': cls,
            'upgraded':   upgraded,
            'rules':      r[6] or '',
            'cases':      r[7] if r[7] is not None else '',
            'source':     r[8] if r[8] is not None else '',
        })
    print(f'  parsed {len(records)} Walsh 2019 Table S6 entries (filtered to our 8 genes)')
    return records


# Matches ClinVar Name field: e.g. NM_000256.3(MYBPC3):c.1504C>T (p.Arg502Trp)
CV_NAME_RE = re.compile(r'^([A-Z]M_[\d\.]+)\(([A-Z0-9]+)\):c\.(\S+?)(?:\s|\(|$)')


def build_clinvar_lookup():
    """Stream variant_summary; build dict (gene, c.notation) &#8594; {assembly: coords + variation_id}."""
    lookup = {}
    n_rows = 0
    with gzip.open(VARIANT_SUMMARY, 'rt') as fh:
        for line in fh:
            if line.startswith('#'):
                continue
            f = line.rstrip('\n').split('\t')
            if len(f) < 31:
                continue
            name = f[2]
            gene = f[4]
            if gene not in OUR_GENES:
                continue
            m = CV_NAME_RE.match(name)
            if not m:
                continue
            cdna = 'c.' + m.group(3)
            assembly = f[16]
            db = 'hg38' if assembly == 'GRCh38' else ('hg19' if assembly == 'GRCh37' else None)
            if db is None:
                continue
            try:
                start1 = int(f[19]); stop1 = int(f[20])
            except ValueError:
                continue
            chrom_num = f[18]
            key = (gene, cdna)
            lookup.setdefault(key, {})[db] = {
                'chrom': f'chr{chrom_num}',
                'start': start1 - 1,
                'end':   stop1,
                'variation_id': f[30],
                'rcv':   f[11],
            }
            n_rows += 1
    print(f'  ClinVar lookup: {len(lookup)} (gene, c.notation) keys &#215; up to 2 assemblies = {n_rows} entries')
    return lookup


def emit_bed(records, lookup, db, out_path):
    rows_emitted = []
    skipped = []
    for rec in records:
        key = (rec['gene'], rec['cdna'])
        c = lookup.get(key, {}).get(db)
        via_tool = False
        if c is None:
            c = walsh_coords_via_tool(rec['gene'], rec['cdna'], db)
            via_tool = c is not None
            if c is None:
                skipped.append(rec)
                continue
        cls = 'Uncertain Significance' if rec['classification'] == 'VUS' else rec['classification']
        color = COLORS.get(cls, DEFAULT_COLOR)
        protein_short = rec['protein'].replace('p.', '') if rec['protein'] else rec['cdna'][:18].replace(' ', '_')
        name = f'{rec["gene"]}_{protein_short}_{cls.replace(" ", "")[:3]}'
        upgraded_label = 'yes' if rec['upgraded'] else 'no'

        mouseover = (
            f'<b>Walsh 2019 pre-EvRepo</b> - PMID 30696458<br>'
            f'{rec["gene"]} {rec["cdna"]} {rec["protein"]}<br>'
            f'<b>Type:</b> {rec["vartype"]}<br>'
            f'<b>Classification:</b> {cls}'
            f'{" (Walsh-upgraded via PM1 EF rule)" if rec["upgraded"] else ""}<br>'
            f'<b>Codes:</b> {rec["rules"]}<br>'
            f'<b>Cases:</b> {rec["cases"]} | <b>ExAC FAF:</b> {rec["exac_faf"] or "&#8212;"}<br>'
            f'<b>Source:</b> {rec["source"] or "&#8212;"}'
            + (f'<br><i>Coordinates derived from {WALSH_TX.get(rec["gene"])} via hgvsToVcf '
               f'(not in ClinVar variant_summary)</i>' if via_tool else '')
        )

        rows_emitted.append('\t'.join([
            c['chrom'], str(c['start']), str(c['end']),
            name, '0', '+',
            str(c['start']), str(c['end']), color,
            rec['gene'],
            rec['cdna'],
            rec['protein'],
            rec['vartype'],
            cls,
            upgraded_label,
            rec['rules'],
            str(rec['cases']),
            str(rec['exac_faf']),
            str(rec['source']),
            c['variation_id'],
            mouseover,
        ]))

    rows_emitted.sort(key=lambda l: (l.split('\t')[0], int(l.split('\t')[1])))
    with open(out_path, 'w') as f:
        for line in rows_emitted:
            f.write(line + '\n')
    print(f'  wrote {len(rows_emitted)} BED features &#8594; {out_path} (skipped {len(skipped)})')
    return len(rows_emitted), skipped


def make_bigbed(bed_path, db, as_path, bb_path):
    cmd = ['bedToBigBed', '-tab', '-type=bed9+12', '-as=' + as_path,
           bed_path, CHROM_SIZES[db], bb_path]
    print(f'  $ {" ".join(cmd)}')
    subprocess.run(cmd, check=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--db', action='append', required=True, choices=['hg38', 'hg19'])
    ap.add_argument('--output-dir', required=True)
    args = ap.parse_args()

    out_dir = os.path.join(args.output_dir, 'cmpVCEPWalsh2019')
    os.makedirs(out_dir, exist_ok=True)

    print('  [B.7c Walsh 2019 Pre-EvRepo curated]')

    records = load_walsh_table_s6()
    lookup = build_clinvar_lookup()

    from collections import Counter
    classes = Counter(r['classification'] for r in records)
    print(f'  classifications: {dict(classes)}')
    n_upgraded = sum(1 for r in records if r['upgraded'])
    print(f'  Walsh-upgraded (PM1 EF rule): {n_upgraded}')
    genes = Counter(r['gene'] for r in records)
    print(f'  genes: {dict(genes)}')

    as_path = os.path.join(out_dir, 'cmpVCEPWalsh2019.as')
    with open(as_path, 'w') as f:
        f.write(AUTOSQL)

    counts = {}
    skipped_summary = None
    for db in args.db:
        bed_path = os.path.join(out_dir, f'cmpVCEPWalsh2019Hg{"38" if db=="hg38" else "19"}.bed')
        bb_path  = os.path.join(out_dir, f'cmpVCEPWalsh2019Hg{"38" if db=="hg38" else "19"}.bb')
        n, skipped = emit_bed(records, lookup, db, bed_path)
        make_bigbed(bed_path, db, as_path, bb_path)
        counts[db] = n
        skipped_summary = skipped
        print(f'  {db} bigBed: {bb_path}')

    if 'hg38' in counts and 'hg19' in counts:
        if counts['hg38'] == counts['hg19']:
            print(f'  cross-assembly parity OK: {counts["hg38"]} features each')
        else:
            print(f'  WARNING: parity FAILED &#8212; hg38={counts["hg38"]} hg19={counts["hg19"]}', file=sys.stderr)

    # Save skipped list for follow-up
    if skipped_summary:
        skip_path = os.path.join(out_dir, 'walsh2019_unmatched.txt')
        with open(skip_path, 'w') as f:
            f.write('# Walsh 2019 Table S6 entries NOT found in ClinVar variant_summary\n')
            f.write('# These need MANE CDS coordinate conversion to render &#8212; deferred to v2 of B.7c\n')
            for r in skipped_summary:
                f.write(f'{r["gene"]}\t{r["cdna"]}\t{r["protein"]}\t{r["classification"]}\n')
        print(f'  skipped variants logged to: {skip_path}')


if __name__ == '__main__':
    main()
