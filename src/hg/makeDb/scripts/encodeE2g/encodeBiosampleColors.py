#!/usr/bin/env python3
# Assign each ENCODE biosample a tissue group and color, reusing the GTEx tissue
# colors where a GTEx tissue matches, and record the ENCODE2 cell line color
# from cv.ra where one exists.  Meant to be reused by other ENCODE tracks: the
# input is any ENCODE API dump of objects with a biosample_ontology (Annotation,
# Experiment, Biosample), the output is a table keyed on biosample term name.
#
# Usage: encodeBiosampleColors.py annotations.json gtexTissueV8.tsv gtexPortalTissues.json cv.ra out.tsv
#   gtexTissueV8.tsv: hgsql hgFixed -Ne 'select name, description, color from gtexTissueV8'
#                     for the UCSC short names and the descriptions
#   gtexPortalTissues.json: curl 'https://gtexportal.org/api/v2/dataset/tissueSiteDetail?itemsPerPage=100'
#                     for the colors.  The colors in hgFixed.gtexTissueV8 are
#                     not GTEx's: seven tissues share a tan placeholder
#                     (#CDB79E), so Liver, Kidney, Spleen etc. would look alike.
#                     GTEx gives every tissue its own color, except that all
#                     brain regions are the same yellow.
#   cv.ra:            ~/kent/src/hg/makeDb/trackDb/cv/alpha/cv.ra
#
# Output columns:
#   biosample   ENCODE biosample_ontology.term_name
#   classification
#   group       GTEx tissue description, or one of the EXTRA_GROUPS below
#   gtexTissue  GTEx tissue short name, empty for an extra group
#   color       #rrggbb, the GTEx color of the group or its EXTRA_GROUPS color
#   match       which rule found the group: name (NAME table), cell (CELL,
#               ENCODE cell_slims), organ (ORGAN, ENCODE organ_slims), none (no
#               rule matched, group Other).  Any rule can lead to one of the
#               EXTRA_GROUPS, used where no GTEx tissue is similar.
#   cvColor     ENCODE2 cv.ra color of the cell line, r,g,b, or empty
#   organs      ENCODE organ_slims, for review
#
# The rules run in order and the first that matches wins.
import json, re, sys

# 1. explicit biosample name -> GTEx tissue
NAME = {
    "Ammon's horn": "brainHippocampus",
    "caudate nucleus": "brainCaudate",
    "head of caudate nucleus": "brainCaudate",
    "putamen": "brainPutamen",
    "globus pallidus": "brainPutamen",
    "cerebellum": "brainCerebellum",
    "cerebellar cortex": "brainCerebellum",
    "dorsolateral prefrontal cortex": "brainFrontCortex",
    "frontal cortex": "brainFrontCortex",
    "middle frontal gyrus": "brainFrontCortex",
    "posterior cingulate gyrus": "brainAnCinCortex",
    "midbrain": "brainSubstanNigra",
    "pons": "brainSubstanNigra",
    "medulla oblongata": "brainSubstanNigra",
    "spinal cord": "brainSpinalcord",
    "astrocyte of the spinal cord": "brainSpinalcord",
    "left cardiac atrium": "heartAtrialAppend",
    "right cardiac atrium": "heartAtrialAppend",
    "right atrium auricular region": "heartAtrialAppend",
    "aorta": "arteryAorta",
    "ascending aorta": "arteryAorta",
    "thoracic aorta": "arteryAorta",
    "fibroblast of the aortic adventitia": "arteryAorta",
    "coronary artery": "arteryCoronary",
    "tibial artery": "arteryTibial",
    "tibial nerve": "nerveTibial",
    "sciatic nerve": "nerveTibial",
    "sigmoid colon": "colonSigmoid",
    "esophagus muscularis mucosa": "esophagusMuscular",
    "esophagus squamous epithelium": "esophagusMucosa",
    "epithelial cell of esophagus": "esophagusMucosa",
    "lower leg skin": "skinExposed",
    "suprapubic skin": "skinNotExposed",
    "omental fat pad": "adiposeVisceral",
    "mesenteric fat pad": "adiposeVisceral",
    "adipocyte": "adiposeSubcut",
    "Peyer's patch": "smallIntestine",
    "mucosa of gallbladder": "liver",
    "hepatic stellate cell": "liver",
    "tongue": "minorSalivGland",
    "germinal center": "spleen",
    # in vitro differentiated cells and cell lines that have no or a
    # misleading organ_slim
    "bipolar neuron": "brainCortex",
    "glutamatergic neuron": "brainCortex",
    "neural progenitor cell": "brainCortex",
    "ecto neural progenitor cell": "brainCortex",
    "cardiovascular progenitor cell": "heartLeftVentricl",
    "endothelial cell": "arteryTibial",
    "urothelium cell line": "bladder",
    "Calu3": "lung",
    "non-pigmented ciliary epithelial cell": "EXTRA:eye",
}

# 2. ENCODE cell_slims -> GTEx tissue, checked in this order
CELL = [
    ("B cell", "xformedlymphocytes"),
    ("T cell", "wholeBlood"),
    ("NK cell", "wholeBlood"),
    ("hematopoietic cell", "wholeBlood"),
    ("leukocyte", "wholeBlood"),
    ("myeloid cell", "wholeBlood"),
    ("induced pluripotent stem cell", "EXTRA:stem"),
    # not "embryonic cell": ENCODE gives that slim to myoblasts too
    ("fibroblast", "xformedfibroblasts"),
    ("keratinocyte", "skinNotExposed"),
    ("melanocyte", "skinNotExposed"),
]

# 3. ENCODE organ_slims -> GTEx tissue, checked in this order, so the more
# specific organ wins over e.g. "epithelium" or "connective tissue"
ORGAN = [
    ("placenta", "EXTRA:placenta"),
    ("extraembryonic component", "EXTRA:placenta"),
    ("embryo", "EXTRA:stem"),
    ("eye", "EXTRA:eye"),
    ("spinal cord", "brainSpinalcord"),
    ("brain", "brainCortex"),
    ("heart", "heartLeftVentricl"),
    ("lung", "lung"),
    ("bronchus", "lung"),
    ("liver", "liver"),
    ("kidney", "kidneyCortex"),
    ("urinary bladder", "bladder"),
    ("ureter", "bladder"),
    ("colon", "colonTransverse"),
    ("large intestine", "colonTransverse"),
    ("small intestine", "smallIntestine"),
    ("stomach", "stomach"),
    ("esophagus", "esophagusMucosa"),
    ("pancreas", "pancreas"),
    ("adrenal gland", "adrenalGland"),
    ("thyroid gland", "thyroid"),
    ("spleen", "spleen"),
    ("thymus", "spleen"),
    ("lymph node", "spleen"),
    ("lymphoid tissue", "spleen"),
    ("bone marrow", "wholeBlood"),
    ("blood", "wholeBlood"),
    ("testis", "testis"),
    ("ovary", "ovary"),
    ("uterus", "uterus"),
    ("vagina", "vagina"),
    ("prostate gland", "prostate"),
    ("mammary gland", "breastMamTissue"),
    ("breast", "breastMamTissue"),
    ("skin of body", "skinNotExposed"),
    ("penis", "skinNotExposed"),
    ("adipose tissue", "adiposeSubcut"),
    ("musculature of body", "muscleSkeletal"),
    ("nerve", "nerveTibial"),
    ("arterial blood vessel", "arteryTibial"),
    ("blood vessel", "arteryTibial"),
    ("vasculature", "arteryTibial"),
    ("mouth", "minorSalivGland"),
    ("bone element", "EXTRA:bone"),
    ("connective tissue", "xformedfibroblasts"),
]

# groups for biosamples with no similar GTEx tissue.  Strong colors, none of
# them used by GTEx.
EXTRA_GROUPS = {
    "stem": ("Embryonic and stem cells", "#00A000"),
    "placenta": ("Placenta and extraembryonic", "#8000FF"),
    "eye": ("Eye", "#003366"),
    "bone": ("Bone", "#805000"),
    "other": ("Other", "#808080"),
}

# ENCODE biosample name -> cv.ra term, where they are spelled differently.
# Only 8 human cell lines have a color in cv.ra (GM12878 K562 HepG2 HepG2b
# HeLa-S3 HUVEC H1-hESC LNCaP), the other colored terms are mouse.
CV_ALIAS = {"H1": "H1-hESC", "LNCaP clone FGC": "LNCaP"}


def readCv(path):
    colors = {}
    term = None
    for line in open(path):
        if line.startswith("term "):
            term = line[5:].strip()
        elif line.startswith("color ") and term:
            colors[term] = line.split()[1]
    return colors


def norm(s):
    return re.sub(r"[^a-z0-9]", "", s.lower())


def assign(bo):
    " return (groupKey, match) for one biosample_ontology "
    name = bo["term_name"]
    if name in NAME:
        return NAME[name], "name"
    cells = bo.get("cell_slims", [])
    for slim, group in CELL:
        if slim in cells:
            return group, "cell"
    organs = bo.get("organ_slims", [])
    for slim, group in ORGAN:
        if slim in organs:
            return group, "organ"
    return "EXTRA:other", "none"


def main():
    annotFile, gtexFile, portalFile, cvFile, outFile = sys.argv[1:6]
    # GTEx portal id: the description with " - " and other punctuation as "_",
    # e.g. "Brain - Caudate (basal ganglia)" -> Brain_Caudate_basal_ganglia
    portal = {t["tissueSiteDetailId"]: "#" + t["colorHex"].upper().zfill(6)
              for t in json.load(open(portalFile))["data"]}
    gtex = {}
    for line in open(gtexFile):
        short, desc, _ = line.rstrip("\n").split("\t")
        portalId = re.sub(r" - |[^A-Za-z0-9-]+", "_", desc).strip("_")
        if portalId not in portal:
            sys.exit("GTEx tissue %s (%s) not in %s" % (short, portalId, portalFile))
        gtex[short] = (desc, portal[portalId])
    cv = readCv(cvFile)
    cvNorm = {norm(t): c for t, c in cv.items()}

    seen = {}
    for a in json.load(open(annotFile)):
        bo = a["biosample_ontology"]
        seen.setdefault(bo["term_name"], bo)

    with open(outFile, "w") as f:
        f.write("biosample\tclassification\tgroup\tgtexTissue\tcolor\tmatch\tcvColor\torgans\n")
        for name in sorted(seen, key=str.lower):
            bo = seen[name]
            key, match = assign(bo)
            if key.startswith("EXTRA:"):
                group, color = EXTRA_GROUPS[key[6:]]
                short = ""
            else:
                group, color = gtex[key]
                short = key
            cvName = CV_ALIAS.get(name, name)
            cvColor = cv.get(cvName) or cvNorm.get(norm(cvName), "") \
                if bo["classification"] == "cell line" else ""
            f.write("\t".join([name, bo["classification"], group, short, color, match,
                               cvColor, "|".join(bo.get("organ_slims", []))]) + "\n")


main()
