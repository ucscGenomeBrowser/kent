#!/usr/bin/env python3
"""
B.11: Computable ACMG Criteria Summary track (NOT a VCEP classification).

For every gnomAD-observed variant in the 8 cardiomyopathy gene CDS regions &#177;20 nt
splice padding, lists the subset of ACMG/AMP evidence codes a hub can compute
automatically. No overall classification is calculated:
  - BA1 / BS1 / PM2_Supporting  (gnomAD v4.1 FAF95; B.3)
  - PP3 / BP4                   (REVEL; B.4; missense only, per hgVai consequence)
  - PM1                         (B.1 hotspot regions; HCM-scoped; NOT combined with PM5)
  - PS1 / PM5                   (EvRepo P/LP reference, LEAVE-ONE-OUT; see caveat below)
  - PM4                         (NMD-escaping truncating variants, non-MYBPC3; CSpec disease-specific)
  - BP7                         (synonymous + SpliceAI no-impact + not conserved)
  - Splice safety net           (SpliceAI &#8805; 0.20 overrides a benign-leaning call to VUS)
  - HCM/DCM tag                 (MYH7, TNNT2: PM1 is HCM-calibrated)

CONSEQUENCE/CODON/AA come from the Phase-1 hgVai annotation TSV (cmpVCEPAnnotate).

HONESTY / KNOWN LIMITS (surfaced to the VCEP, not hidden):
  * This mockup CANNOT compute the clinical/functional codes (PS2, PS3, PS4, PP1,
    PP4, BS3, BS4). Many true P/LP calls rest on those, so this track structurally
    under-calls pathogenicity. It is best read as a benign/VUS-axis + "flag for
    expert review" aid, NOT an accuracy claim. No concordance metric is asserted.
  * PS1/PM5 reference set: the CSpec names NO database ("apply per Richards 2015").
    We use the VCEP EvRepo P/LP set with LEAVE-ONE-OUT (a variant cannot earn PS1/PM5
    from its own EvRepo entry). The choice of reference DB is an open VCEP question.
  * PM4 strength (MOD vs SUP) and BP7 conservation metric/threshold are not fixed by
    the CSpec &#8212; surfaced as VCEP questions; provisional choices are flagged in-line.

Outputs:
  cmpVCEPProvisionalClass/cmpVCEPProvisionalClass.as
  cmpVCEPProvisionalClass/cmpVCEPProvisionalClassHg{38,19}.bed + .bb
"""

import argparse, json, os, re, subprocess, sys
from collections import defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cmpVCEPClinDomains import parse_mane_record

OUR_GENES = ['MYH7', 'MYBPC3', 'TNNT2', 'TNNI3', 'TPM1', 'ACTC1', 'MYL2', 'MYL3']

WORKDIR = '/hive/users/lrnassar/claude/RM37446'
B3_BED = f'{WORKDIR}/cmpVCEPAFfrequencies/cmpVCEPAFfrequenciesHg38.bed'
B4_BED = f'{WORKDIR}/cmpVCEPRevel/cmpVCEPRevelHg38.bed'
B1_BED = f'{WORKDIR}/cmpVCEPClinDomains/cmpVCEPClinDomainsHg38.bed'
EVREPO_JSON = f'{WORKDIR}/cmp_downloads/erepo/cardiomyopathyVCEP_classifications.json'
ANNOT_TSV = f'{WORKDIR}/cmpVCEPAnnotate/cmpVCEPAnnotations.hg38.tsv'
SPLICEAI_BB = '/gbdb/hg38/bbi/spliceAi.bb'
PHYLOP_BW = '/gbdb/hg38/multiz470way/phyloP470way.bw'

# Per-gene thresholds (from CSpec &#8212; NOT invented here)
BS1_THRESHOLDS = {'MYBPC3': 0.0002}
DEFAULT_BS1 = 0.0001
BA1_THRESHOLD = 0.001
PM2_SUPPORTING_THRESHOLD = 0.00004
SPLICE_SAFETY_THRESHOLD = 0.20   # SpliceAI delta for safety-net override (standard recall threshold)
# --- provisional operationalizations the CSpec leaves open (flagged as VCEP questions) ---
BP7_SPLICE_MAX = 0.20            # SpliceAI "no predicted impact" (standard recall threshold)
BP7_PHYLOP_MAX = 0.0             # phyloP470way <= 0 == not under purifying selection (PROVISIONAL &#8212; VCEP to confirm metric+cutoff)

# NC_ accession (hg38) &#8594; chrom, for parsing EvRepo genomic HGVS (leave-one-out keys)
NC_HG38 = {
    'NC_000001.11': 'chr1', 'NC_000003.12': 'chr3', 'NC_000011.10': 'chr11',
    'NC_000012.12': 'chr12', 'NC_000014.9': 'chr14', 'NC_000015.10': 'chr15',
    'NC_000019.10': 'chr19',
}
# Variants with established splicing impact &#8212; excluded from the PS1/PM5 reference per GN002 PS1.
PS1_SPLICE_EXCLUDE = {'NM_000256.3:c.2308G>A'}
NC_G_RE = re.compile(r'^(NC_\d+\.\d+):g\.(\d+)([ACGT]+)>([ACGT]+)$')
PROT_MISSENSE_RE = re.compile(r'p\.([A-Z][a-z]{2})(\d+)([A-Z][a-z]{2})')

AA3TO1 = {
    'Ala': 'A', 'Arg': 'R', 'Asn': 'N', 'Asp': 'D', 'Cys': 'C', 'Gln': 'Q',
    'Glu': 'E', 'Gly': 'G', 'His': 'H', 'Ile': 'I', 'Leu': 'L', 'Lys': 'K',
    'Met': 'M', 'Phe': 'F', 'Pro': 'P', 'Ser': 'S', 'Thr': 'T', 'Trp': 'W',
    'Tyr': 'Y', 'Val': 'V', 'Ter': '*',
}

TRACK_COLOR = '91,107,122'   # neutral slate; no classification encoded (evidence-only track)

CHROM_SIZES = {'hg38': '/cluster/data/hg38/chrom.sizes', 'hg19': '/cluster/data/hg19/chrom.sizes'}
LIFTOVER_HG38_TO_HG19 = '/cluster/data/hg38/bed/liftOver/hg38ToHg19.over.chain.gz'

AUTOSQL = """table cmpVCEPProvisionalClass
"Computable ACMG criteria per variant (evidence summary; NOT a VCEP classification)"
    (
    string  chrom;          "Chromosome"
    uint    chromStart;     "Position"
    uint    chromEnd;       "End"
    string  name;           "Display name"
    uint    score;          "0"
    char[1] strand;         "Strand"
    uint    thickStart;     "Same"
    uint    thickEnd;       "Same"
    uint    itemRgb;        "Display color (neutral; no classification encoded)"
    string  gene;           "Gene"
    string  refAllele;      "Ref"
    string  altAllele;      "Alt"
    string  variantKind;    "Predicted consequence (hgVai)"
    string  appliedCodes;   "Computable ACMG codes triggered (semicolon-separated, with strengths)"
    string  diseaseTag;     "HCM/DCM phenotype scoping note (MYH7, TNNT2)"
    string  codeNotes;      "Suppressed/contested codes (e.g. PM5 not combined with PM1)"
    string  splice_safety;  "yes if SpliceAI >= 0.20 (possible splice impact; informational)"
    lstring _mouseOver;     "Tooltip"
    )
"""


# ============================================================
# Combination rules &#8212; transcribed verbatim from CSpec GN002
# ============================================================

# RETIRED 2026-07-08 (per CM VCEP chair L. Bronicki): this track no longer computes an overall
# ACMG classification, only the computable codes that fire. The GN002 combining logic below is
# kept for reference and is intentionally NOT called.
def classify(codes):
    """Apply the Cardiomyopathy CSpec (GN002) combining rules.
    `codes` is an iterable of code strings carrying explicit strengths where relevant,
    e.g. {'PM1_Moderate', 'PP3_Supporting', 'PS1_Strong', 'BA1', 'BS1_Strong'}.
    Returns (classification, rule_match).

    Strong:     PS1, PS2, PS3, PS4, PP1_Strong
    Moderate:   PS3_Moderate, PS4_Moderate, PM1, PM4(_Moderate), PM5, PM6, PP1_Moderate
    Supporting: PS3_Supporting, PS4_Supporting, PM2_Supporting, PM5_Supporting, PP1, PP3, PM4_Supporting
    Benign:     BA1 (stand-alone); BS1/BS3/BS4 (strong); BP4/BP7 (supporting)
    """
    p_s = p_m = p_sup = 0
    b_sa = b_s = b_sup = 0

    for code in codes:
        c, _, strength = code.partition('_')
        if not strength:
            if c in ('PS1', 'PS2', 'PS3', 'PS4'): strength = 'Strong'
            elif c in ('PM1', 'PM5', 'PM6'): strength = 'Moderate'
            elif c in ('PM2', 'PM4'): strength = 'Supporting'   # CSpec downgrades
            elif c in ('PP1', 'PP3'): strength = 'Supporting'
            elif c == 'BA1': strength = 'StandAlone'
            elif c in ('BS1', 'BS3', 'BS4'): strength = 'Strong'
            elif c in ('BP4', 'BP7'): strength = 'Supporting'
            else: continue

        if c[0] == 'P':
            if strength == 'Strong': p_s += 1
            elif strength == 'Moderate': p_m += 1
            elif strength == 'Supporting': p_sup += 1
        else:
            if strength == 'StandAlone': b_sa += 1
            elif strength == 'Strong': b_s += 1
            elif strength == 'Supporting': b_sup += 1

    # Benign side
    if b_sa >= 1:
        return 'Benign', 'BA1 stand-alone'
    if b_s >= 2:
        return 'Benign', f'{b_s} Strong-benign'
    if b_s == 1 and b_sup >= 1:
        return 'Likely Benign', f'1 Strong + {b_sup} Supporting (benign)'
    if b_sup >= 2:
        return 'Likely Benign', f'{b_sup} Supporting (benign)'
    # CSpec GN002 BS1 carve-out: "BS1 may only be used as standalone evidence to classify a
    # variant as Likely Benign in the absence of conflicting data."
    if b_s == 1 and (p_s + p_m + p_sup) == 0:
        return 'Likely Benign', 'BS1 standalone (CSpec carve-out; no conflicting pathogenic data)'

    # Pathogenic (CSpec GN002): >=2 S; 1S+>=3M; 1S+2M+>=2Sup; 1S+1M+>=4Sup
    if p_s >= 2:
        return 'Pathogenic', '>=2 Strong'
    if p_s == 1 and p_m >= 3:
        return 'Pathogenic', '1 Strong + >=3 Moderate'
    if p_s == 1 and p_m == 2 and p_sup >= 2:
        return 'Pathogenic', '1 Strong + 2 Moderate + >=2 Supporting'
    if p_s == 1 and p_m == 1 and p_sup >= 4:
        return 'Pathogenic', '1 Strong + 1 Moderate + >=4 Supporting'

    # Likely Pathogenic (CSpec GN002): 1S+1-2M; 1S+>=2Sup; >=3M; 2M+>=2Sup; 1M+>=4Sup
    if p_s == 1 and p_m >= 1:
        return 'Likely Pathogenic', f'1 Strong + {p_m} Moderate'
    if p_s == 1 and p_sup >= 2:
        return 'Likely Pathogenic', '1 Strong + >=2 Supporting'
    if p_m >= 3:
        return 'Likely Pathogenic', f'{p_m} Moderate'
    if p_m == 2 and p_sup >= 2:
        return 'Likely Pathogenic', '2 Moderate + >=2 Supporting'
    if p_m == 1 and p_sup >= 4:
        return 'Likely Pathogenic', '1 Moderate + >=4 Supporting'

    return 'Uncertain Significance', f'no rule fires (P:{p_s}S+{p_m}M+{p_sup}Sup; B:{b_sa}SA+{b_s}S+{b_sup}Sup)'


# ============================================================
# Source loaders
# ============================================================

def load_b3_variants():
    rows = []
    for line in open(B3_BED):
        f = line.rstrip('\n').split('\t')
        if len(f) < 18:
            continue
        rows.append({'chrom': f[0], 'start': int(f[1]), 'end': int(f[2]), 'strand': f[5],
                     'gene': f[9], 'ref': f[10], 'alt': f[11], 'faf95': float(f[12]),
                     'af_code': f[15]})
    print(f'  B.3 universe: {len(rows)} variants', file=sys.stderr)
    return rows


def load_b4_revel_lookup():
    lookup = {}
    for line in open(B4_BED):
        f = line.rstrip('\n').split('\t')
        if len(f) < 14:
            continue
        lookup[(f[0], int(f[1]), f[10])] = (f[12], f[11])   # (chrom,start,alt) -> (code, REVEL score)
    print(f'  REVEL lookup: {len(lookup)} keys', file=sys.stderr)
    return lookup


def load_pm1_intervals():
    intervals = []
    for line in open(B1_BED):
        f = line.rstrip('\n').split('\t')
        intervals.append((f[0], int(f[1]), int(f[2])))
    print(f'  PM1 intervals: {len(intervals)}', file=sys.stderr)
    return intervals


def in_pm1_region(chrom, pos0, intervals):
    for c, s, e in intervals:
        if c == chrom and s <= pos0 < e:
            return True
    return False


def load_annotation():
    """Phase-1 hgVai annotations keyed by (chrom, pos1, ref, alt)."""
    ann = {}
    with open(ANNOT_TSV) as fh:
        for line in fh:
            if line.startswith('#'):
                continue
            f = line.rstrip('\n').split('\t')
            (chrom, pos, ref, alt, gene, so, ppos, aaref, aaalt, codon,
             exn, ext, hgvsp, cdna) = f[:14]
            ann[(chrom, int(pos), ref, alt)] = {
                'gene': gene, 'so': set(so.split(',')) if so else set(),
                'codon': int(ppos) if ppos.isdigit() else None,
                'aaRef': aaref, 'aaAlt': aaalt,
                'exonNum': int(exn) if exn.isdigit() else None,
                'exonTotal': int(ext) if ext.isdigit() else None,
                'hgvsp': hgvsp,
                'cdnaPos': int(cdna) if cdna.isdigit() else None,
            }
    print(f'  annotations: {len(ann)}', file=sys.stderr)
    return ann


def load_evrepo_reference():
    """EvRepo P/LP MISSENSE reference for PS1/PM5, each with its genomic key
    (for leave-one-out). Returns list of dicts {gkey, gene, codon, alt_aa1}."""
    data = json.load(open(EVREPO_JSON))
    ref = []
    for v in data['variantInterpretations']:
        outcome = v['guidelines'][0]['outcome']['label']
        if outcome not in ('Pathogenic', 'Likely Pathogenic'):
            continue
        gene = v['gene']['label']
        hgvs_list = v['hgvs']
        # CSpec GN002 PS1 caveat: variants with an established splicing impact must NOT seed
        # PS1/PM5 for other variants with the same amino-acid change (the named example is
        # MYBPC3 c.2308G>A p.Asp770Asn). Exclude such entries from the reference.
        if any(s in h for h in hgvs_list for s in PS1_SPLICE_EXCLUDE):
            continue
        # genomic key (hg38) from NC_ HGVS
        gkey = None
        for h in hgvs_list:
            m = NC_G_RE.match(h)
            if m and m.group(1) in NC_HG38:
                gkey = (NC_HG38[m.group(1)], int(m.group(2)), m.group(3), m.group(4))
                break
        # protein change (missense) from p. HGVS
        codon = alt_aa1 = None
        for h in hgvs_list:
            m = PROT_MISSENSE_RE.search(h)
            if m and m.group(3) in AA3TO1:
                codon = int(m.group(2))
                alt_aa1 = AA3TO1[m.group(3)]
                break
        if codon is not None and alt_aa1 is not None:
            ref.append({'gkey': gkey, 'gene': gene, 'codon': codon, 'alt_aa1': alt_aa1})
    print(f'  EvRepo P/LP missense reference: {len(ref)} entries', file=sys.stderr)
    return ref


def batch_spliceai(regions):
    """Per gene region: (chrom, pos1, ref, alt) -> max SpliceAI delta (bed9+4: AIscore=col9, name='ref>alt')."""
    sa = {}
    for chrom, start, end in regions:
        try:
            out = subprocess.check_output(['bigBedToBed', f'-chrom={chrom}', f'-start={start}',
                                           f'-end={end}', SPLICEAI_BB, 'stdout'],
                                          text=True, stderr=subprocess.DEVNULL)
        except subprocess.CalledProcessError:
            continue
        for line in out.splitlines():
            f = line.split('\t')
            if len(f) < 10 or '>' not in f[3]:
                continue
            ref, alt = f[3].split('>', 1)
            pos1 = int(f[2])   # chromEnd == 1-based SNV pos
            try:
                score = float(f[9])
            except ValueError:
                continue
            k = (chrom, pos1, ref, alt)
            if score > sa.get(k, -1):
                sa[k] = score
    print(f'  SpliceAI entries: {len(sa)}', file=sys.stderr)
    return sa


def batch_phylop(regions):
    """Per gene region: (chrom, pos1) -> phyloP470way value."""
    pp = {}
    for chrom, start, end in regions:
        try:
            out = subprocess.check_output(['bigWigToBedGraph', PHYLOP_BW, 'stdout',
                                           f'-chrom={chrom}', f'-start={start}', f'-end={end}'],
                                          text=True, stderr=subprocess.DEVNULL)
        except subprocess.CalledProcessError:
            continue
        for line in out.splitlines():
            c, s, e, val = line.split('\t')
            s, e, val = int(s), int(e), float(val)
            for p0 in range(s, e):
                pp[(chrom, p0 + 1)] = val   # 1-based
    print(f'  phyloP positions: {len(pp)}', file=sys.stderr)
    return pp


TRUNCATING_SO = {'stop_gained', 'frameshift_variant'}


def variant_kind(so):
    """Most-relevant consequence label for display."""
    for k in ('stop_gained', 'frameshift_variant', 'stop_lost', 'splice_acceptor_variant',
              'splice_donor_variant', 'missense_variant', 'inframe_deletion', 'inframe_insertion',
              'initiator_codon_variant', 'splice_region_variant', 'synonymous_variant',
              'intron_variant', '5_prime_UTR_variant', '3_prime_UTR_variant'):
        if k in so:
            return k
    return ','.join(sorted(so)) if so else 'unknown'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--db', action='append', required=True, choices=['hg38', 'hg19'])
    ap.add_argument('--output-dir', required=True)
    ap.add_argument('--no-spliceai', action='store_true', help='Skip SpliceAI/BP7-conservation (debug only)')
    args = ap.parse_args()

    out_dir = os.path.join(args.output_dir, 'cmpVCEPProvisionalClass')
    os.makedirs(out_dir, exist_ok=True)
    print('  [B.11 Computable ACMG Criteria Summary]')

    b3 = load_b3_variants()
    revel = load_b4_revel_lookup()
    pm1 = load_pm1_intervals()
    ann = load_annotation()
    evref = load_evrepo_reference()

    regions = []
    nmd_junction = {}   # gene -> cDNA coord of the last exon-exon junction (transcript len - last exon len)
    for gene in OUR_GENES:
        m = parse_mane_record(gene)
        regions.append((m['chrom'], m['chromStart'], m['chromEnd']))
        bs = m['blockSizes']
        last_exon = bs[0] if m['strand'] == '-' else bs[-1]   # 3'-most transcript exon
        nmd_junction[gene] = sum(bs) - last_exon
    if args.no_spliceai:
        spliceai, phylop = {}, {}
    else:
        spliceai = batch_spliceai(regions)
        phylop = batch_phylop(regions)

    n_features = 0
    code_counts = defaultdict(int)
    bed_lines = []

    for v in b3:
        gene = v['gene']
        if gene not in OUR_GENES:
            continue
        chrom, pos1, ref, alt = v['chrom'], v['start'] + 1, v['ref'], v['alt']
        a = ann.get((chrom, pos1, ref, alt), {})
        so = a.get('so', set())
        is_missense = 'missense_variant' in so
        is_synonymous = 'synonymous_variant' in so

        codes = set()
        code_why = {}
        notes = []
        faf = v['faf95']

        # gnomAD AF (B.3)
        if v['af_code'] == 'BA1':
            codes.add('BA1')
            code_why['BA1'] = f'gnomAD FAF95 (popmax) {faf:.2e} &#8805; 0.001'
        elif v['af_code'] == 'BS1':
            codes.add('BS1_Strong')
            thr = '0.0002' if gene == 'MYBPC3' else '0.0001'
            code_why['BS1_Strong'] = f'gnomAD FAF95 (popmax) {faf:.2e} &#8805; {thr}'
        elif v['af_code'] == 'PM2_supporting':
            codes.add('PM2_Supporting')
            code_why['PM2_Supporting'] = f'gnomAD FAF95 (popmax) {faf:.2e} &#8804; 4e-05 (rare)'

        # REVEL PP3/BP4 &#8212; missense only
        if is_missense:
            rc = revel.get((chrom, v['start'], alt))
            if rc:
                code, score = rc
                codes.add(code)
                thr = '&#8805; 0.70' if code.startswith('PP3') else '&#8804; 0.40'
                code_why[code] = f'REVEL {score} ({thr})'

        # PM1 hotspot (HCM-calibrated)
        pm1_hit = in_pm1_region(chrom, v['start'], pm1)
        if pm1_hit:
            codes.add('PM1_Moderate')
            code_why['PM1_Moderate'] = f'in the {gene} PM1 hotspot region (HCM-calibrated)'

        # PS1 / PM5 &#8212; EvRepo P/LP reference, LEAVE-ONE-OUT (exclude self by genomic key)
        if is_missense and a.get('codon') and a.get('aaAlt'):
            codon, aaalt = a['codon'], a['aaAlt']
            gkey = (chrom, pos1, ref, alt)
            ps1 = any(e['gene'] == gene and e['codon'] == codon and e['alt_aa1'] == aaalt
                      and e['gkey'] != gkey for e in evref)
            pm5 = any(e['gene'] == gene and e['codon'] == codon and e['alt_aa1'] != aaalt
                      and e['gkey'] != gkey for e in evref)
            if ps1:
                codes.add('PS1_Strong')
                code_why['PS1_Strong'] = f'same amino-acid change as a VCEP EvRepo P/LP variant at codon {codon}'
            if pm5:
                codes.add('PM5_Moderate')
                code_why['PM5_Moderate'] = f'a different missense at codon {codon} is classified P/LP in the VCEP EvRepo set'

        # PM4 &#8212; NMD-escaping truncating, non-MYBPC3 (CSpec disease-specific; PVS1 N/A for these genes).
        # NMD escapes if the PTC is in the last exon OR within 50 nt of the last exon-exon junction
        # (J = transcript length - last exon length); cDNA position is in transcript orientation.
        if gene != 'MYBPC3' and (so & TRUNCATING_SO):
            cdna, J = a.get('cdnaPos'), nmd_junction.get(gene)
            nmd_escape = (a.get('exonNum') is not None and a.get('exonNum') == a.get('exonTotal')) \
                or (cdna is not None and J is not None and cdna > J - 50)
            if nmd_escape:
                codes.add('PM4_Supporting')
                last = a.get('exonNum') == a.get('exonTotal')
                where = 'last exon' if last else 'within 50 nt of the last exon-exon junction'
                code_why['PM4_Supporting'] = f'NMD-escaping truncating variant ({where})'
        elif gene != 'MYBPC3' and 'stop_lost' in so:
            codes.add('PM4_Supporting')
            code_why['PM4_Supporting'] = 'stop-loss variant'

        # BP7 &#8212; synonymous + SpliceAI no-impact + not conserved (conservation cutoff PROVISIONAL)
        sa_score = spliceai.get((chrom, pos1, ref, alt), 0.0)
        if is_synonymous and not args.no_spliceai:
            phy = phylop.get((chrom, pos1))
            if sa_score < BP7_SPLICE_MAX and phy is not None and phy <= BP7_PHYLOP_MAX:
                codes.add('BP7_Supporting')
                code_why['BP7_Supporting'] = (f'synonymous; SpliceAI {sa_score:.2f} &lt; {BP7_SPLICE_MAX}; '
                                              f'phyloP {phy:.2f} &#8804; 0 (conservation cutoff provisional)')

        # CSpec exclusion: PM1 must NOT be combined with PM5. GN002 PM5: "use of PM5 is most
        # appropriate since it is variant specific" &#8594; keep PM5, drop PM1.
        if 'PM1_Moderate' in codes and 'PM5_Moderate' in codes:
            codes.discard('PM1_Moderate')
            notes.append('<b>PM1 suppressed:</b> CSpec says PM5 (variant-specific) is preferred over PM1')
        # CSpec is silent on PM1+PS1; flag as possible double-count for VCEP (do not suppress)
        if 'PM1_Moderate' in codes and 'PS1_Strong' in codes:
            notes.append('<b>Note:</b> PM1+PS1 co-occur &#8212; possible double-counting (VCEP question)')

        # Splice signal (informational): SpliceAI >= 0.20 flags possible splice impact.
        # No longer overrides a call &#8212; this track computes no overall classification.
        splice_safety = 'no'
        if sa_score >= SPLICE_SAFETY_THRESHOLD:
            notes.append(f'<b>Splice flag:</b> SpliceAI {sa_score:.2f} &gt;= {SPLICE_SAFETY_THRESHOLD} (possible splice impact; informational)')
            splice_safety = 'yes'

        n_features += 1
        for c in codes:
            code_counts[c] += 1
        color = TRACK_COLOR

        disease_tag = ''
        if gene in ('MYH7', 'TNNT2'):
            disease_tag = 'PM1 HCM-calibrated' if pm1_hit else 'HCM/DCM'

        applied_str = ';'.join(sorted(codes)) or 'no codes'
        notes_str = ' | '.join(notes)
        kind = variant_kind(so)

        mo = [f'<b>Computable ACMG Criteria Summary</b> (NOT a VCEP classification)<br>',
              f'<b>{gene}</b> {chrom}:{pos1} {ref}&gt;{alt}']
        if a.get('hgvsp'):
            mo.append(f' &nbsp;<i>{a["hgvsp"]}</i>')
        mo.append(f'<br><b>Consequence:</b> {kind}<br>')
        if codes:
            mo.append('<b>Computable codes triggered:</b><br>')
            for c in sorted(codes):
                why = code_why.get(c, '')
                mo.append(f'&nbsp;&nbsp;<b>{c}:</b> {why}<br>' if why else f'&nbsp;&nbsp;<b>{c}</b><br>')
        else:
            mo.append('<b>Computable codes triggered:</b> none<br>')
        if notes_str:
            mo.append(f'<span style="color:#a00">{notes_str}</span><br>')
        mo.append('<br><i>Shows only the computable ACMG codes that are triggered. Excludes clinical/functional '
                  'PS2/PS3/PS4/PP1/PP4/BS3/BS4.</i>')
        mouseover = ''.join(mo)

        name = f'{gene}_{pos1}_{ref}>{alt}'
        bed_lines.append('\t'.join([
            chrom, str(v['start']), str(v['end']), name, '0', v['strand'],
            str(v['start']), str(v['end']), color, gene, ref, alt, kind,
            applied_str, disease_tag, notes_str, splice_safety, mouseover,
        ]))

    print(f'  features: {n_features}')
    print(f'  code firing counts: {dict(sorted(code_counts.items()))}')

    bed_lines.sort(key=lambda l: (l.split('\t')[0], int(l.split('\t')[1])))

    as_path = os.path.join(out_dir, 'cmpVCEPProvisionalClass.as')
    with open(as_path, 'w') as f:
        f.write(AUTOSQL)

    hg38_bed = os.path.join(out_dir, 'cmpVCEPProvisionalClassHg38.bed')
    with open(hg38_bed, 'w') as f:
        f.write('\n'.join(bed_lines) + '\n')
    print(f'  wrote {len(bed_lines)} BED features -> {hg38_bed}')

    if 'hg38' in args.db:
        hg38_bb = os.path.join(out_dir, 'cmpVCEPProvisionalClassHg38.bb')
        subprocess.run(['bedToBigBed', '-tab', '-type=bed9+9', '-as=' + as_path,
                        hg38_bed, CHROM_SIZES['hg38'], hg38_bb], check=True)
        print(f'  hg38 bigBed: {hg38_bb}')

    if 'hg19' in args.db:
        hg19_bed = os.path.join(out_dir, 'cmpVCEPProvisionalClassHg19.bed')
        unmapped = hg19_bed + '.unmapped'
        subprocess.run(['liftOver', '-bedPlus=9', '-tab', hg38_bed, LIFTOVER_HG38_TO_HG19,
                        hg19_bed, unmapped], check=True)
        if os.path.getsize(unmapped) > 0:
            n = sum(1 for line in open(unmapped) if not line.startswith('#'))
            print(f'  WARNING: {n} unmapped in hg19 liftOver: {unmapped}', file=sys.stderr)
        hg19_bb = os.path.join(out_dir, 'cmpVCEPProvisionalClassHg19.bb')
        subprocess.run(['bedToBigBed', '-tab', '-type=bed9+9', '-as=' + as_path,
                        hg19_bed, CHROM_SIZES['hg19'], hg19_bb], check=True)
        print(f'  hg19 bigBed: {hg19_bb}')


if __name__ == '__main__':
    main()
