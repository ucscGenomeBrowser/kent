#!/usr/bin/env python3
"""
Generate the three large composite .ra files for wgEncodeReg4:
  - wgEncodeReg4Epigenetics.ra (3,199 subtracks, 5 views by assay type)
  - wgEncodeReg4RnaSeq.ra (1,046 subtracks, 2 views by strand)
  - wgEncodeReg4TfChip.ra (2,502 subtracks, flat)

Parses the ENCODE V4 Regulation hub.txt and transforms to native trackDb format.
"""

import re
import os
import sys

HUB_FILE = "/hive/data/outside/encode4/ccre/ENCODE_V4_Regulation/hub.txt"
OUTPUT_DIR = "/cluster/home/lrnassar/kent/src/hg/makeDb/trackDb/human/hg38"

GBDB_EPI = "/gbdb/hg38/encode4/regulation/epigenetics"
GBDB_TXN = "/gbdb/hg38/encode4/regulation/transcription"
GBDB_TF = "/gbdb/hg38/encode4/regulation/tf"

# Default-ON tracks: untreated K562, one per assay type (Epigenetics),
# K562+GM12878 plus/minus (RnaSeq), well-known TFs in K562 (TfChip)
EPI_DEFAULT_ON = {
    "DNase_ENCFF414OGC",    # K562 DNase
    "ATAC_ENCFF754EAC",     # K562 ATAC
    "H3K4me3_ENCFF806YEZ",  # K562 H3K4me3
    "H3K27ac_ENCFF849TDM",  # K562 H3K27ac
    "CTCF_ENCFF736UDR",     # K562 CTCF
}

RNASEQ_DEFAULT_ON = {
    "ENCFF335LVS",  # K562 + strand
    "ENCFF257NOL",  # K562 - strand
    "ENCFF470BSF",  # GM12878 + strand
    "ENCFF830QII",  # GM12878 - strand
    "ENCFF908VIH",  # HepG2 + strand
}

TFCHIP_DEFAULT_ON = {
    "ENCFF400DFR",  # K562 CTCF
    "ENCFF836GHX",  # K562 POLR2A
    "ENCFF263IVY",  # K562 MYC
    "ENCFF110LJS",  # K562 MAX
    "ENCFF696URH",  # K562 EP300
}


def clean_subgroup_value(val):
    """Clean special chars from a single subGroup value for native trackDb compatibility.
    subGroup tags only allow alphanumeric, underscore, and hyphen."""
    val = val.replace('μ', 'u')
    val = val.replace('β', 'beta')
    val = val.replace('α', 'alpha')
    val = val.replace(',', '-')
    val = val.replace('(', '').replace(')', '')
    val = val.replace("'", '')
    val = val.replace('.', '')
    val = val.replace('+', 'plus')
    val = val.replace('/', '_')
    val = val.replace(':', '_')
    val = val.replace(';', '_')
    val = val.replace('&', 'and')
    val = val.replace('@', 'at')
    val = val.replace('%', 'pct')
    # Spaces to underscores
    val = val.replace(' ', '_')
    # Remove any remaining non-ASCII or special chars — only allow [A-Za-z0-9_-=]
    val = ''.join(c for c in val if c.isascii() and (c.isalnum() or c in ('_', '-', '=')))
    return val


def clean_subgroups_string(sg_string):
    """Clean a subGroups assignment string like 'organ=Adipose biosampleType=tissue ...'
    Cleans each key=value pair individually, preserving space delimiters."""
    pairs = sg_string.split(' ')
    cleaned = []
    for pair in pairs:
        if '=' in pair:
            k, v = pair.split('=', 1)
            cleaned.append(f"{clean_subgroup_value(k)}={clean_subgroup_value(v)}")
        else:
            cleaned.append(clean_subgroup_value(pair))
    return ' '.join(cleaned)


def parse_hub_stanzas(hub_file):
    """Parse hub.txt into list of stanza dicts, preserving order."""
    stanzas = []
    current = {}
    with open(hub_file) as f:
        for line in f:
            line = line.rstrip('\n')
            if line.strip() == '':
                if current:
                    stanzas.append(current)
                    current = {}
                continue
            if line.strip().startswith('#'):
                continue
            m = re.match(r'^(\S+)\s+(.*)', line)
            if m:
                key, val = m.group(1), m.group(2).strip()
                current[key] = val
    if current:
        stanzas.append(current)
    return stanzas


def find_composite_and_subtracks(stanzas, composite_name):
    """Find a composite stanza and all its subtracks."""
    composite = None
    subtracks = []
    for s in stanzas:
        if s.get('track') == composite_name:
            composite = s
        elif s.get('parent', '').startswith(composite_name):
            subtracks.append(s)
    return composite, subtracks


def build_subgroup_def_from_subtracks(subtracks, sg_name, sg_label):
    """Build a subGroup definition line from actual subtrack values.
    This ensures the definition matches exactly what subtracks reference."""
    values = set()
    for s in subtracks:
        sg = s.get('subGroups', '')
        for pair in sg.split(' '):
            if pair.startswith(f"{sg_name}="):
                val = pair.split('=', 1)[1]
                cleaned = clean_subgroup_value(val)
                values.add(cleaned)
    sorted_vals = sorted(values)
    # Both key and display label must be clean
    pairs = ' '.join(f"{v}={v}" for v in sorted_vals)
    return pairs


def clean_display_label(val):
    """Clean a display label for subGroup definitions. Less aggressive than key cleaning
    but still removes characters that break trackDb parsing."""
    val = val.replace('μ', 'u')
    val = val.replace('β', 'beta')
    val = val.replace('α', 'alpha')
    val = val.replace(',', '-')
    val = val.replace("'", '')
    val = val.replace('.', '')
    val = val.replace('+', 'plus')
    val = val.replace('/', '_')
    val = val.replace(':', '_')
    val = val.replace(';', '_')
    val = val.replace('&', 'and')
    val = val.replace('@', 'at')
    val = val.replace('%', 'pct')
    val = val.replace(' ', '_')
    val = ''.join(c for c in val if c.isalnum() or c in ('_', '-', '='))
    return val


def generate_epigenetics():
    """Generate wgEncodeReg4Epigenetics.ra with 5 assay-type views."""
    print(f"Generating Epigenetics...", file=sys.stderr)
    stanzas = parse_hub_stanzas(HUB_FILE)
    composite, subtracks = find_composite_and_subtracks(stanzas, "Epigenetics")

    if not composite:
        print("ERROR: Epigenetics composite not found!", file=sys.stderr)
        return

    # View definitions
    views = {
        'DNase': {'track': 'wgEncodeReg4EpiDnaseView', 'label': 'DNase'},
        'ATAC': {'track': 'wgEncodeReg4EpiAtacView', 'label': 'ATAC'},
        'H3K4me3': {'track': 'wgEncodeReg4EpiH3k4me3View', 'label': 'H3K4me3'},
        'H3K27ac': {'track': 'wgEncodeReg4EpiH3k27acView', 'label': 'H3K27ac'},
        'CTCF': {'track': 'wgEncodeReg4EpiCtcfView', 'label': 'CTCF'},
    }

    lines = []

    # Composite header
    lines.append("track wgEncodeReg4Epigenetics")
    lines.append("compositeTrack on")
    lines.append("type bigWig")
    lines.append("superTrack wgEncodeReg4 hide")
    lines.append("shortLabel DNase/ATAC/Histone/CTCF (Indiv.)")
    lines.append("longLabel Signal from individual DNase-seq, ATAC-seq, histone ChIP-seq, or CTCF ChIP-seq experiments from ENCODE 4")
    lines.append("visibility hide")
    lines.append("html wgEncodeReg4Epigenetics.html")

    # Build subGroup definitions from actual subtracks to ensure consistency
    organ_vals = build_subgroup_def_from_subtracks(subtracks, 'organ', 'Organ/Tissue')
    biosample_type_vals = build_subgroup_def_from_subtracks(subtracks, 'biosampleType', 'Biosample_type')
    simple_biosample_vals = build_subgroup_def_from_subtracks(subtracks, 'simpleBiosample', 'Biosample')
    life_stage_vals = build_subgroup_def_from_subtracks(subtracks, 'lifeStage', 'Life_Stage')

    lines.append(f"subGroup1 organ Organ/Tissue {organ_vals}")
    lines.append(f"subGroup2 biosampleType Biosample_type {biosample_type_vals}")
    lines.append("subGroup3 view View wgEncodeReg4EpiDnaseView=DNase wgEncodeReg4EpiAtacView=ATAC wgEncodeReg4EpiH3k4me3View=H3K4me3 wgEncodeReg4EpiH3k27acView=H3K27ac wgEncodeReg4EpiCtcfView=CTCF")
    lines.append(f"subGroup4 simpleBiosample Biosample {simple_biosample_vals}")
    lines.append(f"subGroup5 lifeStage Life_Stage {life_stage_vals}")

    lines.append("dimensions dimX=biosampleType dimY=organ dimA=view dimB=lifeStage")
    lines.append("filterComposite dimA dimB")
    lines.append("sortOrder view=+ organ=+ biosampleType=+ simpleBiosample=+")
    lines.append("dragAndDrop subTracks")
    lines.append("priority 1.8")
    lines.append("")

    # Group subtracks by assay type first
    subtracks_by_assay = {a: [] for a in views}
    for s in subtracks:
        sg = s.get('subGroups', '')
        for a in views:
            if f"fileType={a}" in sg:
                subtracks_by_assay[a].append(s)
                break

    # Write view containers interleaved with their subtracks
    for assay, vinfo in views.items():
        lines.append(f"    track {vinfo['track']}")
        lines.append(f"    view {vinfo['track']}")
        lines.append(f"    parent wgEncodeReg4Epigenetics")
        lines.append(f"    shortLabel {vinfo['label']}")
        lines.append(f"    type bigWig")
        lines.append(f"    visibility dense")
        lines.append("")

        for s in subtracks_by_assay[assay]:
            track_name = s.get('track', '')
            bdu = s.get('bigDataUrl', '')
            sg = s.get('subGroups', '')
            view_track = vinfo['track']

            # Clean subGroups - replace fileType with view
            clean_sg = sg.replace(f"fileType={assay}", f"view={view_track}")
            clean_sg = clean_subgroups_string(clean_sg)

            default_state = "on" if track_name in EPI_DEFAULT_ON else "off"
            lines.append(f"        track {track_name}")
            lines.append(f"        type bigWig")
            if 'longLabel' in s:
                lines.append(f"        longLabel {s['longLabel']}")
            if 'shortLabel' in s:
                lines.append(f"        shortLabel {s['shortLabel']}")
            lines.append(f"        parent {view_track} {default_state}")
            lines.append(f"        bigDataUrl {GBDB_EPI}/{bdu}")
            lines.append(f"        subGroups {clean_sg}")
            if 'color' in s:
                lines.append(f"        color {s['color']}")
            lines.append(f"        autoScale on")
            lines.append(f"        maxHeightPixels 100:32:8")
            lines.append(f"        visibility full")
            lines.append("")
    outfile = os.path.join(OUTPUT_DIR, "wgEncodeReg4Epigenetics.ra")
    with open(outfile, 'w') as f:
        f.write('\n'.join(lines))
    print(f"  Wrote {outfile}: {len(subtracks)} subtracks", file=sys.stderr)


def generate_rnaseq():
    """Generate wgEncodeReg4RnaSeq.ra with 2 strand views."""
    print(f"Generating RnaSeq...", file=sys.stderr)
    stanzas = parse_hub_stanzas(HUB_FILE)
    composite, subtracks = find_composite_and_subtracks(stanzas, "Transcription")

    if not composite:
        print("ERROR: Transcription composite not found!", file=sys.stderr)
        return

    lines = []

    # Composite header
    lines.append("track wgEncodeReg4RnaSeq")
    lines.append("compositeTrack on")
    lines.append("type bigWig")
    lines.append("superTrack wgEncodeReg4 hide")
    lines.append("shortLabel RNA-seq (Indiv.)")
    lines.append("longLabel Signal from individual total RNA-seq experiments from ENCODE 4")
    lines.append("visibility hide")
    lines.append("html wgEncodeReg4RnaSeq.html")

    # Build subGroup definitions from actual subtracks
    organ_vals = build_subgroup_def_from_subtracks(subtracks, 'organ', 'Organ/Tissue')
    biosample_type_vals = build_subgroup_def_from_subtracks(subtracks, 'biosampleType', 'Biosample_type')
    simple_biosample_vals = build_subgroup_def_from_subtracks(subtracks, 'simpleBiosample', 'Biosample')
    life_stage_vals = build_subgroup_def_from_subtracks(subtracks, 'lifeStage', 'Life_Stage')

    lines.append(f"subGroup1 organ Organ/Tissue {organ_vals}")
    lines.append(f"subGroup2 biosampleType Biosample_type {biosample_type_vals}")
    lines.append("subGroup3 view View wgEncodeReg4RnaSeqPlusView=Plus_Strand wgEncodeReg4RnaSeqMinusView=Minus_Strand")
    lines.append(f"subGroup4 simpleBiosample Biosample {simple_biosample_vals}")
    lines.append(f"subGroup5 lifeStage Life_Stage {life_stage_vals}")

    lines.append("dimensions dimX=biosampleType dimY=organ dimA=view dimB=lifeStage")
    lines.append("filterComposite dimA dimB")
    lines.append("sortOrder view=+ organ=+ biosampleType=+ simpleBiosample=+")
    lines.append("dragAndDrop subTracks")
    lines.append("priority 1.9")
    lines.append("")

    # Group subtracks by strand
    strand_views = [
        ('Plus', 'wgEncodeReg4RnaSeqPlusView', 'Plus Strand'),
        ('Minus', 'wgEncodeReg4RnaSeqMinusView', 'Minus Strand'),
    ]

    subtracks_by_strand = {'Plus': [], 'Minus': []}
    for s in subtracks:
        sg = s.get('subGroups', '')
        is_minus = 'minus_strand_signal' in sg or 'minus_strand' in sg.lower()
        if is_minus:
            subtracks_by_strand['Minus'].append(s)
        else:
            subtracks_by_strand['Plus'].append(s)

    # Write view containers interleaved with their subtracks
    for strand, view_track, view_label in strand_views:
        lines.append(f"    track {view_track}")
        lines.append(f"    view {view_track}")
        lines.append(f"    parent wgEncodeReg4RnaSeq")
        lines.append(f"    shortLabel {view_label}")
        lines.append(f"    type bigWig")
        lines.append(f"    visibility dense")
        lines.append("")

        for s in subtracks_by_strand[strand]:
            track_name = s.get('track', '')
            bdu = s.get('bigDataUrl', '')
            sg = s.get('subGroups', '')
            is_minus = (strand == 'Minus')

            # Clean subGroups - replace fileType with view
            clean_sg = re.sub(r'fileType=\S+', f'view={view_track}', sg)
            clean_sg = clean_subgroups_string(clean_sg)

            default_state = "on" if track_name in RNASEQ_DEFAULT_ON else "off"
            lines.append(f"        track {track_name}")
            lines.append(f"        type bigWig")
            if 'longLabel' in s:
                lines.append(f"        longLabel {s['longLabel']}")
            if 'shortLabel' in s:
                lines.append(f"        shortLabel {s['shortLabel']}")
            lines.append(f"        parent {view_track} {default_state}")
            lines.append(f"        bigDataUrl {GBDB_TXN}/{bdu}")
            lines.append(f"        subGroups {clean_sg}")
            if 'color' in s:
                lines.append(f"        color {s['color']}")
            lines.append(f"        autoScale on")
            lines.append(f"        maxHeightPixels 100:32:8")
            lines.append(f"        visibility full")
            if s.get('negateValues') == 'on' or is_minus:
                lines.append(f"        negateValues on")
            lines.append("")

    outfile = os.path.join(OUTPUT_DIR, "wgEncodeReg4RnaSeq.ra")
    with open(outfile, 'w') as f:
        f.write('\n'.join(lines))
    print(f"  Wrote {outfile}: {len(subtracks)} subtracks", file=sys.stderr)


def generate_tfchip():
    """Generate wgEncodeReg4TfChip.ra (flat, no views)."""
    print(f"Generating TfChip...", file=sys.stderr)
    stanzas = parse_hub_stanzas(HUB_FILE)
    composite, subtracks = find_composite_and_subtracks(stanzas, "TF")

    if not composite:
        print("ERROR: TF composite not found!", file=sys.stderr)
        return

    lines = []

    # Composite header
    lines.append("track wgEncodeReg4TfChip")
    lines.append("compositeTrack on")
    lines.append("type bigBed 5")
    lines.append("superTrack wgEncodeReg4 hide")
    lines.append("shortLabel TF ChIP-seq (Indiv.)")
    lines.append("longLabel Signal from individual transcription factor ChIP experiments from ENCODE 4")
    lines.append("visibility hide")
    lines.append("html wgEncodeReg4TfChip.html")

    # Build subGroup definitions from actual subtracks
    tf_vals = build_subgroup_def_from_subtracks(subtracks, 'TF', 'TF')
    biosample_type_vals = build_subgroup_def_from_subtracks(subtracks, 'biosampleType', 'Biosample_type')
    life_stage_vals = build_subgroup_def_from_subtracks(subtracks, 'lifeStage', 'Life_Stage')
    simple_biosample_vals = build_subgroup_def_from_subtracks(subtracks, 'simpleBiosample', 'Biosample')
    organ_vals = build_subgroup_def_from_subtracks(subtracks, 'organ', 'Organ/Tissue')

    lines.append(f"subGroup1 TF TF {tf_vals}")
    lines.append(f"subGroup2 biosampleType Biosample_type {biosample_type_vals}")
    lines.append(f"subGroup3 lifeStage Life_Stage {life_stage_vals}")
    lines.append(f"subGroup4 simpleBiosample Biosample {simple_biosample_vals}")
    lines.append(f"subGroup5 organ Organ/Tissue {organ_vals}")

    lines.append("dimensions dimX=organ dimY=TF dimA=biosampleType dimB=lifeStage")
    lines.append("filterComposite dimA dimB")
    lines.append("sortOrder TF=+ organ=+ simpleBiosample=+ biosampleType=+")
    lines.append("dragAndDrop subTracks")
    lines.append("priority 2.0")
    lines.append("")

    # Subtracks (flat, no views)
    for s in subtracks:
        track_name = s.get('track', '')
        bdu = s.get('bigDataUrl', '')
        sg = s.get('subGroups', '')
        clean_sg = clean_subgroups_string(sg)

        default_state = "on" if track_name in TFCHIP_DEFAULT_ON else "off"
        lines.append(f"    track {track_name}")
        lines.append(f"    type bigBed 5")
        if 'longLabel' in s:
            lines.append(f"    longLabel {s['longLabel']}")
        if 'shortLabel' in s:
            lines.append(f"    shortLabel {s['shortLabel']}")
        lines.append(f"    parent wgEncodeReg4TfChip {default_state}")
        lines.append(f"    bigDataUrl {GBDB_TF}/{bdu}")
        lines.append(f"    subGroups {clean_sg}")
        if s.get('useScore'):
            lines.append(f"    useScore {s['useScore']}")
        if s.get('spectrum'):
            lines.append(f"    spectrum {s['spectrum']}")
        lines.append(f"    visibility squish")
        lines.append("")

    outfile = os.path.join(OUTPUT_DIR, "wgEncodeReg4TfChip.ra")
    with open(outfile, 'w') as f:
        f.write('\n'.join(lines))
    print(f"  Wrote {outfile}: {len(subtracks)} subtracks", file=sys.stderr)


if __name__ == '__main__':
    generate_epigenetics()
    generate_rnaseq()
    generate_tfchip()
    print("Done.", file=sys.stderr)
