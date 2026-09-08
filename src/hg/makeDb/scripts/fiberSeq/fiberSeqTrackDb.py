#!/usr/bin/env python3
"""Generate the hg38 Fiber-seq trackDb stanza plus the faceted-composite
metadata and color files.

Writes, given the sample list in fiberSeqSamples.tsv:

  <trackDbDir>/fiberSeq.ra                      the track stanzas
  <dataDir>/fiberSeqCompendium_metadata.tsv     facet table for accessibility
  <dataDir>/fiberSeqCompendium_colors.json      facet swatches
  <dataDir>/fiberSeqMeth_metadata.tsv           facet table for methylation
  <dataDir>/fiberSeqMeth_colors.json            facet swatches

Subtrack names are deliberately "<composite>_<accession>_<dataType>" with the
accession as the only middle component.  facetedCompositeUi() in
hg/hgTrackUi/hgTrackUi.c derives the data element by cutting at the first
underscore after the composite name, and cartDump.c rebuilds the subtrack name
as composite_element_type, so an accession holding an underscore (or a middle
part like "GM12878_PM00001") would make the browser look for tracks that do not
exist.  Sample name and cell type live in the metadata table instead.

Usage: fiberSeqTrackDb.py [--data-dir DIR] [--gbdb-dir DIR] [--trackdb-dir DIR]
"""

import argparse
import json
import os
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SAMPLE_LIST = os.path.join(SCRIPT_DIR, "fiberSeqSamples.tsv")

# The seven cell lines the lab wants in the always-on overlay, in their order,
# with the Okabe-Ito colors they chose in their own hub.
DEFAULT_OVERLAY = [
    ("PM00001", "230,159,0"),    # GM12878, orange
    ("PM00004", "86,180,233"),   # K562, sky blue
    ("PM00005", "0,158,115"),    # HepG2, bluish green
    ("PM00010", "240,228,66"),   # H1, yellow
    ("PM00008", "0,114,178"),    # Hap1, blue
    ("PM00012", "213,94,0"),     # Hek293T, vermillion
    ("PM00009", "204,121,167"),  # Jurkat, reddish purple
]

# Samples selected in the two faceted composites on a first visit.  A clean
# cross-product, so the facet table comes up as a tidy grid rather than the
# ragged per-sample mix the source hub had.
DEFAULT_SELECTED = ["PM00001", "PM00004", "PM00005"]

HAP1_COLOR = "0,114,178"    # Okabe-Ito blue
HAP2_COLOR = "213,94,0"     # Okabe-Ito vermillion

# CpG haplotype-difference thresholds: file suffix, label, color.  A sequential
# yellow-to-red ramp over nested significance cutoffs, monotonic in lightness.
DIFF_LEVELS = [
    ("cpg.diffs_all.bw", "All", "137,143,143"),
    ("cpg.diffs_p0.01.bw", "p < 0.01", "235,229,52"),
    ("cpg.diffs_p0.001.bw", "p < 0.001", "245,148,22"),
    ("cpg.diffs_p0.0001.bw", "p < 0.0001", "255,0,0"),
]

# Sample classes, derived from the lab's own free-text cell type.  27 of the 41
# samples are lymphoblastoid, so cell type alone gives one useless bucket; this
# splits them into three facet values.  Okabe-Ito colors for the swatches.
SAMPLE_CLASS_COLORS = {
    "Lymphoblastoid cell line": "#0072B2",
    "Stem cell": "#009E73",
    "Cancer or immortalized cell line": "#D55E00",
}


def sampleClass(cellType):
    """Group a free-text cell type into one of three facet values."""
    low = cellType.lower()
    if "lymphoblastoid" in low:
        return "Lymphoblastoid cell line"
    if "stem cell" in low or "ipsc" in low:
        return "Stem cell"
    return "Cancer or immortalized cell line"


def readSamples(path):
    """Read fiberSeqSamples.tsv into a list of dicts, in file order."""
    samples = []
    with open(path) as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            fields = line.rstrip("\n").split("\t")
            if len(fields) < 4:
                sys.exit("bad sample line, want 4 fields: %s" % line.rstrip())
            acc, sample, cellType, _hash = fields[:4]
            samples.append({
                "accession": acc,
                "sample": sample,
                "cellType": cellType,
                "sampleClass": sampleClass(cellType),
            })
    if not samples:
        sys.exit("no samples read from %s" % path)
    return samples


def writeMetadata(path, samples):
    """Facet table.  The first column is the primaryKey; plain column names get
    facet checkboxes, a leading underscore means searchable but not faceted."""
    with open(path, "w") as f:
        f.write("accession\tsampleClass\tcellType\t_sample\n")
        for s in samples:
            f.write("%s\t%s\t%s\t%s\n" % (s["accession"], s["sampleClass"],
                                          s["cellType"], s["sample"]))


def writeColors(path):
    with open(path, "w") as f:
        json.dump({"sampleClass": SAMPLE_CLASS_COLORS}, f, indent=4)
        f.write("\n")


def stanza(indent, lines):
    """Render one trackDb stanza at the given indent, with a trailing blank."""
    pad = " " * indent
    return "".join("%s%s\n" % (pad, l) for l in lines) + "\n"


def accOverlay(gbdb, samples):
    """The always-on overlay of the seven common cell lines."""
    byAcc = {s["accession"]: s for s in samples}
    out = stanza(4, [
        "track fiberSeqAcc",
        "parent fiberSeq",
        "container multiWig",
        "aggregate transparentOverlay",
        "showSubtrackColorOnUi on",
        "type bigWig 0 100",
        "viewLimits 0:100",
        "autoScale off",
        "alwaysZero on",
        "graphTypeDefault bar",
        "windowingFunction maximum",
        "maxHeightPixels 100:50:8",
        "visibility full",
        "priority 1",
        "shortLabel Fiber-seq Acc",
        "longLabel Fiber-seq percent-accessible chromatin in seven common cell lines",
    ])
    for acc, color in DEFAULT_OVERLAY:
        s = byAcc[acc]
        out += stanza(8, [
            "track fiberSeqAcc_%s" % acc,
            "parent fiberSeqAcc",
            "type bigWig",
            "bigDataUrl %s/%s/all.percent.accessible.bw" % (gbdb, acc),
            "color %s" % color,
            "shortLabel %s" % s["sample"],
            "longLabel %s Fiber-seq percent-accessible chromatin, both haplotypes"
            % s["sample"],
        ])
    return out


def compendium(gbdb, dataUrlDir, samples):
    """Faceted composite: percent accessible, FIRE peaks, haplotype overlay."""
    out = stanza(4, [
        "track fiberSeqCompendium",
        "parent fiberSeq",
        "compositeTrack faceted",
        "type bigWig",
        "shortLabel Fiber-seq Compendium",
        "longLabel Fiber-seq percent accessible, FIRE peaks and haplotype overlays "
        "in %d samples" % len(samples),
        "metaDataUrl %s/fiberSeqCompendium_metadata.tsv" % dataUrlDir,
        "colorSettingsUrl %s/fiberSeqCompendium_colors.json" % dataUrlDir,
        "primaryKey accession",
        'dataTypes acc|"Percent accessible" peaks|"FIRE peaks" '
        'hap|"Haplotype accessibility"',
        "defaultSortField accession",
        "maxCheckboxes 50",
        "noInherit on",
        "visibility hide",
        "priority 2",
    ])
    for s in samples:
        acc, name = s["accession"], s["sample"]
        accOn = "on" if acc in DEFAULT_SELECTED else "off"
        peaksOn = "on" if acc in DEFAULT_SELECTED else "off"

        out += stanza(8, [
            "track fiberSeqCompendium_%s_acc" % acc,
            "parent fiberSeqCompendium %s" % accOn,
            "type bigWig",
            "bigDataUrl %s/%s/all.percent.accessible.bw" % (gbdb, acc),
            "shortLabel %s Acc" % name,
            "longLabel %s Fiber-seq percent accessible, both haplotypes" % name,
            "color 0,0,0",
            "viewLimits 0:100",
            "autoScale off",
            "alwaysZero on",
            "graphTypeDefault bar",
            "windowingFunction maximum",
            "maxHeightPixels 100:40:8",
            "onlyVisibility full",
        ])

        # bigNarrowPeak, so the point-source offset in the tenth column is drawn
        # as a tick inside the peak (lfFromEncodePeak() sets tallStart/tallEnd
        # from it).  Its filters are the ENCODE peak settings, signalFilter and
        # qValueFilter, which encodePeakCfgUi() already knows how to draw; the
        # bigBed-generic filter.<field> settings are not read by this type.
        # Both defaults are the full range, so nothing is hidden until the user
        # narrows it.  signalValue tops out near 90 and the FIRE pipeline caps
        # qValue at 100.  pValue is -1 throughout, so no filter is offered.
        out += stanza(8, [
            "track fiberSeqCompendium_%s_peaks" % acc,
            "parent fiberSeqCompendium %s" % peaksOn,
            "type bigNarrowPeak",
            "bigDataUrl %s/%s/fire-peaks.ucsc.bb" % (gbdb, acc),
            "shortLabel %s Peaks" % name,
            "longLabel %s Fiber-seq FIRE peaks" % name,
            "signalFilter 0",
            "signalFilterLimits 0:100",
            "qValueFilter 0",
            "qValueFilterLimits 0:100",
            "scoreFilter 0",
            "scoreFilterLimits 0:1000",
            "mouseOver <b>%s FIRE peak</b><br>FIRE score: ${signalValue}"
            "<br>-log10 FDR: ${qValue}<br>Score: ${score}" % name,
            "onlyVisibility dense",
        ])

        out += stanza(8, [
            "track fiberSeqCompendium_%s_hap" % acc,
            "parent fiberSeqCompendium off",
            "container multiWig",
            "aggregate transparentOverlay",
            "showSubtrackColorOnUi on",
            "type bigWig 0 100",
            "viewLimits 0:100",
            "autoScale off",
            "alwaysZero on",
            "windowingFunction maximum",
            "maxHeightPixels 100:40:8",
            "shortLabel %s Hap1/2" % name,
            "longLabel %s Fiber-seq percent accessible, haplotype 1 (blue) and 2 (orange)"
            % name,
            "onlyVisibility full",
        ])
        for hap, color in (("h1", HAP1_COLOR), ("h2", HAP2_COLOR)):
            n = hap[1]
            out += stanza(12, [
                "track fiberSeqCompendium_%s_hap_%s" % (acc, hap),
                "parent fiberSeqCompendium_%s_hap" % acc,
                "type bigWig",
                "bigDataUrl %s/%s/hap%s.percent.accessible.bw" % (gbdb, acc, n),
                "color %s" % color,
                "shortLabel %s Hap%s" % (name, n),
                "longLabel %s Fiber-seq percent accessible, haplotype %s" % (name, n),
            ])
    return out


def methylation(gbdb, dataUrlDir, samples):
    """Faceted composite: CpG methylation, combined and per haplotype."""
    out = stanza(4, [
        "track fiberSeqMeth",
        "parent fiberSeq",
        "compositeTrack faceted",
        "type bigWig",
        "shortLabel Methylation",
        "longLabel CpG methylation from Fiber-seq reads, combined and by haplotype, "
        "in %d samples" % len(samples),
        "metaDataUrl %s/fiberSeqMeth_metadata.tsv" % dataUrlDir,
        "colorSettingsUrl %s/fiberSeqMeth_colors.json" % dataUrlDir,
        "primaryKey accession",
        'dataTypes comb|"Combined CpG" hap|"Hap1/Hap2 CpG" '
        'diffs|"Haplotype differences"',
        "defaultSortField accession",
        "maxCheckboxes 50",
        "noInherit on",
        "visibility hide",
        "priority 3",
    ])
    for s in samples:
        acc, name = s["accession"], s["sample"]
        combOn = "on" if acc in DEFAULT_SELECTED else "off"

        out += stanza(8, [
            "track fiberSeqMeth_%s_comb" % acc,
            "parent fiberSeqMeth %s" % combOn,
            "type bigWig 0 100",
            "bigDataUrl %s/%s/cpg.combined.bw" % (gbdb, acc),
            "shortLabel %s CpG" % name,
            "longLabel %s CpG methylation, both haplotypes" % name,
            "color 0,0,0",
            "viewLimits 0:100",
            "autoScale off",
            "windowingFunction mean",
            "maxHeightPixels 100:40:8",
            "onlyVisibility full",
        ])

        out += stanza(8, [
            "track fiberSeqMeth_%s_hap" % acc,
            "parent fiberSeqMeth off",
            "container multiWig",
            "aggregate transparentOverlay",
            "showSubtrackColorOnUi on",
            "type bigWig 0 100",
            "viewLimits 0:100",
            "autoScale off",
            "windowingFunction mean",
            "maxHeightPixels 100:40:8",
            "shortLabel %s CpG Hap1/2" % name,
            "longLabel %s CpG methylation, haplotype 1 (blue) and 2 (orange)" % name,
            "onlyVisibility full",
        ])
        for hap, color in (("h1", HAP1_COLOR), ("h2", HAP2_COLOR)):
            n = hap[1]
            out += stanza(12, [
                "track fiberSeqMeth_%s_hap_%s" % (acc, hap),
                "parent fiberSeqMeth_%s_hap" % acc,
                "type bigWig",
                "bigDataUrl %s/%s/cpg.hap%s.bw" % (gbdb, acc, n),
                "color %s" % color,
                "shortLabel %s CpG Hap%s" % (name, n),
                "longLabel %s CpG methylation, haplotype %s" % (name, n),
            ])

        out += stanza(8, [
            "track fiberSeqMeth_%s_diffs" % acc,
            "parent fiberSeqMeth off",
            "container multiWig",
            "aggregate solidOverlay",
            "showSubtrackColorOnUi on",
            "type bigWig -100 100",
            "viewLimits -100:100",
            "autoScale off",
            "windowingFunction mean",
            "maxHeightPixels 100:50:8",
            "shortLabel %s CpG diffs" % name,
            "longLabel %s CpG methylation difference between haplotypes, "
            "by significance threshold" % name,
            "onlyVisibility full",
        ])
        # Least significant first, so the more significant levels draw on top.
        for i, (fname, label, color) in enumerate(DIFF_LEVELS):
            out += stanza(12, [
                "track fiberSeqMeth_%s_diffs_l%d" % (acc, i),
                "parent fiberSeqMeth_%s_diffs" % acc,
                "type bigWig",
                "bigDataUrl %s/%s/%s" % (gbdb, acc, fname),
                "color %s" % color,
                "shortLabel %s %s" % (name, label),
                "longLabel %s CpG haplotype difference, %s" % (name, label),
            ])
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--data-dir", default="/hive/data/genomes/hg38/bed/fiberSeq",
                    help="where the metadata and color files are written")
    ap.add_argument("--gbdb-dir", default="/gbdb/hg38/fiberSeq",
                    help="bigDataUrl prefix, i.e. the symlink directory")
    ap.add_argument("--trackdb-dir",
                    default=os.path.expanduser(
                        "~/kent/src/hg/makeDb/trackDb/human/hg38"),
                    help="where fiberSeq.ra is written")
    args = ap.parse_args()

    samples = readSamples(SAMPLE_LIST)

    # The faceted composite fetches these two over http, from the same /gbdb
    # path the browser serves, so trackDb refers to them the same way.
    for name in ("fiberSeqCompendium", "fiberSeqMeth"):
        writeMetadata(os.path.join(args.data_dir, "%s_metadata.tsv" % name), samples)
        writeColors(os.path.join(args.data_dir, "%s_colors.json" % name))

    raPath = os.path.join(args.trackdb_dir, "fiberSeq.ra")
    with open(raPath, "w") as f:
        f.write("# Fiber-seq: chromatin accessibility, FIRE regulatory elements and CpG\n"
                "# methylation from PacBio HiFi Fiber-seq, Stergachis and Vollger labs.\n"
                "# Generated by hg/makeDb/scripts/fiberSeq/fiberSeqTrackDb.py.\n"
                "# Do not edit by hand, edit the script and regenerate.\n\n")
        f.write(stanza(0, [
            "track fiberSeq",
            "superTrack on show",
            "shortLabel Fiber-seq",
            "longLabel Fiber-seq chromatin accessibility, regulatory elements and CpG methylation",
            "group regulation",
            "priority 2.5",
        ]))
        f.write(accOverlay(args.gbdb_dir, samples))
        f.write(compendium(args.gbdb_dir, args.gbdb_dir, samples))
        f.write(methylation(args.gbdb_dir, args.gbdb_dir, samples))

    nSub = len(DEFAULT_OVERLAY) + len(samples) * (3 + 2) + len(samples) * (3 + 2 + 4)
    print("wrote %s" % raPath)
    print("  %d samples, %d track stanzas" % (len(samples), nSub + 4))
    print("  metadata and colors in %s" % args.data_dir)


if __name__ == "__main__":
    main()
