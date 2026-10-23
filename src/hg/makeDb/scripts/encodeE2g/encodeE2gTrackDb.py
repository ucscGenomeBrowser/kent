#!/usr/bin/env python3
# Write the ENCODE-rE2G faceted composite from samples.tsv (made by
# encodeE2gManifest.py), modelled on fiberSeqTrackDb.py:
#   ~/kent/src/hg/makeDb/trackDb/human/hg38/encodeE2g.ra
#   <buildDir>/encodeE2g_metadata.tsv
#   <buildDir>/encodeE2g_colors.json
# Usage: encodeE2gTrackDb.py <buildDir> <raFile>
#
# Subtracks are encodeE2g_<annotation accession>_<dataType>, with the accession
# as the only middle component: facetedCompositeUi() cuts the data element at
# the first underscore after the composite name.  See the fiberSeq makeDoc.
import csv, json, sys

COMPOSITE = "encodeE2g"
GBDB = "/gbdb/hg38/encodeE2g"

# Default subtracks: eight common ENCODE cell lines, untreated.  Where a cell
# line has several untreated annotations, they differ only in the DNase-seq
# experiment used as input, and the one with the lowest input accession (the
# oldest ENCODE DNase) was taken.  The extended models of K562 and GM12878 are
# not in the track at all, see encodeE2gManifest.py.
DEFAULT_SELECTED = [
    "ENCSR171UQE",  # K562,    input ENCSR000EKP
    "ENCSR604WPK",  # GM12878, input ENCSR000EMT
    "ENCSR932TWZ",  # HepG2,   input ENCSR000EJV
    "ENCSR440PMA",  # H1,      input ENCSR000EJN
    "ENCSR575WDI",  # HCT116,  input ENCSR000ENM
    "ENCSR090HJT",  # A549,    input ENCSR000EIE
    "ENCSR936SUR",  # MCF-7,   input ENCSR000EKV
    "ENCSR376SFG",  # IMR-90,  input ENCSR000EKG
]

# Okabe-Ito colors for the facet swatches
CLASS_COLORS = {
    "tissue": "#E69F00",
    "primary cell": "#56B4E9",
    "cell line": "#009E73",
    "in vitro differentiated cells": "#CC79A7",
    "organoid": "#D55E00",
}
SEX_COLORS = {"female": "#D55E00", "male": "#0072B2", "mixed": "#CC79A7",
              "unknown": "#999999"}

# Metadata columns: (header, samples.tsv field).  A plain name is a facet, a
# leading underscore makes a searchable, sortable column that is not faceted.
# Only low-cardinality columns are facets: facetedComposite.js offers a value
# only when it occurs more than once.  Organ has about 60 values, so it is
# searchable rather than faceted.  Names use underscores, not camelCase,
# because toTitleStyle() only turns underscores into spaces.
COLUMNS = [
    ("_Biosample", "biosample"),
    ("Classification", "classification"),
    ("System", "system"),
    ("_Organ", "organ"),
    ("Treated", None),
    ("_Treatment", "treatment"),
    ("Sex", "sex"),
    ("Life_stage", "lifeStage"),
    ("_Age", "age"),
    ("_Donor", "donor"),
    ("_Input_experiment", "inputExp"),
    ("Accession", "accession"),
]


def readSamples(path):
    return list(csv.DictReader(open(path), delimiter="\t"))


def writeMetadata(path, samples):
    with open(path, "w") as f:
        f.write("\t".join(c for c, _ in COLUMNS) + "\n")
        for s in samples:
            row = []
            for col, field in COLUMNS:
                if col == "Treated":
                    row.append("treated" if s["treatment"] else "untreated")
                else:
                    row.append(s[field] or "none")
            f.write("\t".join(row) + "\n")


def writeColors(path):
    with open(path, "w") as f:
        json.dump({"Classification": CLASS_COLORS, "Sex": SEX_COLORS}, f, indent=4)
        f.write("\n")


def stanza(indent, lines):
    pad = " " * indent
    return "".join(pad + l + "\n" for l in lines) + "\n"


def shortName(s, room):
    " biosample name cut to fit the 17 character shortLabel "
    name = s["biosample"]
    return name if len(name) <= room else name[:room - 1] + "."


def longName(s):
    name = s["biosample"]
    if s["treatment"]:
        name += " treated with " + s["treatment"]
    return name


def compositeStanza():
    return stanza(0, [
        "track " + COMPOSITE,
        "compositeTrack faceted",
        "type bigBed",
        "shortLabel ENCODE rE2G",
        "longLabel ENCODE-rE2G enhancer-gene regulatory interaction predictions",
        "group regulation",
        "metaDataUrl %s/encodeE2g_metadata.tsv" % GBDB,
        "colorSettingsUrl %s/encodeE2g_colors.json" % GBDB,
        "primaryKey Accession",
        'dataTypes links|"Element-gene links" elements|"Elements"',
        "defaultSortField Accession",
        "maxCheckboxes 50",
        "noInherit on",
        "subtrackUrls Accession=https://www.encodeproject.org/annotations/$$/"
        " _Donor=https://www.encodeproject.org/human-donors/$$/"
        " _Input_experiment=https://www.encodeproject.org/experiments/$$/",
        "visibility hide",
        "dataVersion ENCODE4 release, downloaded October 2026",
        "html encodeE2g",
    ])


def subtracks(s, priority):
    acc = s["accession"]
    on = "on" if acc in DEFAULT_SELECTED else "off"
    links = stanza(4, [
        "track %s_%s_links" % (COMPOSITE, acc),
        "parent %s %s" % (COMPOSITE, on),
        "type bigInteract",
        "bigDataUrl %s/%s.interact.bb" % (GBDB, acc),
        "shortLabel %s Links" % shortName(s, 11),
        "longLabel %s ENCODE-rE2G element-gene links (%s)" % (longName(s), acc),
        "interactDirectional offsetTarget",
        "interactMultiRegion on",
        "maxHeightPixels 50:100:200",
        "url https://www.encodeproject.org/annotations/%s/" % acc,
        "urlLabel ENCODE annotation:",
        "visibility full",
        "priority %d" % priority,
    ])
    elements = stanza(4, [
        "track %s_%s_elements" % (COMPOSITE, acc),
        "parent %s off" % COMPOSITE,
        "type bigBed 9 +",
        "bigDataUrl %s/%s.elements.bb" % (GBDB, acc),
        "shortLabel %s Elems" % shortName(s, 11),
        "longLabel %s ENCODE-rE2G elements and target genes (%s)" % (longName(s), acc),
        "itemRgb on",
        "filter.e2gScore 0",
        "filterLimits.e2gScore 0:1",
        "filterLabel.e2gScore Minimum ENCODE-rE2G score",
        "filterValues.elementClass promoter,genic,intergenic",
        "filterLabel.elementClass Element class",
        "mouseOver <b>${targetGene}</b> (${elementClass} element)<br>"
        "ENCODE-rE2G score: ${e2gScore}<br>Distance to TSS: ${distanceToTss} bp",
        "url https://www.encodeproject.org/annotations/%s/" % acc,
        "urlLabel ENCODE annotation:",
        "visibility dense",
        "priority %d" % (priority + 1),
    ])
    return links + elements


def main():
    buildDir, raFile = sys.argv[1:3]
    samples = readSamples(buildDir + "/samples.tsv")
    accs = {s["accession"] for s in samples}
    missing = [a for a in DEFAULT_SELECTED if a not in accs]
    if missing:
        sys.exit("default annotations not in samples.tsv: %s" % missing)

    writeMetadata(buildDir + "/encodeE2g_metadata.tsv", samples)
    writeColors(buildDir + "/encodeE2g_colors.json")

    # defaults first, then the rest by accession
    order = sorted(samples, key=lambda s: (s["accession"] not in DEFAULT_SELECTED,
                                           DEFAULT_SELECTED.index(s["accession"])
                                           if s["accession"] in DEFAULT_SELECTED else 0,
                                           s["accession"]))
    with open(raFile, "w") as f:
        f.write("# ENCODE-rE2G enhancer-gene predictions, Engreitz lab, ENCODE4.\n"
                "# Generated by hg/makeDb/scripts/encodeE2g/encodeE2gTrackDb.py.\n"
                "# Do not edit by hand, edit the script and regenerate.\n\n")
        f.write(compositeStanza())
        for i, s in enumerate(order):
            f.write(subtracks(s, i * 10 + 1))
    print("%d samples, %d subtracks, %d on by default" % (
        len(samples), 2 * len(samples), len(DEFAULT_SELECTED)))


main()
