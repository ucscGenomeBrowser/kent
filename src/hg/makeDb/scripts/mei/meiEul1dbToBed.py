#!/usr/bin/env python3
"""Convert euL1db (hg19) tab-tables to a hg19 BED9+ for the meiEul1db track.

euL1db: the European database of L1-HS retrotransposon insertion polymorphisms
Mir AA, Philippe C, Cristofari G. NAR 2015, PMID 25352549.
"""

import argparse
import os
import sys
from collections import defaultdict
from html import escape

# Okabe-Ito lineage colors
COLOR_GERMLINE = "0,114,178"      # blue
COLOR_SOMATIC  = "213,94,0"       # vermillion
COLOR_MIXED    = "204,121,167"    # reddish purple
COLOR_UNKNOWN  = "153,153,153"    # neutral grey

# Max rows shown in the embedded HTML sample table (most MRIPs have <20 SRIPs;
# a few aggregate hundreds — truncate to keep row size bounded).
SAMPLE_TABLE_LIMIT = 200


def open_tab(path):
    """Yield split rows from a euL1db tab table, skipping ###-banner and column header."""
    with open(path) as fh:
        for line in fh:
            if not line.strip():
                continue
            if line.startswith("#"):
                continue
            yield line.rstrip("\n").split("\t")


def load_methods(path):
    """Methods.txt has no ID column; SRIP.method_id is the 1-based row number."""
    methods = {}
    with open(path) as fh:
        i = 0
        for line in fh:
            if not line.strip() or line.startswith("#"):
                continue
            i += 1
            cols = line.rstrip("\n").split("\t")
            methods[str(i)] = cols[0]
    return methods


def load_studies(path):
    studies = {}
    for cols in open_tab(path):
        sid = cols[0]
        pmid = cols[2] if len(cols) > 2 else "."
        studies[sid] = {"pmid": pmid}
    return studies


def load_individuals(path):
    inds = {}
    for cols in open_tab(path):
        name = cols[0]
        inds[name] = {
            "population": cols[3] if len(cols) > 3 else ".",
            "country":    cols[4] if len(cols) > 4 else ".",
            "clinical":   cols[5] if len(cols) > 5 else ".",
            "disease":    cols[6] if len(cols) > 6 else ".",
        }
    return inds


def load_samples(path):
    samples = {}
    for cols in open_tab(path):
        name = cols[0]
        samples[name] = {
            "individual": cols[1] if len(cols) > 1 else ".",
            "clinical":   cols[3] if len(cols) > 3 else ".",
            "tissue":     cols[5] if len(cols) > 5 else ".",
            "subtissue":  cols[6] if len(cols) > 6 else ".",
            "study":      cols[7] if len(cols) > 7 else ".",
        }
    return samples


def load_chrom_sizes(path):
    sizes = {}
    with open(path) as fh:
        for line in fh:
            chrom, size = line.rstrip().split("\t")[:2]
            sizes[chrom] = int(size)
    return sizes


def fix_chrom(chrom):
    """euL1db MRIP uses chr-prefixed names; chr23 = chrX (Helman2014 study).
    SRIP uses bare chromosome names — we prepend 'chr' before calling this."""
    if chrom == "chr23":
        return "chrX"
    if chrom == "chr24":
        return "chrY"
    return chrom


def aggregate_srips(srip_path, methods, samples, individuals):
    """Group SRIPs by their parent MRIP id, returning aggregated attributes."""
    agg = defaultdict(lambda: {
        "sripIds":      [],
        "sampleIds":    [],
        "individuals":  set(),
        "studies":      [],   # ordered list (set used for unique later)
        "subGroups":    set(),
        "integrity":    set(),
        "lineage":      set(),
        "methods":      set(),
        "tissues":      set(),
        "diseases":     set(),
        "populations":  set(),
        "pcrAny":       False,
        "rows":         [],   # for sample table
    })
    n_total = 0
    n_no_mrip = 0
    for cols in open_tab(srip_path):
        n_total += 1
        if len(cols) < 40:
            continue
        srip_id = cols[0]
        study   = cols[1]
        integrity = cols[2]
        sub_group = cols[13]
        lineage  = cols[20]
        method_id = cols[21]
        pcr5 = cols[22]
        pcr3 = cols[23]
        sample_id = cols[34]
        mrip_id  = cols[37]
        if not mrip_id or mrip_id == ".":
            n_no_mrip += 1
            continue
        a = agg[mrip_id]
        a["sripIds"].append(srip_id)
        if sample_id and sample_id != ".":
            a["sampleIds"].append(sample_id)
            samp = samples.get(sample_id, {})
            ind_id = samp.get("individual", ".")
            if ind_id and ind_id != ".":
                a["individuals"].add(ind_id)
            tissue = samp.get("tissue", ".")
            if tissue and tissue != ".":
                a["tissues"].add(tissue)
            ind = individuals.get(ind_id, {})
            pop = ind.get("population", ".")
            if pop and pop != "." and pop != "Unknown":
                a["populations"].add(pop)
            disease = ind.get("disease", ".")
            if disease and disease != "." and disease.lower() != "unknown":
                a["diseases"].add(disease)
            clinical = samp.get("clinical", ".")
            if clinical and clinical != "." and clinical not in ("Healthy", "Normal"):
                a["diseases"].add(clinical)
        if study and study != ".":
            a["studies"].append(study)
        if sub_group and sub_group != "." and sub_group != "unknown":
            a["subGroups"].add(sub_group)
        elif sub_group == "unknown":
            a["subGroups"].add("unknown")
        if integrity and integrity != ".":
            a["integrity"].add(integrity)
        if lineage and lineage != ".":
            a["lineage"].add(lineage)
        if method_id and method_id != "." and method_id in methods:
            a["methods"].add(methods[method_id])
        if (pcr5 not in (".", "no", "")) or (pcr3 not in (".", "no", "")):
            a["pcrAny"] = True
        a["rows"].append({
            "srip": srip_id,
            "sample": sample_id,
            "study": study,
            "lineage": lineage,
            "integrity": integrity,
            "subGroup": sub_group,
            "pcr": "yes" if ((pcr5 not in (".", "no", "")) or (pcr3 not in (".", "no", ""))) else "no",
        })
    print(f"SRIPs read: {n_total:,}; without MRIP id (skipped): {n_no_mrip}", file=sys.stderr)
    return agg


def lineage_color(lineages):
    if "somatic" in lineages and "germline" in lineages:
        return COLOR_MIXED, "germline,somatic"
    if "somatic" in lineages:
        return COLOR_SOMATIC, "somatic"
    if "germline" in lineages:
        return COLOR_GERMLINE, "germline"
    return COLOR_UNKNOWN, "unknown"


def build_sample_table(rows):
    """Build a small HTML table summarising contributing SRIPs."""
    n = len(rows)
    truncated = n > SAMPLE_TABLE_LIMIT
    shown = rows[:SAMPLE_TABLE_LIMIT]
    html = ['<table class="stdTbl"><tr>'
            '<th>SRIP</th><th>Sample</th><th>Study</th>'
            '<th>Lineage</th><th>Integrity</th><th>Sub-group</th><th>PCR</th></tr>']
    for r in shown:
        html.append("<tr>" + "".join(
            f"<td>{escape(r[k]) if r[k] and r[k] != '.' else ''}</td>"
            for k in ("srip", "sample", "study", "lineage", "integrity", "subGroup", "pcr")
        ) + "</tr>")
    html.append("</table>")
    if truncated:
        html.append(f"<p>Showing first {SAMPLE_TABLE_LIMIT} of {n} contributing samples. "
                    f"See euL1db for the full list.</p>")
    return "".join(html)


def comma_join(items):
    if not items:
        return ""
    return ", ".join(sorted(items))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", default="/hive/data/genomes/hg38/bed/mei/eul1db",
                    help="Directory containing euL1db .txt tables")
    ap.add_argument("--chrom-sizes", default="/hive/data/genomes/hg19/chrom.sizes")
    ap.add_argument("-o", "--out", required=True, help="Output BED9+ file")
    args = ap.parse_args()

    methods = load_methods(os.path.join(args.src, "Methods.txt"))
    studies = load_studies(os.path.join(args.src, "Study.txt"))
    individuals = load_individuals(os.path.join(args.src, "Individuals.txt"))
    samples = load_samples(os.path.join(args.src, "Samples.txt"))
    sizes = load_chrom_sizes(args.chrom_sizes)

    print(f"Loaded {len(methods)} methods, {len(studies)} studies, "
          f"{len(individuals)} individuals, {len(samples)} samples", file=sys.stderr)

    agg = aggregate_srips(os.path.join(args.src, "SRIP.txt"),
                          methods, samples, individuals)

    n_in = 0
    n_out = 0
    n_skip_chrom = 0
    n_skip_range = 0
    n_rename = 0
    dropped_chroms = defaultdict(int)
    with open(args.out, "w") as out:
        for cols in open_tab(os.path.join(args.src, "MRIP.txt")):
            n_in += 1
            if len(cols) < 12:
                continue
            mrip_id, chrom, start_s, stop_s, strand, midpoint, freq_s, \
                samples_field, studies_field, gene, in_ref, warning = cols[:12]
            orig = chrom
            chrom = fix_chrom(chrom)
            if chrom != orig:
                n_rename += 1
            if chrom not in sizes:
                dropped_chroms[chrom] += 1
                n_skip_chrom += 1
                continue
            # euL1db coordinates are 1-based inclusive [start, stop]; convert to BED
            try:
                start_1 = int(start_s)
                stop_1  = int(stop_s)
            except ValueError:
                continue
            bed_start = max(0, start_1 - 1)
            bed_end   = stop_1
            if bed_end <= bed_start:
                bed_end = bed_start + 1
            if bed_end > sizes[chrom]:
                n_skip_range += 1
                continue
            try:
                freq = float(freq_s) if freq_s not in (".", "") else 0.0
            except ValueError:
                freq = 0.0
            score = min(1000, int(round(freq * 1000)))
            if strand not in ("+", "-"):
                strand = "."
            a = agg.get(mrip_id, {
                "sripIds": [], "sampleIds": [], "individuals": set(),
                "studies": [], "subGroups": set(), "integrity": set(),
                "lineage": set(), "methods": set(), "tissues": set(),
                "diseases": set(), "populations": set(),
                "pcrAny": False, "rows": [],
            })
            rgb, lineage_str = lineage_color(a["lineage"])
            # Use MRIP-table study list if no SRIP-derived studies were available
            mrip_studies = [s for s in studies_field.split(";") if s and s != "."]
            study_set = sorted(set(a["studies"]) | set(mrip_studies))
            pmid_set = sorted({studies[s]["pmid"] for s in study_set
                               if s in studies and studies[s]["pmid"] not in (".", "")})
            out.write("\t".join([
                chrom,
                str(bed_start),
                str(bed_end),
                mrip_id,
                str(score),
                strand,
                str(bed_start),
                str(bed_end),
                rgb,
                f"{freq:.4f}",
                str(len(a["sripIds"])),
                str(len(set(a["sampleIds"]))),
                str(len(a["individuals"])),
                str(len(study_set)),
                comma_join(a["subGroups"]),
                comma_join(a["integrity"]),
                lineage_str,
                "yes" if a["pcrAny"] else "no",
                gene if gene != "." else "",
                "yes" if in_ref == "yes" else "no",
                warning if warning not in (".", "") else "",
                ", ".join(study_set),
                ", ".join(pmid_set),
                comma_join(a["methods"]),
                comma_join(a["tissues"]),
                comma_join(a["diseases"]),
                comma_join(a["populations"]),
                build_sample_table(a["rows"]) if a["rows"] else "",
            ]) + "\n")
            n_out += 1

    print(f"MRIPs read: {n_in:,}", file=sys.stderr)
    print(f"MRIPs written: {n_out:,}", file=sys.stderr)
    print(f"chr23/24 renamed to chrX/chrY: {n_rename}", file=sys.stderr)
    print(f"Skipped (chrom not in hg19 chrom.sizes): {n_skip_chrom}", file=sys.stderr)
    if dropped_chroms:
        for c, n in sorted(dropped_chroms.items()):
            print(f"    {c}: {n}", file=sys.stderr)
    print(f"Skipped (end beyond chrom): {n_skip_range}", file=sys.stderr)


if __name__ == "__main__":
    main()
