#!/usr/bin/env python3
"""
Assemble per-collection cell-type crosswalks for the mm10 (and later hg38) Cell
Browser datasets from the paper-curated decode tables (Redmine #37914).

Inputs (paper-sourced decode TSVs produced by the curation pass):
  xwalk_catlas-mouse-brain.tsv      code, canonical, broad_class, brain_region, note      (Li 2021)
  xwalk_catlas-mouse-aging.tsv      code, canonical, broad_class, tissue, age_months, note (Zhang 2022)
  xwalk_allen-basal-ganglia-atac.tsv code, canonical, broad_class, brain_region, note      (Allen taxonomy)
  xwalk_catlas-paired-tag.tsv       code, canonical, broad_class, brain_region, note        (Zhu 2021)
  xwalk_clean5.tsv                  raw_cell_type, canonical, broad_class, note             (the 5 clean datasets)

The decode TSVs (paper-curated, with a per-row `note` giving the source justification)
are archived alongside this script in celltype-crosswalks/paper-decodes/.

Output: one crosswalk file per collection in celltype-crosswalks/<collection>.tsv, in the
extended format read by build_stanzas.py:
  hubCellType <TAB> canonicalCellType <TAB> R,G,B <TAB> tissue <TAB> life_stage <TAB> condition
Empty tissue/life_stage/condition = keep the value build_stanzas computes.
Also writes celltype-palette.tsv (broad_class -> R,G,B) for the color legend.

NOTE: the cross-assembly celltype-class.tsv (celltype -> broad class -> color, the map
build_stanzas colors every track from) is NOT written here -- it is the union of these
mm10 crosswalk classes with the hg38 name-classification in paper-decodes/hg38_ct_class.tsv;
sea-ad-celltype-crosswalk.tsv (canonical harmonization for SEA-AD subclasses) is likewise
curated by hand. Both are archived in celltype-crosswalks/ as the data of record.
"""
import os, csv

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.environ.get("XWALK_SRC", os.path.join(HERE, "celltype-crosswalks", "paper-decodes"))
OUT = os.path.join(HERE, "celltype-crosswalks")
os.makedirs(OUT, exist_ok=True)

# ---- broad-class -> RGB palette (colorblind-conscious: Okabe-Ito core + Paul Tol) ----
PALETTE = {
    "Excitatory neuron":            "0,114,178",     # Okabe-Ito blue
    "Inhibitory neuron":            "213,94,0",      # vermillion
    "Medium spiny neuron":          "230,159,0",     # orange
    "Other neuron":                 "86,180,233",    # sky blue
    "Neural progenitor":            "51,34,136",     # Tol indigo
    "Astrocyte":                    "0,158,115",     # bluish green
    "Oligodendrocyte":              "204,121,167",   # reddish purple
    "Oligodendrocyte precursor":    "240,228,66",    # yellow
    "Microglia":                    "0,0,0",         # black
    # Coarse/unspecified glia, the glial counterpart to "Other neuron": datasets that
    # group all glia together (brainvar "Glia"), CNS glia with no specific class of their
    # own (olfactory ensheathing cells), and the fly glia. These were all landing in
    # "Other" grey alongside non-glial leftovers. Hue picked from the widest gap in the
    # palette; nearest neighbours are Epithelial (dE 17) and Astrocyte (dE 20), both
    # above the palette's own closest pair, and it stays separated under simulated
    # deuteranopia and protanopia.
    "Other glia":                   "88,160,88",     # medium green
    "Ependymal":                    "68,170,153",    # Tol teal
    "Choroid plexus":               "136,34,85",     # Tol wine
    "Endothelial":                  "136,204,238",   # Tol cyan
    "Mural":                        "221,204,119",   # Tol sand
    "Immune":                       "238,102,119",   # Tol red
    "Erythroid":                    "170,68,153",    # Tol purple
    "Hematopoietic stem/progenitor":"153,153,51",    # Tol olive
    "Muscle":                       "102,17,0",      # Tol dark red
    "Cardiomyocyte":                "170,68,0",      # burnt orange
    "Epithelial":                   "17,119,51",     # Tol green
    "Stromal":                      "153,79,0",      # brown
    "Other":                        "153,153,153",   # grey
    "Unknown":                      "187,187,187",   # light grey
}
# normalize raw broad_class labels from the decode tables into palette keys
NORM = {
    "OPC": "Oligodendrocyte precursor",
    "Other neural": "Other", "Other glia": "Other", "Aggregate": "Other",
    "Progenitor": "Neural progenitor",
}
# The decode tables use a bare "Progenitor" broad class. Almost every one of them is
# neural (radial glia, neuroblasts, intermediate progenitors), which is why NORM defaults
# it to "Neural progenitor" -- but a bare "Progenitor" is not by itself evidence of a
# neural lineage, and the default silently mis-assigned the one row that is not. Nephron
# progenitors are Six2+ kidney cap mesenchyme, so they belong with the mesenchymal cells.
# Keyed on the canonical cell type; anything not listed keeps the neural default.
NON_NEURAL_PROGENITOR = {
    "nephron progenitor": "Stromal",       # kidney cap mesenchyme, per clean5 kidney decode
}
def norm_class(bc, canonical):
    cl = canonical.lower()
    if bc == "Progenitor" and cl in NON_NEURAL_PROGENITOR:
        bc = NON_NEURAL_PROGENITOR[cl]
    else:
        bc = NORM.get(bc, bc)
    if "vlmc" in cl or "leptomening" in cl:       # VLMC -> Mural (agents used "Other")
        bc = "Mural"
    if canonical.startswith("OBDOP") or "olfactory bulb dopaminergic" in cl:
        bc = "Inhibitory neuron"                  # atlas places OB-dopaminergic in GABAergic
    return bc if bc in PALETTE else "Unknown"

def rgb(bc):
    return PALETTE.get(bc, PALETTE["Unknown"])

def read_tsv(name):
    with open(os.path.join(SRC, name)) as fh:
        return list(csv.DictReader(fh, delimiter="\t"))

# ---- aging: organ-level Tissue + Life_stage from the decoded tissue/age ----
AGING_TISSUE = {"Frontal cortex": "brain", "Dorsal hippocampus": "brain",
                "Bone marrow": "bone marrow", "Limb muscle": "muscle", "Heart": "heart"}
def aging_lifestage(age):
    return "Aged" if str(age).strip() == "18" else "Adult"   # 3,10 mo adult; 18 mo aged

rows_by_coll = {}

# catlas-mouse-aging: per-track tissue + life_stage + condition=Healthy
for r in read_tsv("xwalk_catlas-mouse-aging.tsv"):
    bc = norm_class(r["broad_class"], r["canonical_cell_type"])
    tissue = AGING_TISSUE.get(r["tissue"].strip(), "")
    rows_by_coll.setdefault("catlas-mouse-aging", []).append(
        [r["code"], r["canonical_cell_type"], rgb(bc), tissue,
         aging_lifestage(r["age_months"]), "Healthy"])

# three brain atlases: uniform tissue/life_stage/condition
BRAIN_DEFAULTS = {
    "catlas-mouse-brain":       ("brain", "Adult", "Healthy"),
    "allen-basal-ganglia-atac": ("brain", "Adult", "Healthy"),
    "catlas-paired-tag":        ("brain", "Adult", "Healthy"),
}
for coll, fn in [("catlas-mouse-brain", "xwalk_catlas-mouse-brain.tsv"),
                 ("allen-basal-ganglia-atac", "xwalk_allen-basal-ganglia-atac.tsv"),
                 ("catlas-paired-tag", "xwalk_catlas-paired-tag.tsv")]:
    tis, life, cond = BRAIN_DEFAULTS[coll]
    for r in read_tsv(fn):
        bc = norm_class(r["broad_class"], r["canonical_cell_type"])
        rows_by_coll.setdefault(coll, []).append(
            [r["code"], r["canonical_cell_type"], rgb(bc), tis, life, cond])

# clean-5: color + canonical only; keep computed tissue/life_stage/condition (blank)
clean5 = read_tsv("xwalk_clean5.tsv")
# clean5 raw names are global; emit one shared file applied to all 5 clean collections
clean5_rows = []
for r in clean5:
    bc = norm_class(r["broad_class"], r["canonical_cell_type"])
    clean5_rows.append([r["raw_cell_type"], r["canonical_cell_type"], rgb(bc), "", "", ""])

def write_xwalk(path, rows):
    with open(path, "w") as fh:
        for row in rows:
            fh.write("\t".join(row) + "\n")

for coll, rows in rows_by_coll.items():
    write_xwalk(os.path.join(OUT, coll + ".tsv"), rows)
    print("wrote %s: %d rows" % (coll, len(rows)))
write_xwalk(os.path.join(OUT, "clean5.tsv"), clean5_rows)
print("wrote clean5: %d rows" % len(clean5_rows))

# palette / legend file
with open(os.path.join(OUT, "celltype-palette.tsv"), "w") as fh:
    for bc, c in PALETTE.items():
        fh.write("%s\t%s\n" % (bc, c))
print("wrote celltype-palette.tsv: %d classes" % len(PALETTE))
