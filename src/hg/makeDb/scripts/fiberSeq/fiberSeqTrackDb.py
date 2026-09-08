#!/usr/bin/env python3
"""Generate the hg38 Fiber-seq trackDb stanza plus the faceted-composite
metadata and color files.

Writes, given the sample list in fiberSeqSamples.tsv:

  <trackDbDir>/fiberSeq.ra                      the track stanzas
  <dataDir>/fiberSeqCompendium_metadata.tsv     the faceted sample table
  <dataDir>/fiberSeqCompendium_colors.json      facet swatches

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

# Samples selected on a first visit.  A clean cross-product with the three
# default data types, so the facet table comes up as a tidy grid rather than the
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
# splits them into three facet values, and is the only faceted column left
# (written Sample_class in the metadata file, which the table shows as
# "Sample class").
# Okabe-Ito colors for the swatches.
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
    """Facet table.  The first column is the primaryKey; a plain column name gets
    facet checkboxes, a leading underscore means searchable and sortable but not
    faceted.  Names are underscore separated rather than camelCase: the header
    is rendered by toTitleStyle() in facetedComposite.js, which turns an
    underscore into a space but does not split camelCase, so "sampleClass" would
    have read "sampleClass" in the table.  A literal space cannot be used
    instead, because the saved sort order is a space separated list of column
    names and the submit code drops any name containing whitespace.

    Cell_type is deliberately underscored at the front.  facetedComposite.js only offers facet
    values that occur more than once, since a checkbox matching a single row is
    just a slow search box, and 12 of the 14 cell types here are a single sample
    each.  As a facet it drew exactly two checkboxes, Lymphoblastoid and
    Embryonic stem cell, leaving 12 samples unreachable by any cell-type filter.
    It is more useful as a searchable column.  The same rule is why the sample
    name cannot be a facet at all: all 41 values are distinct."""
    with open(path, "w") as f:
        f.write("Accession\tSample_class\t_Cell_type\t_Sample\n")
        for s in samples:
            f.write("%s\t%s\t%s\t%s\n" % (s["accession"], s["sampleClass"],
                                          s["cellType"], s["sample"]))


def writeColors(path):
    with open(path, "w") as f:
        json.dump({"Sample_class": SAMPLE_CLASS_COLORS}, f, indent=4)
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
    """One faceted composite over all six per-sample data types.

    Accessibility and CpG methylation are read off the same molecules in the
    same experiment, so they belong in one table: the user picks a sample once,
    and cartDump.c assigns priority with the data element as the outer loop and
    the data type as the inner one, which keeps a sample's six subtracks
    contiguous in the image.  Split across two composites they would draw as an
    accessibility block followed by a methylation block, and comparing the two
    for one sample would mean reading across every other sample.
    """
    out = stanza(4, [
        "track fiberSeqCompendium",
        "parent fiberSeq",
        "compositeTrack faceted",
        "type bigWig",
        "shortLabel Fiber-seq Compendium",
        "longLabel Fiber-seq accessibility, FIRE peaks and CpG methylation "
        "in %d samples" % len(samples),
        "metaDataUrl %s/fiberSeqCompendium_metadata.tsv" % dataUrlDir,
        "colorSettingsUrl %s/fiberSeqCompendium_colors.json" % dataUrlDir,
        "primaryKey Accession",
        # Declared order sets the order of the data type checkboxes, and of the
        # subtracks within each sample.  No data type name may contain an
        # underscore: hgTrackUi globs "<composite>_*_<dataType>_sel".
        'dataTypes acc|"Percent accessible" peaks|"FIRE peaks" '
        'hap|"Haplotype accessibility" cpg|"CpG methylation" '
        'cpgHap|"Haplotype CpG" cpgDiff|"CpG haplotype difference"',
        "defaultSortField Accession",
        "maxCheckboxes 50",
        "noInherit on",
        "visibility hide",
        "priority 2",
    ])

    for i, s in enumerate(samples):
        acc, name = s["accession"], s["sample"]
        # A clean cross-product on a first visit: three samples, and the three
        # data types that answer the question the merge is for, accessibility
        # next to methylation.
        on = "on" if acc in DEFAULT_SELECTED else "off"
        # Without an explicit priority the subtracks fall back to a label sort,
        # which on a first visit puts a sample's data types in an arbitrary
        # order (Peaks, CpG, Acc).  Sample order outer and declared data type
        # order inner matches the row of checkboxes across the top of the table.
        # cartDump.c replaces these with its own priorities as soon as the user
        # submits a selection, so this only sets the starting order.
        pri = lambda j: "priority %d" % (i * 10 + j + 1)

        out += stanza(8, [
            "track fiberSeqCompendium_%s_acc" % acc,
            "parent fiberSeqCompendium %s" % on,
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
            pri(0),
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
            "parent fiberSeqCompendium %s" % on,
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
            pri(1),
        ])

        out += hapOverlay(gbdb, acc, name, "hap",
                          "hap1.percent.accessible.bw", "hap2.percent.accessible.bw",
                          "%s Hap1/2" % name,
                          "%s Fiber-seq percent accessible, haplotype 1 (blue) "
                          "and 2 (orange)" % name,
                          "%s Fiber-seq percent accessible, haplotype" % name,
                          "maximum", pri(2))

        out += stanza(8, [
            "track fiberSeqCompendium_%s_cpg" % acc,
            "parent fiberSeqCompendium %s" % on,
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
            pri(3),
        ])

        out += hapOverlay(gbdb, acc, name, "cpgHap",
                          "cpg.hap1.bw", "cpg.hap2.bw",
                          "%s CpG Hap1/2" % name,
                          "%s CpG methylation, haplotype 1 (blue) and 2 (orange)" % name,
                          "%s CpG methylation, haplotype" % name,
                          "mean", pri(4))

        out += stanza(8, [
            "track fiberSeqCompendium_%s_cpgDiff" % acc,
            "parent fiberSeqCompendium off",
            "container multiWig",
            "aggregate solidOverlay",
            "showSubtrackColorOnUi on",
            "type bigWig -100 100",
            "viewLimits -100:100",
            "autoScale off",
            "windowingFunction mean",
            "maxHeightPixels 100:50:8",
            "shortLabel %s CpG diff" % name,
            "longLabel %s CpG methylation difference between haplotypes, "
            "by significance threshold" % name,
            "onlyVisibility full",
            pri(5),
        ])
        # Least significant first, so the more significant levels draw on top.
        for i, (fname, label, color) in enumerate(DIFF_LEVELS):
            out += stanza(12, [
                "track fiberSeqCompendium_%s_cpgDiff_l%d" % (acc, i),
                "parent fiberSeqCompendium_%s_cpgDiff" % acc,
                "type bigWig",
                "bigDataUrl %s/%s/%s" % (gbdb, acc, fname),
                "color %s" % color,
                "shortLabel %s %s" % (name, label),
                "longLabel %s CpG haplotype difference, %s" % (name, label),
            ])
    return out


def hapOverlay(gbdb, acc, name, dataType, file1, file2,
               shortLabel, longLabel, childLongLabel, windowing, priority):
    """A haplotype 1 / haplotype 2 transparent overlay, used for both the
    accessibility and the CpG haplotype data types."""
    out = stanza(8, [
        "track fiberSeqCompendium_%s_%s" % (acc, dataType),
        "parent fiberSeqCompendium off",
        "container multiWig",
        "aggregate transparentOverlay",
        "showSubtrackColorOnUi on",
        "type bigWig 0 100",
        "viewLimits 0:100",
        "autoScale off",
        "alwaysZero on",
        "windowingFunction %s" % windowing,
        "maxHeightPixels 100:40:8",
        "shortLabel %s" % shortLabel,
        "longLabel %s" % longLabel,
        "onlyVisibility full",
        priority,
    ])
    for hap, fname, color in (("h1", file1, HAP1_COLOR), ("h2", file2, HAP2_COLOR)):
        n = hap[1]
        out += stanza(12, [
            "track fiberSeqCompendium_%s_%s_%s" % (acc, dataType, hap),
            "parent fiberSeqCompendium_%s_%s" % (acc, dataType),
            "type bigWig",
            "bigDataUrl %s/%s/%s" % (gbdb, acc, fname),
            "color %s" % color,
            "shortLabel %s Hap%s" % (name, n),
            "longLabel %s %s" % (childLongLabel, n),
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

    # The faceted composite fetches these over http, from the same /gbdb path
    # the browser serves, so trackDb refers to them the same way.
    writeMetadata(os.path.join(args.data_dir, "fiberSeqCompendium_metadata.tsv"), samples)
    writeColors(os.path.join(args.data_dir, "fiberSeqCompendium_colors.json"))

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

    # Per sample: acc, peaks, cpg, three container stanzas (hap, cpgHap,
    # cpgDiff) and their 2 + 2 + 4 children.
    nSub = len(DEFAULT_OVERLAY) + len(samples) * (3 + 3 + 8)
    print("wrote %s" % raPath)
    print("  %d samples, %d track stanzas" % (len(samples), nSub + 3))
    print("  metadata and colors in %s" % args.data_dir)


if __name__ == "__main__":
    main()
