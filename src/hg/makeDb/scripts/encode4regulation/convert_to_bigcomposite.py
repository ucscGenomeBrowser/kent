#!/usr/bin/env python3
"""
Convert wgEncodeReg4 traditional composites to bigComposite (faceted) format.

Converts Epigenetics, RnaSeq, and TfChip from compositeTrack on to
compositeTrack faceted, generating metadata TSVs and new .ra files.

Uses S3 URLs from the pre-existing encode4_url_mapping.tsv.
The "Biosample" column is prefixed with underscore to hide it from facets.
"""

import os

TRACKDB_DIR = "/cluster/home/lrnassar/kent/src/hg/makeDb/trackDb/human/hg38"
GBDB_BASE = "/gbdb/hg38/encode4/regulation"
URL_MAPPING = "/hive/users/lrnassar/claude/RM34923/encode4_url_mapping.tsv"

EPI_VIEWS = {
    'wgEncodeReg4EpiDnaseView': 'DNase',
    'wgEncodeReg4EpiAtacView': 'ATAC',
    'wgEncodeReg4EpiH3k4me3View': 'H3K4me3',
    'wgEncodeReg4EpiH3k27acView': 'H3K27ac',
    'wgEncodeReg4EpiCtcfView': 'CTCF',
}

RNASEQ_VIEWS = {
    'wgEncodeReg4RnaSeqPlusView': 'Plus',
    'wgEncodeReg4RnaSeqMinusView': 'Minus',
}

# Default-on subtracks (5 per composite)
EPI_DEFAULT_ON = {
    'ENCFF414OGC',  # K562 DNase
    'ENCFF754EAC',  # K562 ATAC
    'ENCFF806YEZ',  # K562 H3K4me3
    'ENCFF849TDM',  # K562 H3K27ac
    'ENCFF736UDR',  # K562 CTCF
}
RNASEQ_DEFAULT_ON = {
    'ENCFF335LVS',  # K562 + strand
    'ENCFF257NOL',  # K562 - strand
    'ENCFF470BSF',  # GM12878 + strand
    'ENCFF830QII',  # GM12878 - strand
    'ENCFF908VIH',  # HepG2 + strand
}
TFCHIP_DEFAULT_ON = {
    'ENCFF400DFR',  # K562 CTCF
    'ENCFF836GHX',  # K562 POLR2A
    'ENCFF263IVY',  # K562 MYC
    'ENCFF110LJS',  # K562 MAX
    'ENCFF696URH',  # K562 EP300
}


def load_s3_map():
    """Load accession -> S3 URL mapping."""
    s3_map = {}
    with open(URL_MAPPING) as f:
        header = f.readline()
        for line in f:
            parts = line.rstrip('\n').split('\t')
            if len(parts) >= 3:
                s3_map[parts[0]] = parts[2]
    return s3_map


def parse_subgroups(subgroups_str):
    """Parse a subGroups line value into a dict."""
    result = {}
    for pair in subgroups_str.split():
        if '=' in pair:
            k, v = pair.split('=', 1)
            result[k] = v
    return result


def clean_label(val, capitalize=False):
    """Convert subGroup-safe value back to human-readable label."""
    result = val.replace('_', ' ')
    if capitalize and result:
        result = result[0].upper() + result[1:]
    return result


def parse_composite_ra(ra_file, has_views=False, view_map=None):
    """Parse a traditional composite .ra file and extract subtracks."""
    subtracks = []
    current_view = None

    with open(ra_file) as f:
        lines = f.readlines()

    i = 0
    while i < len(lines):
        line = lines[i].rstrip()
        stripped = line.strip()

        # Detect view containers
        if has_views and view_map and stripped.startswith('track '):
            track_name = stripped.split()[1]
            if track_name in view_map:
                current_view = view_map[track_name]
                i += 1
                continue

        # Detect subtrack stanzas
        if has_views:
            is_subtrack = line.startswith('        track ')
        else:
            is_subtrack = line.startswith('    track ') and not stripped.startswith('track wgEncodeReg4')

        if is_subtrack:
            st = {'trackName': stripped.split()[1]}
            if current_view:
                st['view'] = current_view
            i += 1
            while i < len(lines):
                sline = lines[i].strip()
                if not sline:
                    break
                parts = sline.split(None, 1)
                if len(parts) == 2:
                    st[parts[0]] = parts[1]
                elif len(parts) == 1:
                    st[parts[0]] = ''
                i += 1
            subtracks.append(st)
            continue

        i += 1

    return subtracks


def extract_accession(track_name, bigdata_url=''):
    """Extract ENCFF accession from track name or bigDataUrl.

    Falls back to bigDataUrl when the track name has a malformed accession
    (e.g. hub bug: ATAC_ENCFF128Muscle instead of ATAC_ENCFF128OID).
    """
    import re
    parts = track_name.split('_')
    for p in parts:
        # Valid ENCFF accessions are exactly ENCFF + 6 alphanumeric chars
        if re.match(r'^ENCFF[0-9A-Z]{6}$', p):
            return p
    # Fallback: extract from bigDataUrl
    if bigdata_url:
        m = re.search(r'(ENCFF[0-9A-Z]{6})', bigdata_url)
        if m:
            return m.group(1)
    return track_name


def convert_epigenetics(s3_map):
    """Convert Epigenetics composite to bigComposite."""
    ra_file = os.path.join(TRACKDB_DIR, "wgEncodeReg4Epigenetics.ra")
    out_ra = ra_file
    tsv_file = f"{GBDB_BASE}/wgEncodeReg4Epigenetics_metadata.tsv"

    print(f"Parsing {ra_file}...")
    subtracks = parse_composite_ra(ra_file, has_views=True, view_map=EPI_VIEWS)
    print(f"  Found {len(subtracks)} subtracks")

    rows = []
    missing_s3 = 0
    for st in subtracks:
        sg = parse_subgroups(st.get('subGroups', ''))
        acc = extract_accession(st['trackName'], st.get('bigDataUrl', ''))
        assay = st.get('view', '')

        s3_url = s3_map.get(acc, '')
        if not s3_url:
            missing_s3 += 1
            s3_url = st.get('bigDataUrl', '')

        rows.append({
            'accession': acc,
            'trackName': st['trackName'],
            'Assay': assay,
            'Organ': clean_label(sg.get('organ', 'unknown'), capitalize=True),
            'Biosample Type': clean_label(sg.get('biosampleType', 'unknown'), capitalize=True),
            'Life Stage': clean_label(sg.get('lifeStage', 'unknown'), capitalize=True),
            '_Biosample': clean_label(sg.get('simpleBiosample', 'unknown')),
            'bigDataUrl': s3_url,
            'shortLabel': st.get('shortLabel', ''),
            'longLabel': st.get('longLabel', ''),
            'color': st.get('color', ''),
            'type': st.get('type', 'bigWig'),
        })

    if missing_s3:
        print(f"  WARNING: {missing_s3} subtracks missing S3 URLs, using fallback")

    print(f"  Writing metadata TSV to {tsv_file}...")
    with open(tsv_file, 'w') as f:
        f.write("accession\tAssay\tOrgan\tBiosample Type\tLife Stage\t_Biosample\n")
        for r in sorted(rows, key=lambda x: (x['Assay'], x['Organ'], x['accession'])):
            f.write(f"{r['accession']}\t{r['Assay']}\t{r['Organ']}\t{r['Biosample Type']}\t{r['Life Stage']}\t{r['_Biosample']}\n")

    print(f"  Writing .ra to {out_ra}...")
    with open(out_ra, 'w') as f:
        f.write("track wgEncodeReg4Epigenetics\n")
        f.write("compositeTrack faceted\n")
        f.write("type bigWig\n")
        f.write("superTrack wgEncodeReg4 hide\n")
        f.write("shortLabel DNase/ATAC/Histone/CTCF (Indiv.)\n")
        f.write("longLabel Signal from individual DNase-seq, ATAC-seq, histone ChIP-seq, or CTCF ChIP-seq experiments from ENCODE 4\n")
        f.write("visibility hide\n")
        f.write("html wgEncodeReg4Epigenetics.html\n")
        f.write(f"metaDataUrl {tsv_file}\n")
        f.write("primaryKey accession\n")
        f.write("maxCheckboxes 40\n")
        f.write("noInherit on\n")
        f.write('pennantIcon New red ../goldenPath/newsarch.html#TBD "Released TBD"\n')
        f.write("\n")

        for r in sorted(rows, key=lambda x: (x['Assay'], x['Organ'], x['accession'])):
            acc = r['accession']
            default = "on" if acc in EPI_DEFAULT_ON else "off"
            f.write(f"    track wgEncodeReg4Epigenetics_{acc}\n")
            f.write(f"    type {r['type']}\n")
            f.write(f"    parent wgEncodeReg4Epigenetics {default}\n")
            f.write(f"    shortLabel {r['shortLabel']}\n")
            f.write(f"    longLabel {r['longLabel']}\n")
            f.write(f"    bigDataUrl {r['bigDataUrl']}\n")
            if r['color']:
                f.write(f"    color {r['color']}\n")
            f.write("    autoScale on\n")
            f.write("    maxHeightPixels 100:32:8\n")
            f.write("    visibility full\n")
            f.write("\n")

    print(f"  Done: {len(rows)} subtracks")
    return len(rows)


def convert_rnaseq(s3_map):
    """Convert RnaSeq composite to bigComposite."""
    ra_file = os.path.join(TRACKDB_DIR, "wgEncodeReg4RnaSeq.ra")
    out_ra = ra_file
    tsv_file = f"{GBDB_BASE}/wgEncodeReg4RnaSeq_metadata.tsv"

    print(f"Parsing {ra_file}...")
    subtracks = parse_composite_ra(ra_file, has_views=True, view_map=RNASEQ_VIEWS)
    print(f"  Found {len(subtracks)} subtracks")

    rows = []
    missing_s3 = 0
    for st in subtracks:
        sg = parse_subgroups(st.get('subGroups', ''))
        acc = st['trackName']
        strand = st.get('view', 'Unknown')

        s3_url = s3_map.get(acc, '')
        if not s3_url:
            missing_s3 += 1
            s3_url = st.get('bigDataUrl', '')

        rows.append({
            'accession': acc,
            'Organ': clean_label(sg.get('organ', 'unknown'), capitalize=True),
            'Biosample Type': clean_label(sg.get('biosampleType', 'unknown'), capitalize=True),
            'Life Stage': clean_label(sg.get('lifeStage', 'unknown'), capitalize=True),
            'Strand': strand,
            '_Biosample': clean_label(sg.get('simpleBiosample', 'unknown')),
            'bigDataUrl': s3_url,
            'shortLabel': st.get('shortLabel', ''),
            'longLabel': st.get('longLabel', ''),
            'color': st.get('color', ''),
            'type': st.get('type', 'bigWig'),
            'negateValues': st.get('negateValues', ''),
        })

    if missing_s3:
        print(f"  WARNING: {missing_s3} subtracks missing S3 URLs, using fallback")

    print(f"  Writing metadata TSV to {tsv_file}...")
    with open(tsv_file, 'w') as f:
        f.write("accession\tOrgan\tBiosample Type\tLife Stage\tStrand\t_Biosample\n")
        for r in sorted(rows, key=lambda x: (x['Organ'], x['accession'])):
            f.write(f"{r['accession']}\t{r['Organ']}\t{r['Biosample Type']}\t{r['Life Stage']}\t{r['Strand']}\t{r['_Biosample']}\n")

    print(f"  Writing .ra to {out_ra}...")
    with open(out_ra, 'w') as f:
        f.write("track wgEncodeReg4RnaSeq\n")
        f.write("compositeTrack faceted\n")
        f.write("type bigWig\n")
        f.write("superTrack wgEncodeReg4 hide\n")
        f.write("shortLabel RNA-seq (Indiv.)\n")
        f.write("longLabel Signal from individual total RNA-seq experiments from ENCODE 4\n")
        f.write("visibility hide\n")
        f.write("html wgEncodeReg4RnaSeq.html\n")
        f.write(f"metaDataUrl {tsv_file}\n")
        f.write("primaryKey accession\n")
        f.write("maxCheckboxes 40\n")
        f.write("noInherit on\n")
        f.write('pennantIcon New red ../goldenPath/newsarch.html#TBD "Released TBD"\n')
        f.write("\n")

        for r in sorted(rows, key=lambda x: (x['Organ'], x['accession'])):
            acc = r['accession']
            default = "on" if acc in RNASEQ_DEFAULT_ON else "off"
            f.write(f"    track wgEncodeReg4RnaSeq_{acc}\n")
            f.write(f"    type {r['type']}\n")
            f.write(f"    parent wgEncodeReg4RnaSeq {default}\n")
            f.write(f"    shortLabel {r['shortLabel']}\n")
            f.write(f"    longLabel {r['longLabel']}\n")
            f.write(f"    bigDataUrl {r['bigDataUrl']}\n")
            if r['color']:
                f.write(f"    color {r['color']}\n")
            f.write("    autoScale on\n")
            f.write("    maxHeightPixels 100:32:8\n")
            if r['negateValues']:
                f.write(f"    negateValues {r['negateValues']}\n")
            f.write("    visibility full\n")
            f.write("\n")

    print(f"  Done: {len(rows)} subtracks")
    return len(rows)


def convert_tfchip(s3_map):
    """Convert TfChip composite to bigComposite."""
    ra_file = os.path.join(TRACKDB_DIR, "wgEncodeReg4TfChip.ra")
    out_ra = ra_file
    tsv_file = f"{GBDB_BASE}/wgEncodeReg4TfChip_metadata.tsv"

    print(f"Parsing {ra_file}...")
    subtracks = parse_composite_ra(ra_file, has_views=False)
    print(f"  Found {len(subtracks)} subtracks")

    rows = []
    missing_s3 = 0
    for st in subtracks:
        sg = parse_subgroups(st.get('subGroups', ''))
        acc = st['trackName']

        s3_url = s3_map.get(acc, '')
        if not s3_url:
            missing_s3 += 1
            s3_url = st.get('bigDataUrl', '')

        rows.append({
            'accession': acc,
            'TF': sg.get('TF', 'unknown'),
            'Organ': clean_label(sg.get('organ', 'unknown'), capitalize=True),
            'Biosample Type': clean_label(sg.get('biosampleType', 'unknown'), capitalize=True),
            'Life Stage': clean_label(sg.get('lifeStage', 'unknown'), capitalize=True),
            '_Biosample': clean_label(sg.get('simpleBiosample', 'unknown')),
            'bigDataUrl': s3_url,
            'shortLabel': st.get('shortLabel', ''),
            'longLabel': st.get('longLabel', ''),
            'type': st.get('type', 'bigBed 5'),
        })

    if missing_s3:
        print(f"  WARNING: {missing_s3} subtracks missing S3 URLs, using fallback")

    print(f"  Writing metadata TSV to {tsv_file}...")
    with open(tsv_file, 'w') as f:
        f.write("accession\tTF\tOrgan\tBiosample Type\tLife Stage\t_Biosample\n")
        for r in sorted(rows, key=lambda x: (x['TF'], x['Organ'], x['accession'])):
            f.write(f"{r['accession']}\t{r['TF']}\t{r['Organ']}\t{r['Biosample Type']}\t{r['Life Stage']}\t{r['_Biosample']}\n")

    print(f"  Writing .ra to {out_ra}...")
    with open(out_ra, 'w') as f:
        f.write("track wgEncodeReg4TfChip\n")
        f.write("compositeTrack faceted\n")
        f.write("type bigBed 5\n")
        f.write("superTrack wgEncodeReg4 hide\n")
        f.write("shortLabel TF ChIP-seq (Indiv.)\n")
        f.write("longLabel Signal from individual transcription factor ChIP experiments from ENCODE 4\n")
        f.write("visibility hide\n")
        f.write("html wgEncodeReg4TfChip.html\n")
        f.write(f"metaDataUrl {tsv_file}\n")
        f.write("primaryKey accession\n")
        f.write("maxCheckboxes 40\n")
        f.write("noInherit on\n")
        f.write('pennantIcon New red ../goldenPath/newsarch.html#TBD "Released TBD"\n')
        f.write("\n")

        for r in sorted(rows, key=lambda x: (x['TF'], x['Organ'], x['accession'])):
            acc = r['accession']
            default = "on" if acc in TFCHIP_DEFAULT_ON else "off"
            f.write(f"    track wgEncodeReg4TfChip_{acc}\n")
            f.write(f"    type {r['type']}\n")
            f.write(f"    parent wgEncodeReg4TfChip {default}\n")
            f.write(f"    shortLabel {r['shortLabel']}\n")
            f.write(f"    longLabel {r['longLabel']}\n")
            f.write(f"    bigDataUrl {r['bigDataUrl']}\n")
            f.write("    useScore 1\n")
            f.write("    spectrum on\n")
            f.write("    visibility squish\n")
            f.write("\n")

    print(f"  Done: {len(rows)} subtracks")
    return len(rows)


def main():
    print("Loading S3 URL mapping...")
    s3_map = load_s3_map()
    print(f"  Loaded {len(s3_map)} accession -> S3 URL mappings")

    print("\nConverting ENCODE4 Regulation composites to bigComposite format\n")

    n_epi = convert_epigenetics(s3_map)
    print()
    n_rna = convert_rnaseq(s3_map)
    print()
    n_tf = convert_tfchip(s3_map)

    print(f"\n{'='*60}")
    print(f"Summary:")
    print(f"  Epigenetics: {n_epi} subtracks")
    print(f"  RNA-seq:     {n_rna} subtracks")
    print(f"  TF ChIP:     {n_tf} subtracks")
    print(f"  Total:       {n_epi + n_rna + n_tf} subtracks")
    print(f"\nMetadata TSVs written to {GBDB_BASE}/")
    print(f"\nNext steps:")
    print(f"  1. Remove 'include wgEncodeReg4RnaSeqBc.ra' from wgEncodeReg4.ra")
    print(f"  2. Rebuild trackDb: cd .../trackDb && make DBS=hg38")


if __name__ == "__main__":
    main()
