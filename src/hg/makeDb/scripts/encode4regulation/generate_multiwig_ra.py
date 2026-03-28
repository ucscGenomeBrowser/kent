#!/usr/bin/env python3
"""
Generate the multiWig portions of wgEncodeReg4.ra from the ENCODE V4 Regulation hub.txt.
Parses multiWig containers (RegMarkDNase, RegMarkATAC, RegMarkH3K4me3, RegMarkH3K27ac,
RegMarkCTCF, AveRNA) and their subtracks, renaming to wgEncodeReg4* convention.
"""

import re
import sys

HUB_FILE = "/hive/data/outside/encode4/ccre/ENCODE_V4_Regulation/hub.txt"
GBDB_BASE = "/gbdb/hg38/encode4/regulation/organAve"

# Map hub container names -> native names
CONTAINER_MAP = {
    "RegMarkDNase": "wgEncodeReg4Dnase",
    "RegMarkATAC": "wgEncodeReg4Atac",
    "RegMarkH3K4me3": "wgEncodeReg4MarkH3k4me3",
    "RegMarkH3K27ac": "wgEncodeReg4MarkH3k27ac",
    "RegMarkCTCF": "wgEncodeReg4MarkCtcf",
    "AveRNA": "wgEncodeReg4Txn",
}

# Hub container -> visibility override (H3K27ac full, rest hide)
CONTAINER_VIS = {
    "RegMarkDNase": "hide",
    "RegMarkATAC": "hide",
    "RegMarkH3K4me3": "hide",
    "RegMarkH3K27ac": "full",
    "RegMarkCTCF": "hide",
    "AveRNA": "hide",
}

# Priority mapping (matching plan)
CONTAINER_PRIORITY = {
    "RegMarkH3K27ac": "1.4",
    "RegMarkDNase": "1.1",
    "RegMarkATAC": "1.2",
    "RegMarkH3K4me3": "1.3",
    "RegMarkCTCF": "1.5",
    "AveRNA": "1.6",
}

# HTML page mapping
CONTAINER_HTML = {
    "RegMarkDNase": "wgEncodeReg4Dnase.html",
    "RegMarkATAC": "wgEncodeReg4Atac.html",
    "RegMarkH3K4me3": "wgEncodeReg4MarkH3k4me3.html",
    "RegMarkH3K27ac": "wgEncodeReg4MarkH3k27ac.html",
    "RegMarkCTCF": "wgEncodeReg4MarkCtcf.html",
    "AveRNA": "wgEncodeReg4Txn.html",
}


def parse_hub_stanzas(hub_file):
    """Parse hub.txt into a list of stanzas (list of key-value dicts)."""
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
            # Skip comment lines
            if line.strip().startswith('#'):
                continue
            # Key-value pair
            m = re.match(r'^(\S+)\s+(.*)', line)
            if m:
                key, val = m.group(1), m.group(2).strip()
                current[key] = val
    if current:
        stanzas.append(current)
    return stanzas


def rename_subtrack(hub_track_name, hub_container_name, native_container_name):
    """Rename a hub subtrack to native convention.

    Hub patterns:
      RegMarkDNaseblood -> wgEncodeReg4DnaseBlood
      AllRegMarkDNaseblood -> wgEncodeReg4DnaseAllBlood
      AveRNAblood+ -> wgEncodeReg4TxnBloodPlus
      AllAveRNAblood+ -> wgEncodeReg4TxnAllBloodPlus
    """
    name = hub_track_name

    # Handle "All" prefix
    is_all = False
    all_prefix = f"All{hub_container_name}"
    if name.startswith(all_prefix):
        is_all = True
        organ_part = name[len(all_prefix):]
    elif name.startswith(hub_container_name):
        organ_part = name[len(hub_container_name):]
    else:
        # Shouldn't happen
        return name

    # Handle strand suffix for AveRNA
    strand_suffix = ""
    if organ_part.endswith('+'):
        strand_suffix = "Plus"
        organ_part = organ_part[:-1]
    elif organ_part.endswith('-'):
        strand_suffix = "Minus"
        organ_part = organ_part[:-1]

    # Capitalize organ name parts (blood_vessel -> BloodVessel)
    organ_capitalized = ''.join(word.capitalize() for word in organ_part.split('_'))

    if is_all:
        return f"{native_container_name}All{organ_capitalized}{strand_suffix}"
    else:
        return f"{native_container_name}{organ_capitalized}{strand_suffix}"


def format_stanza(track_name, settings, indent="    "):
    """Format a track stanza as .ra text."""
    lines = [f"{indent}track {track_name}"]
    # Order: type, longLabel, shortLabel, then rest alphabetically, with specific ones last
    priority_keys = ['type', 'longLabel', 'shortLabel']
    last_keys = ['parent', 'bigDataUrl', 'visibility', 'priority', 'color', 'negateValues']

    # Print priority keys first
    for k in priority_keys:
        if k in settings:
            lines.append(f"{indent}{k} {settings[k]}")

    # Print middle keys alphabetically
    skip = set(priority_keys + last_keys + ['track'])
    for k in sorted(settings.keys()):
        if k not in skip:
            lines.append(f"{indent}{k} {settings[k]}")

    # Print last keys
    for k in last_keys:
        if k in settings:
            lines.append(f"{indent}{k} {settings[k]}")

    return '\n'.join(lines)


def main():
    stanzas = parse_hub_stanzas(HUB_FILE)

    # Index stanzas by track name
    by_name = {}
    for s in stanzas:
        if 'track' in s:
            by_name[s['track']] = s

    # Find containers and their subtracks
    containers_order = ["RegMarkH3K27ac", "RegMarkDNase", "RegMarkATAC",
                        "RegMarkH3K4me3", "RegMarkCTCF", "AveRNA"]

    output_lines = []

    for hub_container in containers_order:
        native_container = CONTAINER_MAP[hub_container]
        container_stanza = by_name.get(hub_container)
        if not container_stanza:
            print(f"WARNING: container {hub_container} not found", file=sys.stderr)
            continue

        # Write container stanza
        output_lines.append(f"    track {native_container}")
        output_lines.append(f"    container multiWig")
        output_lines.append(f"    noInherit on")
        output_lines.append(f"    type bigWig")
        output_lines.append(f"    parent wgEncodeReg4")
        output_lines.append(f"    shortLabel {container_stanza.get('shortLabel', '')}")
        output_lines.append(f"    longLabel {container_stanza.get('longLabel', '')}")
        output_lines.append(f"    visibility {CONTAINER_VIS[hub_container]}")
        if 'viewLimits' in container_stanza:
            output_lines.append(f"    viewLimits {container_stanza['viewLimits']}")
        output_lines.append(f"    autoScale on")
        output_lines.append(f"    maxHeightPixels {container_stanza.get('maxHeightPixels', '100:50:11')}")
        output_lines.append(f"    aggregate transparentOverlay")
        output_lines.append(f"    showSubtrackColorOnUi on")
        output_lines.append(f"    priority {CONTAINER_PRIORITY[hub_container]}")
        output_lines.append(f"    dragAndDrop subtracks")
        output_lines.append(f"    allButtonPair on")
        output_lines.append(f"    html {CONTAINER_HTML[hub_container]}")
        output_lines.append("")

        # Find all subtracks for this container
        subtracks = []
        for s in stanzas:
            if s.get('parent', '').startswith(hub_container):
                subtracks.append(s)

        # Separate regular and "All" subtracks
        regular = []
        all_variants = []
        for s in subtracks:
            track_name = s['track']
            if track_name.startswith(f"All{hub_container}") or track_name.startswith(f"AllAve"):
                all_variants.append(s)
            else:
                regular.append(s)

        # Write regular subtracks
        for s in regular:
            hub_name = s['track']
            native_name = rename_subtrack(hub_name, hub_container, native_container)

            # Determine parent on/off
            parent_val = s.get('parent', '')
            if parent_val.endswith(' off'):
                parent_line = f"{native_container} off"
            else:
                # no "off" suffix means ON
                parent_line = native_container

            output_lines.append(f"        track {native_name}")
            output_lines.append(f"        type bigWig")
            output_lines.append(f"        shortLabel {s.get('shortLabel', '')}")
            output_lines.append(f"        longLabel {s.get('longLabel', '')}")
            output_lines.append(f"        color {s.get('color', '0,0,0')}")
            output_lines.append(f"        parent {parent_line}")

            # bigDataUrl - convert to gbdb path
            bdu = s.get('bigDataUrl', '')
            output_lines.append(f"        bigDataUrl {GBDB_BASE}/{bdu}")

            if 'priority' in s:
                output_lines.append(f"        priority {s['priority']}")
            if s.get('negateValues'):
                output_lines.append(f"        negateValues {s['negateValues']}")
            output_lines.append("")

        # Write "All" variant subtracks
        if all_variants:
            for s in all_variants:
                hub_name = s['track']
                native_name = rename_subtrack(hub_name, hub_container, native_container)

                parent_val = s.get('parent', '')
                if parent_val.endswith(' off'):
                    parent_line = f"{native_container} off"
                else:
                    parent_line = native_container

                output_lines.append(f"        track {native_name}")
                output_lines.append(f"        type bigWig")
                output_lines.append(f"        shortLabel {s.get('shortLabel', '')}")
                output_lines.append(f"        longLabel {s.get('longLabel', '')}")
                output_lines.append(f"        color {s.get('color', '0,0,0')}")
                output_lines.append(f"        parent {parent_line}")

                bdu = s.get('bigDataUrl', '')
                output_lines.append(f"        bigDataUrl {GBDB_BASE}/{bdu}")

                if 'priority' in s:
                    output_lines.append(f"        priority {s['priority']}")
                if s.get('negateValues'):
                    output_lines.append(f"        negateValues {s['negateValues']}")
                output_lines.append("")

    # Print everything
    print('\n'.join(output_lines))


if __name__ == '__main__':
    main()
