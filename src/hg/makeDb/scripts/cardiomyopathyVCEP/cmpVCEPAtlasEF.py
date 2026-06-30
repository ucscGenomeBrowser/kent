#!/usr/bin/env python3
"""
B.10 &#8212; Atlas of Cardiac Genetic Variation per-variant track (PS4 case-control).

Parses 189 cached per-variant HTML pages from the Atlas scrape (A.9), extracts
case carriers + cohort + ExAC data, computes Fisher 2&#215;2 OR + 95% CI with
Haldane 0.5 correction, and renders as bigBed.

Color by computed CI_lower PS4 strength binning:
  STRONG     CI_lower >= 20
  MODERATE   CI_lower >= 10
  SUPPORTING CI_lower >= 5
  below      CI_lower < 5

Mouseover shows: case counts, ExAC carriers, computed OR + CI, etiologic fraction
(from Atlas), OMGL/LMM classification.

Validation: spot-check 5 known PS4-relevant variants &#8212; recompute OR and verify
within &#177;5% of cardiodb.org/cmgwap/ output (deferred to D.1 audit step).

Source: cmp_downloads/atlas/variants/var_*.html (cached scrape from A.9)

Outputs:
  cmpVCEPAtlasEF/cmpVCEPAtlasEF.as
  cmpVCEPAtlasEF/cmpVCEPAtlasEFHg{38,19}.bed + .bb
"""

import argparse, math, os, re, subprocess, sys
from html.parser import HTMLParser

ATLAS_VAR_DIR = '/hive/users/lrnassar/claude/RM37446/cmp_downloads/atlas/variants'

OUR_GENES = {'MYH7', 'MYBPC3', 'TNNT2', 'TNNI3', 'TPM1', 'ACTC1', 'MYL2', 'MYL3'}

# ExAC reference cohort size &#8212; DEFAULT (Walsh 2017 baseline; used as fallback)
EXAC_TOTAL_INDIVIDUALS = 60706

# Per-gene Non-Truncating ExAC denominators from Walsh 2019 Table S1.
# Used for non-truncating Atlas variants (missense, synonymous, inframe) to match
# the cohort sizes the VCEP CSpec PS4 calibration is grounded in.
# Truncating/splice variants fall back to the default; documented as a v1 limitation.
WALSH_2019_EXAC_NONTRUNC = {
    'MYH7':   60469,
    'MYBPC3': 45794,
    'TNNT2':  57018,
    'TNNI3':  52607,
    'TPM1':   58642,
    'MYL2':   60521,
    'MYL3':   60605,
    'ACTC1':  60198,
}

NONTRUNC_VARTYPES = {'missense', 'synonymous', 'inframe', 'in-frame'}

# PS4 strength binning by CI_lower
PS4_COLOR_STRONG     = '210,0,0'
PS4_COLOR_MODERATE   = '230,80,80'
PS4_COLOR_SUPPORTING = '245,152,152'
PS4_COLOR_BELOW      = '136,136,136'

CHROM_SIZES = {
    'hg38': '/cluster/data/hg38/chrom.sizes',
    'hg19': '/cluster/data/hg19/chrom.sizes',
}

LIFTOVER_HG19_TO_HG38 = '/cluster/data/hg19/bed/liftOver/hg19ToHg38.over.chain.gz'

AUTOSQL = """table cmpVCEPAtlasEF
"Atlas of Cardiac Genetic Variation per-variant case counts + UCSC-computed OR (PS4)"
    (
    string  chrom;          "Chromosome"
    uint    chromStart;     "Position (BED 0-based)"
    uint    chromEnd;       "End (BED half-open)"
    string  name;           "Display name"
    uint    score;          "0"
    char[1] strand;         "Strand"
    uint    thickStart;     "Same as chromStart"
    uint    thickEnd;       "Same as chromEnd"
    uint    itemRgb;        "PS4 strength color"
    string  gene;           "Gene symbol"
    string  hgvsCdna;       "c. notation"
    string  hgvsProtein;    "p. notation (or splice/frameshift type)"
    string  variantType;    "missense / nonsense / frameshift / splice site / etc."
    string  cohortDisease;  "HCM / DCM / both"
    uint    caseCarriers;   "Total case carriers (OMGL+LMM combined)"
    uint    caseCohortSize; "Total case cohort size for that disease"
    uint    exacCarriers;   "ExAC carrier count"
    double  oddsRatio;      "Computed OR (Fisher 2x2 with Haldane correction)"
    double  ciLower;        "95% CI lower bound (Woolf log-OR)"
    double  ciUpper;        "95% CI upper bound"
    string  ps4Strength;    "Computed PS4 strength: Strong/Moderate/Supporting/below"
    double  etiologicFraction; "EF as published on the Atlas page"
    string  omglClass;      "OMGL classification (Class 1-5)"
    string  lmmClass;       "LMM classification (Pathogenic / VUS / Benign)"
    string  atlasUrl;       "Link to acgv_variant.php?id=N"
    lstring _mouseOver;     "Tooltip HTML"
    )
"""


class TextOnly(HTMLParser):
    def __init__(self): super().__init__(); self.text = []; self.skip = False
    def handle_starttag(self, t, a):
        if t in ('script', 'style'): self.skip = True
    def handle_endtag(self, t):
        if t in ('script', 'style'): self.skip = False
    def handle_data(self, d):
        if not self.skip:
            self.text.append(d)


def page_text(html):
    p = TextOnly()
    p.feed(html)
    return re.sub(r'\s+', ' ', ' '.join(p.text)).strip()


def parse_variant_page(html, var_id):
    """Extract structured fields from one Atlas variant page. Returns dict or None on parse fail."""
    text = page_text(html)

    # Header pattern: "<GENE> : c.<VAR>"
    m = re.search(r'\b([A-Z][A-Z0-9]+)\s*:\s*(c\.\S+?)\s+\1', text)
    if not m:
        return None
    gene = m.group(1)
    cdna = m.group(2).rstrip(',')
    if gene not in OUR_GENES:
        return None

    # Variant Details: protein, type, effect, GRCh37 coords, ExAC freq
    # The order in the page: protein | type | effect | coords | ExAC_freq
    # e.g.: c.1504C>T | p.R502W (Arg > Trp) | substitution | missense | chr11:47364249 (reverse strand) | 0.00002486
    m = re.search(
        r'Variant Details.*?'
        r'(c\.\S+?)\s+'
        r'(p\.\S+?(?:\s*\([^)]*\))?|\(splice site\)|\(frameshift\)|\(in-frame\))\s+'
        r'(\w+(?:\s+\w+)?)\s+'  # variant effect (substitution / insertion / deletion / dup)
        r'(missense|nonsense|frameshift|splice site|essential splice site|inframe|synonymous|in-frame)\s+'
        r'chr([\dXYMT]+):(\d+)(?:\s*\(([^)]*)\))?\s+'  # chr:pos and optional strand note
        r'([\d.]+|-)',  # ExAC freq or '-'
        text)
    if not m:
        # try a more lenient parse
        m2 = re.search(r'chr([\dXYMT]+):(\d+)\s*\((reverse|forward) strand\)', text)
        if not m2:
            return None
        chrom_num = m2.group(1)
        pos1_grch37 = int(m2.group(2))
        strand_note = m2.group(3)
        protein = ''
        vartype = ''
        # crude protein extract
        m3 = re.search(r'(p\.\S+(?:\s*\([^)]*\))?)', text[:1500])
        if m3:
            protein = m3.group(1)
        m4 = re.search(r'(missense|nonsense|frameshift|essential splice site|splice site|inframe|in-frame|synonymous)', text[:2000], re.I)
        if m4:
            vartype = m4.group(1).lower()
        exac_freq = None
    else:
        cdna = m.group(1)
        protein = m.group(2)
        # m.group(3) is variant effect (substitution / etc.); skip
        vartype = m.group(4)
        chrom_num = m.group(5)
        pos1_grch37 = int(m.group(6))
        strand_note = m.group(7) or 'forward'
        try:
            exac_freq = float(m.group(8))
        except (ValueError, TypeError):
            exac_freq = None

    # Case counts: OMGL: Detected in N / Total Disease patients
    case_data = {'HCM': {'omgl': None, 'lmm': None, 'omgl_class': '', 'lmm_class': ''},
                 'DCM': {'omgl': None, 'lmm': None, 'omgl_class': '', 'lmm_class': ''}}

    for disease in ('HCM', 'DCM'):
        # OMGL
        m_o = re.search(rf'OMGL:\s*Detected in\s*(\d+)\s*/\s*(\d+)\s*{disease} patients(?:\s*-\s*classified as\s*([^\.]+?)\s*\.)?', text)
        if m_o:
            case_data[disease]['omgl'] = (int(m_o.group(1)), int(m_o.group(2)))
            case_data[disease]['omgl_class'] = (m_o.group(3) or '').strip()
        # LMM
        m_l = re.search(rf'LMM:\s*Detected in\s*(\d+)\s*/\s*(\d+)\s*{disease} patients(?:\s*-\s*classified as\s*([^\.]+?)\s*\.)?', text)
        if m_l:
            case_data[disease]['lmm'] = (int(m_l.group(1)), int(m_l.group(2)))
            case_data[disease]['lmm_class'] = (m_l.group(3) or '').strip()

    # ExAC total carriers/alleles (from "Total" column of ExAC subpopulation table)
    # Usually appears as: "0.00002486 3 / 120674" (freq, count, total alleles)
    m_exac = re.search(r'(\d+\.\d{4,8})\s+(\d+)\s*/\s*(120674|6\d{4,5}|\d{5,6})\b', text)
    exac_carriers = None
    exac_alleles_total = None
    if m_exac:
        exac_carriers = int(m_exac.group(2))
        exac_alleles_total = int(m_exac.group(3))

    # Etiologic Fraction
    ef = None
    m_ef = re.search(r'etiological fraction \(EF\) of\s*(\d+\.\d+)', text)
    if m_ef:
        ef = float(m_ef.group(1))

    return {
        'var_id': var_id,
        'gene': gene,
        'cdna': cdna,
        'protein': protein,
        'vartype': vartype,
        'grch37_chrom': f'chr{chrom_num}',
        'grch37_pos': pos1_grch37,
        'strand_note': strand_note,
        'exac_freq': exac_freq,
        'exac_carriers': exac_carriers,
        'exac_alleles_total': exac_alleles_total,
        'case_data': case_data,
        'ef': ef,
    }


def fisher_or_haldane(a, b, c, d):
    """OR + 95% CI for 2x2 table with Haldane 0.5 correction.
    Returns (OR, CI_lower, CI_upper) or (None, None, None) if all zero."""
    # Haldane correction when any cell is 0
    if min(a, b, c, d) == 0:
        a += 0.5; b += 0.5; c += 0.5; d += 0.5
    if a*b*c*d == 0:
        return None, None, None
    or_val = (a * d) / (b * c)
    log_or = math.log(or_val)
    se = math.sqrt(1/a + 1/b + 1/c + 1/d)
    ci_lo = math.exp(log_or - 1.96 * se)
    ci_hi = math.exp(log_or + 1.96 * se)
    return or_val, ci_lo, ci_hi


def ps4_bin(ci_lower):
    if ci_lower is None: return 'unable to compute', PS4_COLOR_BELOW
    if ci_lower >= 20: return 'Strong', PS4_COLOR_STRONG
    if ci_lower >= 10: return 'Moderate', PS4_COLOR_MODERATE
    if ci_lower >= 5:  return 'Supporting', PS4_COLOR_SUPPORTING
    return 'below threshold', PS4_COLOR_BELOW


def liftover_grch37_to_grch38(grch37_records, out_dir):
    """liftOver hg19 &#8594; hg38. Returns dict of grch37_pos &#8594; (chrom, start, end) in hg38."""
    if not grch37_records:
        return {}
    tmp_bed = os.path.join(out_dir, '_atlas_lift.bed')
    out_bed = os.path.join(out_dir, '_atlas_lift_out.bed')
    unmapped = os.path.join(out_dir, '_atlas_lift.unmapped')
    with open(tmp_bed, 'w') as f:
        for i, r in enumerate(grch37_records):
            # 1-based to BED 0-based
            start = r['grch37_pos'] - 1
            end   = start + 1
            f.write(f'{r["grch37_chrom"]}\t{start}\t{end}\tatlas_{r["var_id"]}\t0\t+\n')
    cmd = ['liftOver', tmp_bed, LIFTOVER_HG19_TO_HG38, out_bed, unmapped]
    subprocess.run(cmd, check=True)
    mapping = {}
    for line in open(out_bed):
        f = line.rstrip('\n').split('\t')
        var_id = int(f[3].replace('atlas_', ''))
        mapping[var_id] = (f[0], int(f[1]), int(f[2]))
    return mapping


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--db', action='append', required=True, choices=['hg38', 'hg19'])
    ap.add_argument('--output-dir', required=True)
    args = ap.parse_args()

    out_dir = os.path.join(args.output_dir, 'cmpVCEPAtlasEF')
    os.makedirs(out_dir, exist_ok=True)

    print('  [B.10 Atlas per-variant + computed OR]')

    # Parse all cached variant pages
    records = []
    parse_failures = 0
    for fname in sorted(os.listdir(ATLAS_VAR_DIR)):
        if not fname.startswith('var_') or not fname.endswith('.html'):
            continue
        var_id = int(fname.replace('var_', '').replace('.html', ''))
        html = open(os.path.join(ATLAS_VAR_DIR, fname)).read()
        rec = parse_variant_page(html, var_id)
        if rec is None:
            parse_failures += 1
            continue
        records.append(rec)
    print(f'  parsed {len(records)} variant pages; {parse_failures} parse failures')

    from collections import Counter
    print(f'  gene distribution: {Counter(r["gene"] for r in records)}')

    # liftOver Atlas GRCh37 coords to hg38
    lift_map = liftover_grch37_to_grch38(records, out_dir)
    print(f'  liftOver hg19&#8594;hg38: {len(lift_map)} of {len(records)} mapped')

    # Build BED features
    bed_lines = []
    n_strong = n_moderate = n_supporting = n_below = 0

    for r in records:
        # Compute combined case carriers + cohort across HCM and DCM
        # Use the disease that has data; if both, sum
        diseases_with_data = []
        for disease in ('HCM', 'DCM'):
            cd = r['case_data'][disease]
            if cd['omgl'] is not None or cd['lmm'] is not None:
                diseases_with_data.append(disease)
        if not diseases_with_data:
            continue  # no case data &#8594; skip

        # For each disease, compute OR
        for disease in diseases_with_data:
            cd = r['case_data'][disease]
            omgl = cd['omgl'] or (0, 0)
            lmm  = cd['lmm']  or (0, 0)
            case_carriers = omgl[0] + lmm[0]
            case_cohort   = omgl[1] + lmm[1]
            if case_cohort == 0 or case_carriers == 0:
                continue  # skip zero-case rows

            # ExAC carriers &#8212; Atlas reports allele count, not individual count.
            # Use exac_carriers as "individuals carrying" (approximation: rare variants &#8776; heterozygous only)
            exac_c = r['exac_carriers'] or 0
            # Per D.1 audit P0 #1: use Walsh 2019 Table S1 per-gene NonTrunc denominators
            # for non-truncating variants; truncating/splice fall back to default.
            vartype_lower = (r['vartype'] or '').lower()
            if vartype_lower in NONTRUNC_VARTYPES:
                exac_cohort = WALSH_2019_EXAC_NONTRUNC.get(r['gene'], EXAC_TOTAL_INDIVIDUALS)
            else:
                exac_cohort = EXAC_TOTAL_INDIVIDUALS

            a = case_carriers
            b = case_cohort - case_carriers
            c = exac_c
            d = exac_cohort - exac_c
            or_val, ci_lo, ci_hi = fisher_or_haldane(a, b, c, d)
            strength, color = ps4_bin(ci_lo)
            if strength == 'Strong':     n_strong += 1
            elif strength == 'Moderate': n_moderate += 1
            elif strength == 'Supporting': n_supporting += 1
            else: n_below += 1

            # Get hg38 coords
            hg38 = lift_map.get(r['var_id'])
            if hg38 is None:
                continue
            chrom_hg38, start_hg38, end_hg38 = hg38

            atlas_url = f'https://www.cardiodb.org/acgv/acgv_variant.php?id={r["var_id"]}'
            ef_str = f'{r["ef"]:.3f}' if r['ef'] is not None else '&#8212;'
            mouseover = (
                f'<b>PS4 per-variant</b> - Atlas case-control<br>'
                f'<b>{r["gene"]}</b> {r["cdna"]} {r["protein"]} ({r["vartype"]})<br>'
                f'<b>{disease}:</b> {case_carriers}/{case_cohort} cases | ExAC: {exac_c}/{exac_cohort}<br>'
                f'<b>OR</b> {or_val:.1f} 95% CI ({ci_lo:.1f}&#8211;{ci_hi:.1f}) &#8594; {strength}<br>'
                f'<b>Etiologic fraction (Atlas):</b> {ef_str}<br>'
                f'<b>OMGL:</b> {cd["omgl_class"] or "&#8212;"} | <b>LMM:</b> {cd["lmm_class"] or "&#8212;"}<br>'
                f'<i>Note: Atlas data uses ExAC, not gnomAD v4.1; OR computed by UCSC.</i><br>'
                f'<b>Source:</b> <a href="{atlas_url}">acgv_variant.php?id={r["var_id"]}</a>'
            )
            name = f'{r["gene"]}_{r["protein"].replace("p.","")[:12]}_{disease}_{strength.replace(" ","_")}'
            bed_lines.append('\t'.join([
                chrom_hg38, str(start_hg38), str(end_hg38),
                name, '0', '+',
                str(start_hg38), str(end_hg38), color,
                r['gene'],
                r['cdna'],
                r['protein'],
                r['vartype'],
                disease,
                str(case_carriers),
                str(case_cohort),
                str(exac_c),
                f'{or_val:.3f}',
                f'{ci_lo:.3f}',
                f'{ci_hi:.3f}',
                strength,
                f'{r["ef"]}' if r['ef'] is not None else '0',
                cd['omgl_class'] or '',
                cd['lmm_class'] or '',
                atlas_url,
                mouseover,
            ]))

    print(f'  PS4 binning: STRONG={n_strong}, MODERATE={n_moderate}, SUPPORTING={n_supporting}, below={n_below}')

    bed_lines.sort(key=lambda l: (l.split('\t')[0], int(l.split('\t')[1])))

    as_path = os.path.join(out_dir, 'cmpVCEPAtlasEF.as')
    with open(as_path, 'w') as f:
        f.write(AUTOSQL)

    hg38_bed = os.path.join(out_dir, 'cmpVCEPAtlasEFHg38.bed')
    with open(hg38_bed, 'w') as f:
        for l in bed_lines:
            f.write(l + '\n')
    print(f'  wrote {len(bed_lines)} BED features &#8594; {hg38_bed}')

    if 'hg38' in args.db:
        hg38_bb = os.path.join(out_dir, 'cmpVCEPAtlasEFHg38.bb')
        cmd = ['bedToBigBed', '-tab', '-type=bed9+16', '-as=' + as_path,
               hg38_bed, CHROM_SIZES['hg38'], hg38_bb]
        print(f'  $ {" ".join(cmd)}')
        subprocess.run(cmd, check=True)

    # hg19: derive from grch37 records directly (no liftover round-trip)
    if 'hg19' in args.db:
        hg19_bed = os.path.join(out_dir, 'cmpVCEPAtlasEFHg19.bed')
        # Re-emit lines using GRCh37 coords from original parse
        # Build a map var_id &#8594; row, then re-emit with GRCh37 coords
        var_recs = {r['var_id']: r for r in records}
        hg19_lines = []
        for line in bed_lines:
            f = line.split('\t')
            # the first BED9 are hg38 coords; everything else is the same
            # Find the var_id from atlas URL field (last data column before mouseover)
            atlas_url = f[-2]  # '...atlas_url' is the second-to-last (mouseover is last)
            m = re.search(r'id=(\d+)', atlas_url)
            if not m:
                continue
            var_id = int(m.group(1))
            r = var_recs.get(var_id)
            if r is None:
                continue
            # GRCh37 coords (1-based) &#8594; BED 0-based half-open (assume 1-bp)
            grch37_start = r['grch37_pos'] - 1
            grch37_end = grch37_start + 1
            f[0] = r['grch37_chrom']
            f[1] = str(grch37_start)
            f[2] = str(grch37_end)
            f[6] = str(grch37_start)
            f[7] = str(grch37_end)
            hg19_lines.append('\t'.join(f))
        hg19_lines.sort(key=lambda l: (l.split('\t')[0], int(l.split('\t')[1])))
        with open(hg19_bed, 'w') as fh:
            for l in hg19_lines:
                fh.write(l + '\n')
        hg19_bb = os.path.join(out_dir, 'cmpVCEPAtlasEFHg19.bb')
        cmd = ['bedToBigBed', '-tab', '-type=bed9+16', '-as=' + as_path,
               hg19_bed, CHROM_SIZES['hg19'], hg19_bb]
        print(f'  $ {" ".join(cmd)}')
        subprocess.run(cmd, check=True)

    if 'hg38' in args.db and 'hg19' in args.db:
        n38 = len(bed_lines)
        n19 = len(hg19_lines)
        if n38 == n19:
            print(f'  cross-assembly parity OK: {n38} features each')
        else:
            print(f'  WARNING: parity FAILED &#8212; hg38={n38} hg19={n19}', file=sys.stderr)


if __name__ == '__main__':
    main()
