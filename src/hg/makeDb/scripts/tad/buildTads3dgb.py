#!/usr/bin/env python3
# Build the tads3dgb faceted bigBed composite: all 464 human hg38 3D Genome Browser
# TAD-domain datasets, one subtrack each, faceted by metadata, organ-colored.
# Literal data: only a format normalization (10-col 3DGB bed -> bed4). No re-calling,
# merging, lifting, or recurrence scoring. refs #21599
#
# Reads:  3DGB source beds + datasets_api.json + classification.tsv (<tadDir>/3dgenome/source)
#         organ_colors.json (TAD-owned <tadDir> copy; has Colon/Bladder/Cervix added)
# Writes: build/hg38/tads3dgb/<id>.bb, tads3dgb_metadata.tsv, tads3dgb.ra

import json, csv, os, subprocess, sys, collections, re

# Assembly-aware: `python3 buildTads3dgb.py [hg38|mm10]` (default hg38). hg38 iterates the human
# classification.tsv (which carries the curated Condition/Treatment/Provenance facets); mm10 iterates
# the mouse TAD-called datasets straight from the API (no curated facets -> those 3 are omitted).
# mm39 is produced by lifting the mm10 bigBeds (see liftTads3dgbMouse.sh).
# Optional second argument tadDir (default /hive/data/outside/tad) is the root that inputs are read
# from and output is written under; pass a staging copy to rebuild without touching live files.
ASM    = sys.argv[1] if len(sys.argv) > 1 else "hg38"
TAD    = sys.argv[2] if len(sys.argv) > 2 else "/hive/data/outside/tad"
APIJS  = TAD + "/3dgenome/source/datasets_api.json"
CLASS  = TAD + "/3dgenome/source/classification.tsv"
ASFILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "tadDomain.as")
COLORS = TAD + "/organ_colors.json"   # TAD-owned (symlinked into /gbdb/<asm>/bbi/tad/)
ASCFG = {
 "hg38": dict(beds=TAD + "/3dgenome/source/tad_beds",
              chroms="/hive/data/genomes/hg38/chrom.sizes",
              out=TAD + "/3dgenome/build/hg38", species="human",
              default_on={"63","67","122","103"}),   # GM12878, H1-ESC, IMR-90, HMEC
 "mm10": dict(beds=TAD + "/3dgenome/mouse/tad_beds",
              chroms="/hive/data/genomes/mm10/chrom.sizes",
              out=TAD + "/3dgenome/build/mm10", species="mouse",
              default_on={"418","424","429"}),        # mESC Dixon 2012, CH12-LX Rao 2014, mESC WT Micro-C
}
C = ASCFG[ASM]
BEDS=C["beds"]; CHROMS=C["chroms"]; OUT=C["out"]; SPECIES=C["species"]; DEFAULT_ON=C["default_on"]
HAS_CTP = (ASM == "hg38")          # Condition/Treatment/Provenance facets (human curation only)
GBDB    = "/gbdb/%s/bbi/tad" % ASM
BBDIR  = os.path.join(OUT, "tads3dgb")
TSV    = os.path.join(OUT, "tads3dgb_metadata.tsv")
RA     = os.path.join(OUT, "tads3dgb.ra")

# 3DGB organ value -> organ_colors.json key (case/vocabulary normalization).
# The 5 case-only fixes map to existing keys; Colon/Bladder/Cervix were added to the JSON.
ORGAN_NORM = {
    "Blood Vessel":      "Blood vessel",
    "Lymphoid Tissue":   "Lymphoid tissue",
    "Small Intestine":   "Small intestine",
    "Adrenal Gland":     "Adrenal gland",
    "Connective Tissue": "Connective tissue",
}

def hex2rgb(h):
    h = h.lstrip("#")
    return "%d,%d,%d" % (int(h[0:2],16), int(h[2:4],16), int(h[4:6],16))

# 3DGB catalog text uses typographic hyphens/minus/spaces that break text search
# ("PANC-1" does not match "PANC\u20111"); fold them to ASCII. Greek letters are kept,
# with MICRO SIGN folded to GREEK MU so "uM" is spelled one way.
PUNCT = str.maketrans({"\u2010": "-", "\u2011": "-", "\u2012": "-", "\u2013": "-", "\u2212": "-",
                       "\u00a0": " ", "\u202f": " ", "\u2009": " ", "\u00b5": "\u03bc"})

def clean(s):
    """Collapse whitespace; never return empty (faceted JS throws on blank cells)."""
    s = "" if s is None else str(s).translate(PUNCT)
    s = re.sub(r"\s+", " ", s.replace("\t", " ")).strip()
    return s

STOP = {"the","of","a","an","in","on","to","and","for","with","at","by"}
def shortlab(name, maxlen=22):
    """A concise, sensical shortLabel: underscores->spaces, truncate only at whole-word
    boundaries (never mid-word), and drop a trailing connective word/punctuation."""
    s = re.sub(r"\s+", " ", name.replace("_", " ")).strip()
    if " of " in s.lower():        # "A of B" -> organ-first "B A" (avoids dangling-preposition cuts)
        i = s.lower().find(" of "); head, tail = s[:i].strip(), s[i+4:].strip()
        if head and tail:
            if head[:1].isupper() and head[1:2].islower():
                head = head[0].lower() + head[1:]
            s = tail + " " + head
    if len(s) > maxlen:
        out = ""
        for t in s.split(" "):
            if len((out + " " + t).strip()) <= maxlen:
                out = (out + " " + t).strip()
            else:
                break
        s = out if out else s[:maxlen]
    toks = s.split(" ")
    while len(toks) > 1 and toks[-1].lower().strip(",;-") in STOP:
        toks.pop()
    return " ".join(toks).rstrip(" ,;-")

def scap(s):
    """Sentence-case: capitalize the first letter only if the first word is all-lowercase
    (so acronyms/proper tokens like GM12878, Hi-C, RAD21 are preserved)."""
    if not s:
        return s
    first = s.split(" ", 1)[0]
    if first[:1].isalpha() and first == first.lower():
        return s[:1].upper() + s[1:]
    return s

# Hand-picked shortLabels where shortlab() truncation collides and the study suffix
# (below) does not separate them (QA, refs #21599). Keyed by 3DGB dataset id.
SHORT_OVERRIDE = {
    "48": "Keratinocyte day0", "49": "Keratinocyte day3", "50": "Keratinocyte day6",
    "690": "NPC neuron D868D 1", "691": "NPC neuron D868D 2", "692": "NPC neuron D868D 3",
    "693": "NPC neuron D868N 1", "694": "NPC neuron D868N 2", "695": "NPC neuron D868N 3",
    "96": "HCT-116 RAD21 auxin 6h", "97": "HCT-116 RAD21 no auxin",
}

def main():
    os.makedirs(BBDIR, exist_ok=True)

    api = {str(r["id"]): r for r in json.load(open(APIJS))}
    colors = json.load(open(COLORS))["Organ"]

    if ASM == "hg38":
        rows = []
        with open(CLASS) as fh:
            for r in csv.DictReader(fh, delimiter="\t"):
                rows.append({k: (v.strip() if v else "") for k, v in r.items()})
    else:
        # mm10: the iteration source is the set of mouse TAD-called datasets (one <id>.bed per
        # dataset in BEDS); all per-dataset metadata (year, refNo, organ, ...) comes from the API.
        have = sorted(int(f[:-4]) for f in os.listdir(BEDS) if f.endswith(".bed"))
        rows = [{"id": str(i), "bed": "%d.bed" % i} for i in have]

    meta = []          # (id, dict of tsv cols, color rgb, shortLabel, longLabel, mouseOver)
    missing_color = collections.Counter()
    fail = []

    for r in rows:
        did = r["id"]
        bed = os.path.join(BEDS, r["bed"])
        if not os.path.exists(bed):
            fail.append((did, "missing bed")); continue
        a = api.get(did, {})

        organ_raw = clean(a.get("organ")) or "Unknown"
        organ = ORGAN_NORM.get(organ_raw, organ_raw)
        if organ not in colors:
            missing_color[organ] += 1
        rgb = hex2rgb(colors.get(organ, "#000000"))

        name   = clean(a.get("name")) or ("dataset " + did)
        cell   = scap(clean(a.get("cellType"))) or "(unspecified)"
        assay  = scap(clean(a.get("dataType"))) or "Unknown"
        cond   = {"normal": "Normal", "cancer": "Cancer"}.get(r.get("normal_cancer",""), "Unknown")
        treat  = {"baseline": "Baseline", "pert": "Perturbation"}.get(r.get("baseline_pert",""), "Unknown")
        prov   = {"have": "Also in another UCSC track", "novel": "Novel to browser"}.get(r.get("have_novel",""), "Unknown")
        yr     = r.get("year") or a.get("year", "")     # classification (human) or API (mouse)
        try:
            year = str(int(float(yr))) if yr else "Unknown"
        except ValueError:
            year = "Unknown"
        study  = clean(r.get("refNo") or a.get("refNo")) or "Unknown"
        desc   = clean(a.get("description")) or name

        # display name: underscores -> spaces, sentence-cased (logic still keys on id/name)
        dname = scap(name.replace("_", " "))
        short = scap(shortlab(name))
        long  = "%s TAD domains (%s, %s, %s)" % (dname, organ, assay, study)
        mouse = "<b>3DGB TAD domain:</b> %s (%s, %s)" % (dname, organ, assay)

        # bed4: format-only. sort -k1,1 -k2,2n, then chrom/start/end + display name (-tab: name has spaces).
        bb = os.path.join(BBDIR, did + ".bb")
        tmp = os.path.join(BBDIR, did + ".bed4")
        with open(tmp, "w") as out:
            p1 = subprocess.run(["sort", "-k1,1", "-k2,2n", bed],
                                stdout=subprocess.PIPE, check=True, text=True)
            for line in p1.stdout.splitlines():
                f = line.split("\t")
                if len(f) < 3:
                    continue
                out.write("%s\t%s\t%s\t%s\n" % (f[0], f[1], f[2], dname))
        rc = subprocess.run(["bedToBigBed", "-type=bed4", "-tab", "-as=" + ASFILE, tmp, CHROMS, bb],
                            stderr=subprocess.PIPE, text=True)
        os.remove(tmp)
        if rc.returncode != 0:
            fail.append((did, rc.stderr.strip())); continue
        od = [("DatasetId", did), ("Organ", organ), ("Cell_type", cell), ("Assay", assay)]
        if HAS_CTP:   # human-only curated facets
            od += [("Condition", cond), ("Treatment", treat), ("Provenance", prov)]
        od += [("Year", year), ("Study", study), ("_Description", desc)]
        meta.append((did, collections.OrderedDict(od), rgb, short, long, mouse))

    if fail:
        print("FAILURES (%d):" % len(fail), file=sys.stderr)
        for d, e in fail[:20]:
            print("  ", d, e, file=sys.stderr)
    if missing_color:
        print("ORGANS WITHOUT COLOR KEY:", dict(missing_color), file=sys.stderr)
    print("converted %d / %d datasets" % (len(meta), len(rows)))

    # shortLabels: a cell line in several studies gets the study ID appended ("K-562 GSE63525");
    # SHORT_OVERRIDE wins. All shortLabels must end up unique.
    nshort = collections.Counter(x[3] for x in meta)
    meta = [(d, m, rgb, SHORT_OVERRIDE.get(d) or (short + " " + m["Study"] if nshort[short] > 1 else short),
             long, mouse) for d, m, rgb, short, long, mouse in meta]
    dup = [k for k, n in collections.Counter(x[3] for x in meta).items() if n > 1]
    if dup:
        sys.exit("duplicate shortLabels: %s" % dup)

    # metadata TSV (tab-sep; primaryKey + facet cols + hidden _Description)
    cols = list(meta[0][1].keys())
    with open(TSV, "w") as fh:
        fh.write("\t".join(cols) + "\n")
        for _, m, *_ in meta:
            fh.write("\t".join(m[c] for c in cols) + "\n")

    # faceted composite stanza
    with open(RA, "w") as fh:
        fh.write(
"""track tads3dgb
parent tads
priority 3
compositeTrack faceted
shortLabel 3D Genome Browser
longLabel TAD domains across %d %s Hi-C/Micro-C datasets (3D Genome Browser 2.0)
type bigBed 4
group regulation
visibility hide
metaDataUrl %s/tads3dgb_metadata.tsv
primaryKey DatasetId
colorSettingsUrl %s/organ_colors.json
maxCheckboxes 50
html tads3dgb

""" % (len(meta), SPECIES, GBDB, GBDB))
        for did, m, rgb, short, long, mouse in meta:
            onoff = "on" if did in DEFAULT_ON else "off"
            fh.write(
"""    track tads3dgb_%s
    parent tads3dgb %s
    shortLabel %s
    longLabel %s
    type bigBed 4
    bigDataUrl %s/tads3dgb/%s.bb
    color %s
    visibility pack
    mouseOver %s

""" % (did, onoff, short, long, GBDB, did, rgb, mouse))

    print("wrote %d subtracks -> %s" % (len(meta), RA))
    print("metadata -> %s" % TSV)

if __name__ == "__main__":
    main()
