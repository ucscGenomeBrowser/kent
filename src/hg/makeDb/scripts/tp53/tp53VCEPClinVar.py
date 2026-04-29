#!/usr/bin/env python3
"""
TP53 VCEP Curated Variants track generator.

Fetches TP53 VCEP classifications from the ClinGen Evidence Repository
(EvRepo). EvRepo is the authoritative source for VCEP classifications
(176 TP53 variants as of 2026-04-17) and is used here instead of ClinVar
esearch because ClinVar's NCBI esearch index silently drops legacy VCV
records like VCV 12374 / R175H.

Emits bigBed 9+9 with the per-variant list of applied evidence codes plus
the overall classification (P / LP / VUS / LB / B). hg38 and hg19
coordinates parsed directly from the HGVS list; liftOver fallback where
absent.
"""

import argparse
import json
import os
import re
import subprocess
import sys
import tempfile
import time
import urllib.parse
import urllib.request

EVREPO_URL = ("https://erepo.genome.network/evrepo/api/classifications"
              "?gene=TP53&matchLimit=2000&format=json")
CLINVAR_EFETCH = "https://eutils.ncbi.nlm.nih.gov/entrez/eutils/efetch.fcgi"

# HGVS genomic regex for hg38 (NC_000017.11) and hg19 (NC_000017.10)
HGVS_G_RE = re.compile(r'^NC_0000(17)\.(\d+):g\.(\d+)([ACGT])>([ACGT])$')
HGVS_G_INDEL_RE = re.compile(r'^NC_0000(17)\.(\d+):g\.(\d+)_?(\d+)?(del|ins|dup).*$')

LIFTOVER_CHAINS = {
    'hg19_to_hg38': '/cluster/data/hg19/bed/liftOver/hg19ToHg38.over.chain.gz',
    'hg38_to_hg19': '/cluster/data/hg38/bed/liftOver/hg38ToHg19.over.chain.gz',
}
CHROM_SIZES = {
    'hg19': '/cluster/data/hg19/chrom.sizes',
    'hg38': '/cluster/data/hg38/chrom.sizes',
}

COLORS = {
    'pathogenic':             '210,0,0',
    'likely pathogenic':      '245,152,152',
    'uncertain significance': '0,0,136',
    'likely benign':          '213,247,213',
    'benign':                 '0,210,0',
}
DEFAULT_COLOR = '136,136,136'

CLASSIFICATION_CANONICAL = {
    'pathogenic':             'Pathogenic',
    'likely pathogenic':      'Likely pathogenic',
    'uncertain significance': 'Uncertain significance',
    'likely benign':          'Likely benign',
    'benign':                 'Benign',
}

AUTOSQL = """table TP53VCEPCuratedVars
"TP53 VCEP expert-panel-reviewed classifications from ClinGen EvRepo"
   (
   string chrom;           "Reference sequence chromosome or scaffold"
   uint   chromStart;      "Start position in chromosome"
   uint   chromEnd;        "End position in chromosome"
   string name;            "HGVS (NM_000546.6 preferred)"
   uint   score;           "Not used, set to 0"
   char[1] strand;         "Not used, set to ."
   uint   thickStart;      "Same as chromStart"
   uint   thickEnd;        "Same as chromEnd"
   uint   reserved;        "RGB color"
   string classification;  "Final VCEP classification (P/LP/VUS/LB/B)"
   string hgvsc;           "c. notation (NM_000546.6)"
   string hgvsp;           "p. notation if available"
   string varId;           "ClinVar variation ID"
   string caid;            "ClinGen Canonical Allele ID"
   string publishedDate;   "Date published to EvRepo"
   string metCodes;        "Semicolon-separated list of Met evidence codes"
   string notMetCodes;     "Count / summary of Not Met codes"
   lstring _mouseOver;     "HTML mouseover"
   )
"""


def log(msg):
    print(msg, file=sys.stderr)


def bash(cmd):
    result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError("Command failed: {}\n{}".format(cmd, result.stderr))
    return result.stdout


def escape_html(s):
    if not s:
        return ""
    # First escape HTML special chars, then encode any remaining non-ASCII
    # as numeric entities so UCSC hgTracks' mouseover pipeline renders
    # cleanly (raw UTF-8 ends up as mojibake like 'â€"').
    escaped = (str(s).replace('&', '&amp;').replace('<', '&lt;')
                     .replace('>', '&gt;').replace('"', '&quot;'))
    return "".join(
        ch if ord(ch) < 128 else "&#{};".format(ord(ch))
        for ch in escaped
    )


def fetch_evrepo():
    """Download the full TP53 VCEP classification list from EvRepo."""
    log("Fetching TP53 VCEP classifications from EvRepo...")
    req = urllib.request.Request(EVREPO_URL,
                                 headers={'User-Agent': 'UCSC-kent/TP53-hub'})
    with urllib.request.urlopen(req, timeout=60) as resp:
        data = json.loads(resp.read())
    entries = data.get('variantInterpretations', [])
    log("  Found {} TP53 VCEP entries".format(len(entries)))
    return entries


def pick_hgvs(hgvs_list):
    """Select preferred HGVS notations from the HGVS list.

    Returns dict with hg38_g, hg19_g (genomic NC format), hgvsc (NM_000546.6),
    hgvsp (NM_000546.6 with p.), display name.
    """
    out = {'hg38_g': None, 'hg19_g': None,
           'hgvsc': None, 'hgvsp_full': None, 'display': None}
    for h in hgvs_list:
        if h.startswith('NC_000017.11:g.'):
            out['hg38_g'] = h
        elif h.startswith('NC_000017.10:g.'):
            out['hg19_g'] = h
        elif h.startswith('NM_000546.6:c.'):
            out['hgvsc'] = h
        elif h.startswith('NM_000546.5(TP53):c.') or h.startswith('NM_000546.6(TP53):c.'):
            # The prose form includes p. &#8212; e.g.
            # "NM_000546.5(TP53):c.245C>T (p.Pro82Leu)"
            out['hgvsp_full'] = h
            if out['display'] is None:
                out['display'] = h
    if out['display'] is None:
        out['display'] = out['hgvsc'] or (hgvs_list[0] if hgvs_list else 'unknown')
    return out


def hgvs_to_bed_coords(hgvs_g):
    """Convert NC_000017:g.POS[ACGT]>[ACGT] or indel-style g. to (start, end)."""
    if not hgvs_g:
        return None
    m = HGVS_G_RE.match(hgvs_g)
    if m:
        pos = int(m.group(3))
        return (pos - 1, pos)
    m = HGVS_G_INDEL_RE.match(hgvs_g)
    if m:
        start = int(m.group(3))
        end_s = m.group(4)
        end = int(end_s) if end_s else start
        kind = m.group(5)
        # For a dup, the coord is the duplicated region. For del, the deleted range.
        # For ins, zero-width between positions; use start-1 to start in BED.
        if kind == 'ins':
            return (start, start)
        return (start - 1, end)
    return None


def parse_entry(entry):
    """Turn one EvRepo interpretation into our internal dict."""
    hgvs_list = entry.get('hgvs', []) or []
    picks = pick_hgvs(hgvs_list)

    hg38 = hgvs_to_bed_coords(picks['hg38_g'])
    hg19 = hgvs_to_bed_coords(picks['hg19_g'])

    outcome = None
    met = []
    not_met_count = 0
    for g in entry.get('guidelines', []) or []:
        if g.get('outcome'):
            outcome = g['outcome'].get('label')
        for a in g.get('agents', []) or []:
            for c in a.get('evidenceCodes', []) or []:
                label = c.get('label') or ''
                status = (c.get('status') or '').strip().lower()
                if status == 'met':
                    met.append(label)
                elif status == 'not met':
                    not_met_count += 1

    cls_raw = outcome or ''
    cls = CLASSIFICATION_CANONICAL.get(cls_raw.strip().lower(), cls_raw)

    # Pull a short p. from the prose form, if present.
    hgvsp = ''
    if picks['hgvsp_full']:
        m = re.search(r'\(p\.[^)]+\)', picks['hgvsp_full'])
        if m:
            hgvsp = m.group(0)[1:-1]

    hgvsc = ''
    if picks['hgvsc']:
        hgvsc = picks['hgvsc']
    elif picks['hgvsp_full']:
        # Fallback to c. from the prose form
        m = re.search(r'c\.[^\s(]+', picks['hgvsp_full'])
        if m:
            hgvsc = 'NM_000546.6:' + m.group(0)

    return {
        'var_id': entry.get('variationId', ''),
        'caid': entry.get('caid', ''),
        'published': entry.get('publishedDate', ''),
        'classification': cls,
        'hgvsc': hgvsc,
        'hgvsp': hgvsp,
        'display': picks['display'],
        'hg38_bed': hg38,          # (start,end) or None
        'hg19_bed': hg19,
        'met_codes': met,
        'not_met_count': not_met_count,
    }


def liftover_coords(coords, chain):
    if not coords:
        return {}
    with tempfile.NamedTemporaryFile(mode='w', suffix='.bed', delete=False) as f:
        in_bed = f.name
        for vid, (chrom, s, e) in coords.items():
            f.write("{}\t{}\t{}\t{}\n".format(chrom, s, e, vid))
    out_bed = in_bed.replace('.bed', '.lifted.bed')
    un_bed = in_bed.replace('.bed', '.unmapped.bed')
    try:
        bash("liftOver {} {} {} {} 2>/dev/null".format(in_bed, chain, out_bed, un_bed))
    except Exception:
        pass
    lifted = {}
    if os.path.exists(out_bed):
        with open(out_bed) as f:
            for line in f:
                flds = line.strip().split('\t')
                if len(flds) >= 4:
                    lifted[flds[3]] = (flds[0], int(flds[1]), int(flds[2]))
    for p in [in_bed, out_bed, un_bed]:
        if os.path.exists(p):
            os.remove(p)
    return lifted


def create_bed(variants, assembly):
    lines = []
    unmapped = []
    # Which coord set is native on this assembly?
    native_key = 'hg38_bed' if assembly == 'hg38' else 'hg19_bed'
    other_key = 'hg19_bed' if assembly == 'hg38' else 'hg38_bed'
    chain = LIFTOVER_CHAINS['hg19_to_hg38'] if assembly == 'hg38' \
        else LIFTOVER_CHAINS['hg38_to_hg19']

    # Build needs_liftover map for variants missing native coords.
    needs = {}
    for v in variants:
        if v[native_key] is None and v[other_key] is not None:
            other_start, other_end = v[other_key]
            needs[v['var_id']] = ('chr17', other_start, other_end)
    lifted = liftover_coords(needs, chain) if needs else {}
    if needs:
        log("  liftOver ({} -> {}): {} lifted of {} needing".format(
            'hg19' if assembly == 'hg38' else 'hg38',
            assembly, len(lifted), len(needs)))

    for v in variants:
        start, end = None, None
        if v[native_key] is not None:
            start, end = v[native_key]
            chrom = 'chr17'
        elif v['var_id'] in lifted:
            chrom, start, end = lifted[v['var_id']]
        else:
            unmapped.append(v)
            continue

        color = COLORS.get((v['classification'] or '').strip().lower(), DEFAULT_COLOR)
        cv_url = "https://www.ncbi.nlm.nih.gov/clinvar/variation/{}/".format(v['var_id'])
        met_summary = ", ".join(v['met_codes']) if v['met_codes'] else "(none Met)"
        mo = (
            "<b>Final &#8212; VCEP ClinVar submission</b><br>"
            "<b>Variant:</b> {disp}<br>"
            "<b>Classification:</b> {cls}<br>"
            "<b>ClinGen CAid:</b> {caid}<br>"
            "<b>ClinVar ID:</b> <a href=\"{url}\" target=\"_blank\">{vid}</a><br>"
            "<b>Published:</b> {pub}<br>"
            "<b>Met evidence codes:</b> {met}<br>"
            "<b>Not Met codes:</b> {notmet}<br>"
            "<b>Source:</b> ClinGen Evidence Repository (EvRepo)"
        ).format(
            disp=escape_html(v['display']),
            cls=escape_html(v['classification']),
            caid=escape_html(v['caid']),
            url=cv_url, vid=v['var_id'],
            pub=escape_html(v['published']),
            met=escape_html(met_summary),
            notmet=v['not_met_count'],
        )
        # Use HGVSp short if available, else c. notation, else full display.
        # ASCII-encode every free-text field to avoid UCSC mouseover mojibake.
        def _safe(s):
            if s is None:
                return ''
            return "".join(
                ch if ord(ch) < 128 else "&#{};".format(ord(ch)) for ch in str(s)
            )
        name = v['hgvsp'] or v['hgvsc'] or v['display']
        if len(name) > 200:
            name = name[:197] + "..."
        lines.append("\t".join([
            chrom, str(start), str(end),
            _safe(name), '0', '.',
            str(start), str(end),
            color,
            v['classification'] or 'Unknown',
            _safe(v['hgvsc'] or ''),
            _safe(v['hgvsp'] or ''),
            v['var_id'],
            _safe(v['caid']),
            v['published'],
            _safe("; ".join(v['met_codes'])),
            str(v['not_met_count']),
            mo,
        ]))
    return lines, unmapped


def build_assembly(variants, assembly, outdir):
    log("\n=== {} ===".format(assembly))
    entries, unmapped = create_bed(variants, assembly)
    log("  mapped: {}  unmapped: {}".format(len(entries), len(unmapped)))
    if not entries:
        return None
    bed = os.path.join(outdir, "TP53VCEPCuratedVars_{}.bed".format(assembly))
    with open(bed, 'w') as f:
        f.write("\n".join(entries) + "\n")
    bash("sort -k1,1 -k2,2n {0} -o {0}".format(bed))
    as_file = os.path.join(outdir, "TP53VCEPCuratedVars.as")
    bb = os.path.join(outdir, "TP53VCEPCuratedVars{}.bb".format(assembly.capitalize()))
    bash("bedToBigBed -as={} -type=bed9+9 -tab {} {} {}".format(
        as_file, bed, CHROM_SIZES[assembly], bb))
    log("  wrote {}".format(bb))
    return bb


def write_tsv(variants, path):
    cols = ['var_id', 'caid', 'display', 'hgvsc', 'hgvsp', 'classification',
            'published', 'met_codes', 'not_met_count',
            'hg38_start', 'hg38_end', 'hg19_start', 'hg19_end']
    with open(path, 'w') as f:
        f.write("\t".join(cols) + "\n")
        for v in variants:
            h38 = v['hg38_bed'] or (None, None)
            h19 = v['hg19_bed'] or (None, None)
            row = [
                v['var_id'], v['caid'], v['display'], v['hgvsc'] or '',
                v['hgvsp'] or '', v['classification'] or '', v['published'],
                ';'.join(v['met_codes']), str(v['not_met_count']),
                str(h38[0] if h38[0] is not None else ''),
                str(h38[1] if h38[1] is not None else ''),
                str(h19[0] if h19[0] is not None else ''),
                str(h19[1] if h19[1] is not None else ''),
            ]
            f.write("\t".join(row) + "\n")


def backfill_clinvar_for_unlinked(variants):
    """For records that came in without a ClinVar var_id, look them up by
    HGVSc and backfill var_id + hgvsp from ClinVar esummary. Some EvRepo
    records (e.g. complex indels like c.322_332delinsATTCA) come through
    the variantInterpretations endpoint with hgvsp empty and no variationId
    populated; we recover both via ClinVar's esearch by VarName + esummary.
    """
    eutils_search = "https://eutils.ncbi.nlm.nih.gov/entrez/eutils/esearch.fcgi"
    eutils_summary = "https://eutils.ncbi.nlm.nih.gov/entrez/eutils/esummary.fcgi"
    fixed = 0
    # Pace requests to stay well under NCBI's 3 req/sec limit and avoid 429s
    # on first-hit cold cache.
    time.sleep(1.0)
    for v in variants:
        if v['var_id'] or not v['hgvsc']:
            continue
        m = re.match(r'NM_000546(?:\.\d+)?:(c\.[^\s(]+)', v['hgvsc'])
        if not m:
            continue
        c_form = m.group(1)
        success = False
        for attempt in range(3):
            try:
                url = (eutils_search +
                       "?db=clinvar&retmode=json&term=" +
                       urllib.parse.quote('"{}"[VarName] AND tp53[gene]'.format(c_form)))
                with urllib.request.urlopen(url, timeout=20) as resp:
                    hits = json.loads(resp.read())['esearchresult']['idlist']
                if not hits:
                    break
                vid = hits[0]
                time.sleep(0.5)
                url = "{}?db=clinvar&id={}&retmode=json".format(eutils_summary, vid)
                with urllib.request.urlopen(url, timeout=20) as resp:
                    summ = json.loads(resp.read())['result'][vid]
                v['var_id'] = vid
                title = summ.get('title', '')
                mp = re.search(r'\(p\.[^)]+\)', title)
                if mp and not v['hgvsp']:
                    v['hgvsp'] = mp.group(0)[1:-1]
                fixed += 1
                success = True
                break
            except urllib.error.HTTPError as e:
                if e.code == 429 and attempt < 2:
                    time.sleep(2.0 * (attempt + 1))
                    continue
                log("  backfill skipped for {}: {}".format(c_form, e))
                break
            except Exception as e:
                log("  backfill skipped for {}: {}".format(c_form, e))
                break
        if success:
            time.sleep(0.5)
    if fixed:
        log("Backfilled ClinVar var_id for {} previously-unlinked records".format(fixed))


def fetch_all_variants():
    entries = fetch_evrepo()
    variants = [parse_entry(e) for e in entries]
    variants = [v for v in variants if v['classification']]
    log("Parsed {} variants with classifications".format(len(variants)))
    backfill_clinvar_for_unlinked(variants)
    return variants


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('-o', '--output-dir',
                   default=os.path.dirname(os.path.abspath(__file__)),
                   help='Output directory (default: script directory)')
    p.add_argument('--db', action='append',
                   help='hg38 or hg19 (repeat); default both')
    args = p.parse_args()
    outdir = args.output_dir
    os.makedirs(outdir, exist_ok=True)
    dbs = args.db if args.db else ['hg38', 'hg19']

    variants = fetch_all_variants()
    if not variants:
        log("ERROR: no variants parsed from EvRepo")
        sys.exit(1)

    tsv = os.path.join(outdir, "tp53_vcep_clinvar_variants.tsv")
    write_tsv(variants, tsv)
    log("TSV: {}".format(tsv))

    as_file = os.path.join(outdir, "TP53VCEPCuratedVars.as")
    with open(as_file, 'w') as f:
        f.write(AUTOSQL)

    for db in dbs:
        build_assembly(variants, db, outdir)

    from collections import Counter
    cls_counts = Counter(v['classification'] for v in variants)
    log("\nClassification counts:")
    for c, n in cls_counts.most_common():
        log("  {}: {}".format(c, n))


if __name__ == "__main__":
    main()
