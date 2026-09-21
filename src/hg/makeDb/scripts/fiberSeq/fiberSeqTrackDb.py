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

# Nucleosome density is mirrored for all 41 samples but is not shown.  The lab
# asked us to hold it back on 2026-09-14, while they settle internally on how
# they want nucleosomes displayed, and to keep the track set on FIRE peaks and
# CpG methylation for now.  The files stay in place and the download and check
# scripts still fetch and verify them, so turning this back on is this one flag
# plus a regenerate; nothing has to be downloaded again.
INCLUDE_NUC = False

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

# Shown behind an info icon on the Sample class column heading.
SAMPLE_CLASS_DESCRIPTION = (
    "HPRC = Lymphoblastoid (B-lymphocyte, EBV) cell lines from the NHGRI "
    "Human Pangenome Reference Consortium")

# Sample class swatches, shown next to that facet's checkboxes.
# Okabe-Ito colors for the swatches.
SAMPLE_CLASS_COLORS = {
    "HPRC": "#0072B2",
    "Common Cell Line": "#D55E00",
}


def readSamples(path):
    """Read fiberSeqSamples.tsv into a list of dicts, in file order.

    sampleClass comes from the lab's own sample sheet, not from the cell type:
    five of the lymphoblastoid lines are common cell lines rather than HPRC
    samples, so there is nothing in the cell type that tells the two apart."""
    samples = []
    with open(path) as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            fields = line.rstrip("\n").split("\t")
            if len(fields) < 5:
                sys.exit("bad sample line, want 5 fields: %s" % line.rstrip())
            acc, sample, cellType, _hash, sampleClass = fields[:5]
            samples.append({
                "accession": acc,
                "sample": sample,
                "cellType": cellType,
                "sampleClass": sampleClass,
            })
    if not samples:
        sys.exit("no samples read from %s" % path)
    return samples


def writeMetadata(path, samples):
    """The sample table shown on the track UI page.  A plain column name gets
    facet checkboxes, a leading underscore means searchable and sortable but not
    faceted.

    Accession is the primaryKey but sits last, since it is the least interesting
    thing about a sample.  Nothing requires the primaryKey to come first:
    facetedComposite.js only checks that the column exists, and every use of it
    is by name.  It is still the default sort, because accession order keeps the
    common cell lines together and then the HPRC samples together, which sample
    name in alphabetical order would scatter.

    Sample class is the one faceted column.  Its two values, HPRC and Common
    Cell Line, each cover many samples, which is what a facet needs:
    facetedComposite.js only offers a value that occurs more than once, since a
    checkbox matching a single row is just a slow search box.  The other three
    are underscored for that reason.  Sample and Accession are unique per row by
    definition, and 12 of the 14 cell types are a single sample, so as a facet
    cell type drew two checkboxes and left 12 samples unreachable.

    Names are underscore separated rather than camelCase: the header is rendered
    by toTitleStyle() in facetedComposite.js, which turns an underscore into a
    space but does not split camelCase, so "sampleClass" would have read
    "sampleClass" in the table.  A literal space cannot be used instead, because
    the saved sort order is a space separated list of column names and the
    submit code drops any name containing whitespace."""
    with open(path, "w") as f:
        # A header cell may carry a longer description after a "|", which the
        # track UI shows behind an info icon on the column heading.
        f.write("_Sample\t_Cell_type\tSample_class|%s\tAccession\n"
                % SAMPLE_CLASS_DESCRIPTION)
        for s in samples:
            f.write("%s\t%s\t%s\t%s\n" % (s["sample"], s["cellType"],
                                          s["sampleClass"], s["accession"]))


def writeColors(path):
    """Swatches beside a facet's checkboxes, keyed by column name.

    Only faceted columns get swatches, so the key has to match the column name
    exactly as the header writes it, underscore prefix included if it has one."""
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
    """One faceted composite over the per-sample data types.

    Accessibility and CpG methylation are read off the same molecules in the
    same experiment, so they belong in one table: the user picks a sample once,
    and cartDump.c assigns priority with the data element as the outer loop and
    the data type as the inner one, which keeps a sample's subtracks
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
        # Two literal wordings rather than one with a slot, to keep both under
        # the 80 characters a longLabel should stay within.
        ("longLabel Fiber-seq accessibility, peaks, nucleosomes and CpG "
         "methylation in %d samples" % len(samples)) if INCLUDE_NUC else
        ("longLabel Fiber-seq accessibility, FIRE peaks and CpG methylation "
         "in %d samples" % len(samples)),
        "metaDataUrl %s/fiberSeqCompendium_metadata.tsv" % dataUrlDir,
        "colorSettingsUrl %s/fiberSeqCompendium_colors.json" % dataUrlDir,
        "primaryKey Accession",
        # Declared order sets the order of the data type checkboxes, and of the
        # subtracks within each sample.  No data type name may contain an
        # underscore: hgTrackUi globs "<composite>_*_<dataType>_sel".
        'dataTypes acc|"Percent accessible" peaks|"FIRE peaks" '
        'hap|"Haplotype accessibility" '
        + ('nuc|"Nucleosome density" ' if INCLUDE_NUC else "")
        + 'cpg|"CpG methylation" '
        'cpgHap|"Haplotype CpG" cpgDiff|"CpG haplotype difference"',
        "defaultSortField Accession",
        "defaultGroupBy sample",
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
        # submits a selection, so this only sets the starting order.  A running
        # counter rather than a fixed index per data type, so the numbering stays
        # contiguous whether or not INCLUDE_NUC adds one in the middle.
        priN = [0]
        def pri():
            priN[0] += 1
            return "priority %d" % (i * 10 + priN[0])

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
            pri(),
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
            pri(),
        ])

        out += hapOverlay(gbdb, acc, name, "hap",
                          "hap1.percent.accessible.bw", "hap2.percent.accessible.bw",
                          "%s Hap1/2" % name,
                          "%s Fiber-seq percent accessible, haplotype 1 (blue) "
                          "and 2 (orange)" % name,
                          "%s Fiber-seq percent accessible, haplotype" % name,
                          "maximum", pri())

        # Held back, see INCLUDE_NUC.  Nucleosome density is read depth, not a
        # percentage, so unlike every other wiggle here it cannot have fixed
        # viewLimits: the per-sample mean runs from 25 (PS00971) to 142
        # (GM12878) purely with sequencing depth, and single loci spike into the
        # hundred thousands.  autoScale per window is the only setting that
        # shows all 41 samples usefully, and absolute values are not comparable
        # between samples anyway.
        if INCLUDE_NUC:
            out += stanza(8, [
                "track fiberSeqCompendium_%s_nuc" % acc,
                "parent fiberSeqCompendium off",
                "type bigWig",
                "bigDataUrl %s/%s/all.nucleosome.coverage.bw" % (gbdb, acc),
                "shortLabel %s Nuc" % name,
                "longLabel %s Fiber-seq nucleosome density, both haplotypes" % name,
                "color 0,158,115",
                "autoScale on",
                "alwaysZero on",
                "windowingFunction mean",
                "maxHeightPixels 100:40:8",
                "onlyVisibility full",
                pri(),
            ])

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
            pri(),
        ])

        out += hapOverlay(gbdb, acc, name, "cpgHap",
                          "cpg.hap1.bw", "cpg.hap2.bw",
                          "%s CpG Hap1/2" % name,
                          "%s CpG methylation, haplotype 1 (blue) and 2 (orange)" % name,
                          "%s CpG methylation, haplotype" % name,
                          "mean", pri())

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
            pri(),
        ])
        # Least significant first, so the more significant levels draw on top.
        # Not "i": that is the sample index pri() builds its priority from.
        #
        # The priority is what makes that happen and is not decoration.  This is
        # a solid overlay and all four files hold the same value at a shared
        # base, so the level that draws last is the colour the user sees.  Order
        # of declaration does not survive: makeContainerTrack() in
        # hg/hgTracks/container.c sorts the children with trackPriCmp, which
        # compares priority alone, and slSort is not a stable sort, so children
        # left on the parent's inherited priority draw in an arbitrary order.
        for level, (fname, label, color) in enumerate(DIFF_LEVELS):
            out += stanza(12, [
                "track fiberSeqCompendium_%s_cpgDiff_l%d" % (acc, level),
                "parent fiberSeqCompendium_%s_cpgDiff" % acc,
                "type bigWig",
                "bigDataUrl %s/%s/%s" % (gbdb, acc, fname),
                "color %s" % color,
                "shortLabel %s %s" % (name, label),
                "longLabel %s CpG haplotype difference, %s" % (name, label),
                "priority %d" % (level + 1),
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
    # Haplotype 1 then haplotype 2.  This overlay is transparent, so the order
    # barely shows, but a child without its own priority inherits the parent's
    # and then draws in whatever order slSort happens to leave it in - see the
    # longer note on the cpgDiff children, where the same thing is visible.
    for hap, fname, color in (("h1", file1, HAP1_COLOR), ("h2", file2, HAP2_COLOR)):
        n = hap[1]
        out += stanza(12, [
            "track fiberSeqCompendium_%s_%s_%s" % (acc, dataType, hap),
            "parent fiberSeqCompendium_%s_%s" % (acc, dataType),
            "type bigWig",
            "bigDataUrl %s/%s/%s" % (gbdb, acc, fname),
            "color %s" % color,
            "priority %s" % n,
            # Take the prefix from the container's own shortLabel, so the CpG
            # children come out "<sample> CpG Hap1" rather than colliding with
            # the accessibility children's "<sample> Hap1".
            "shortLabel %s%s" % (shortLabel.removesuffix("1/2"), n),
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

    # Per sample: acc, peaks, cpg (plus nuc when enabled), three container
    # stanzas (hap, cpgHap, cpgDiff) and their 2 + 2 + 4 children.
    nSub = len(DEFAULT_OVERLAY) + len(samples) * (3 + int(INCLUDE_NUC) + 3 + 8)
    print("wrote %s" % raPath)
    print("  %d samples, %d track stanzas" % (len(samples), nSub + 3))
    print("  metadata and colors in %s" % args.data_dir)


if __name__ == "__main__":
    main()
