#!/usr/bin/env python3
"""
Turn the MethaDory episignature probe list into a bed9+ file, one row per CpG
probe, merging all episignatures that report that probe into comma-separated
lists.

Inputs
  - the episignature loci table (tsv, gzipped): IlmnID, Gene_region_exposure,
    deltaBeta, Study, pval, padj, Label
  - the signature metadata sheet (xlsx): StudyID, PMID, Signature, Disorder,
    signature, Notes
  - the probe -> hg38 coordinate table written by probeCoords.sh

Output
  - bed9+11, sorted, ready for bedToBigBed
  - a trackDb fragment with the filterValues lines, so the disorder, locus and
    study menus do not have to be maintained by hand
  - a tsv summary per study, used to build the table on the description page

Signatures with no row in the metadata sheet keep their signature name and get
an empty disorder; they are listed on stderr so they can be queried with the
authors.
"""

import argparse
import csv
import gzip
import json
import sys
from collections import defaultdict, OrderedDict

# Okabe-Ito, distinguishable under all three kinds of colour blindness. Two
# steps per direction make a diverging scale around zero, which is what a
# delta-beta is; the pairs are the same hue family at different lightness.
HYPER_STRONG = "213,94,0"    # vermillion
HYPER_WEAK = "230,159,0"     # orange
HYPO_WEAK = "86,180,233"     # sky blue
HYPO_STRONG = "0,114,178"    # blue
CONFLICT = "204,121,167"     # reddish purple

# A 10% methylation difference is the conventional effect-size line in array
# work, and it is also the modal reporting cut-off across these studies.
STRONG_DELTA = 0.10

# A site counts as conflicting only when a signature pointing the other way
# reaches this fraction of the strongest effect. Flagging any disagreement at
# all instead would colour 39% of the track, and that class is nearly a proxy
# for how many signatures share the probe: 0% of single-signature sites are
# mixed but 99.7% of sites with 11 or more signatures are. Requiring a
# comparable opposite effect leaves 10%, where the site really has no dominant
# direction.
CONFLICT_FRAC = 0.8

DIRECTIONS = ["Strong hypermethylation", "Weak hypermethylation",
              "Weak hypomethylation", "Strong hypomethylation", "Conflicting"]

# Plain typos in the metadata sheet's Disorder column, fixed here rather than
# in the sheet since they show up verbatim in the bigBed and the filter menu.
# This is not the same kind of call as the study/author conflicts flagged
# below: those are ambiguous provenance questions, these are just misspelled
# eponyms/words.
DISORDER_SPELLING_FIXES = {
    "Nicolaiders-Baraitser syndrome": "Nicolaides-Baraitser syndrome",
    "PURA-related disoder": "PURA-related disorder",
}


def readMeta(fname):
    """Read the xlsx metadata sheet, keyed on the 'signature' column, which is
    the same string as the Label column of the loci table with the space
    replaced by an underscore."""
    import openpyxl
    wb = openpyxl.load_workbook(fname, read_only=True)
    ws = wb.worksheets[0]
    rows = ws.iter_rows(values_only=True)
    header = [str(c).strip() for c in next(rows)]
    idx = {name: i for i, name in enumerate(header)}
    meta = {}
    for row in rows:
        vals = ["" if c is None else str(c).strip() for c in row]
        if not any(vals):
            continue
        # the 'signature' column joins to the Label column of the loci table.
        # The authors have spelled the join with an underscore in one release
        # and a space in the next, so normalise both sides the same way.
        key = vals[idx["signature"]].replace(" ", "_")
        # a disorder name can contain a comma, and comma is the separator both
        # of the bigBed list fields and of trackDb filterValues, so swap it out
        disorder = vals[idx["Disorder"]].replace(",", ";")
        disorder = DISORDER_SPELLING_FIXES.get(disorder, disorder)
        meta[key] = {
            "study": vals[idx["StudyID"]],
            "pmid": vals[idx["PMID"]],
            "disorder": disorder,
            "signature": vals[idx["Signature"]],
            "notes": vals[idx["Notes"]] if "Notes" in idx else "",
        }
    return meta


def normStudy(s):
    """Fold the spellings of a study name that the metadata sheet uses
    interchangeably: case, punctuation and the trailing year all vary
    (ArefEShghi2018 / ArefEshghi2018, Leitao / Leitao2025)."""
    s = "".join(c for c in s.lower() if c.isalnum())
    while s and s[-1].isdigit():
        s = s[:-1]
    return s


def crossedMetaRows(meta):
    """Find metadata rows whose StudyID names a different author from the study
    embedded in the 'signature' column. The signature column is the string that
    joins to the data, so where the two disagree beyond a spelling variant the
    row's PubMed ID and disorder may belong to the other study. Reported, not
    corrected: only the authors can say which of the two is right."""
    odd = []
    for key, m in sorted(meta.items()):
        if "_" not in key:
            continue
        suffix = key.rsplit("_", 1)[1]
        if normStudy(suffix) != normStudy(m["study"]):
            odd.append((key, m["study"], m["pmid"], m["signature"], m["disorder"]))
    return odd


def readCoords(fname):
    coords = {}
    with open(fname) as fh:
        for line in fh:
            probe, chrom, start, end, source = line.rstrip("\n").split("\t")
            coords[probe] = (chrom, int(start), int(end), source)
    return coords


def fmtDelta(x):
    return ("%.4f" % x).rstrip("0").rstrip(".")


JSON_HEADER = ["Episignature", "Disorder", "Delta-beta", "Adjusted p-value"]


def jsonTable(rows):
    """Encode the per-signature rows for the detailsDynamicTable trackDb setting.

    hgc validates this with jsonParse and then hands the raw string to hgc.js,
    whose makeGenericTable() walks Object.keys(): a key whose value is an array
    becomes one table row with one cell per element, and the key itself is not
    drawn. So an object of arrays gives a plain grid. Keys are 'r00', 'r01', ...
    rather than '0', '1', ... because JavaScript reorders integer-like keys
    numerically but keeps other string keys in insertion order.

    Only the ';|' encoding of detailsDynamicTable goes through hgc's fixed
    4096-byte buffer (and errAborts past it); this JSON path has no such limit,
    which matters here because one probe carries 41 signatures.
    """
    out = OrderedDict()
    for i, row in enumerate([JSON_HEADER] + rows):
        out["r%03d" % i] = row
    # ensure_ascii keeps the bigBed field plain ASCII; kent's jsonParse passes
    # \uXXXX through untouched and the browser renders it
    return json.dumps(out, ensure_ascii=True, separators=(",", ":"))


def fmtPval(s):
    """Three significant digits is plenty for display and keeps the file small."""
    try:
        return "%.3g" % float(s)
    except ValueError:
        return s


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("loci", help="episignature loci tsv.gz")
    ap.add_argument("metadata", help="signature metadata xlsx")
    ap.add_argument("coords", help="probe -> hg38 coordinate tsv from probeCoords.sh")
    ap.add_argument("outBed", help="output bed file")
    ap.add_argument("--raOut", help="write the trackDb filterValues fragment here")
    ap.add_argument("--studyOut", help="write the per-study summary tsv here")
    ap.add_argument("--locusOut", help="write the per-gene/locus summary tsv here")
    ap.add_argument("--chromSizes", default="/hive/data/genomes/hg38/chrom.sizes")
    args = ap.parse_args()

    meta = readMeta(args.metadata)
    oddMetaRows = crossedMetaRows(meta)
    coords = readCoords(args.coords)
    chromSizes = {}
    with open(args.chromSizes) as fh:
        for line in fh:
            c, s = line.split()
            chromSizes[c] = int(s)

    # probe -> list of (absDelta, signature, locus, disorder, delta, padj, study, pmid)
    byProbe = defaultdict(list)
    noMeta = defaultdict(int)
    perStudy = defaultdict(lambda: {"rows": 0, "probes": set(), "sigs": set()})
    perLocus = defaultdict(lambda: {"rows": 0, "probes": set(), "sigs": set()})
    nIn = 0
    nNoCoord = 0
    noCoordProbes = set()

    with gzip.open(args.loci, "rt") as fh:
        rdr = csv.DictReader(fh, delimiter="\t")
        for rec in rdr:
            nIn += 1
            probe = rec["IlmnID"]
            label = rec["Label"]
            key = label.replace(" ", "_")
            locus = rec["Gene_region_exposure"]
            study = rec["Study"]
            m = meta.get(key)
            disorder = m["disorder"] if m else ""
            if m is None:
                noMeta[label] += 1
            pmid = m["pmid"] if m else ""
            try:
                delta = float(rec["deltaBeta"])
            except ValueError:
                delta = 0.0
            padj = rec["padj"]

            perStudy[study]["rows"] += 1
            perStudy[study]["probes"].add(probe)
            perStudy[study]["sigs"].add(key)
            perLocus[locus]["rows"] += 1
            perLocus[locus]["probes"].add(probe)
            perLocus[locus]["sigs"].add(key)

            if probe not in coords:
                nNoCoord += 1
                noCoordProbes.add(probe)
                continue
            byProbe[probe].append((abs(delta), key, locus, disorder, delta, padj,
                                   study, pmid))

    # ---- write the bed ----
    beds = []
    allDisorders, allLoci, allStudies = set(), set(), set()
    nTrunc = 0
    for probe, hits in byProbe.items():
        chrom, start, end, source = coords[probe]
        if chrom not in chromSizes:
            continue
        if end > chromSizes[chrom]:
            # a 2bp CpG probe should never run off the end; stop rather than
            # quietly drop it
            sys.stderr.write("ERROR: %s at %s:%d-%d is past the end of %s (%d)\n"
                             % (probe, chrom, start, end, chrom, chromSizes[chrom]))
            sys.exit(1)
        # strongest effect first, then alphabetical so the order is stable
        hits.sort(key=lambda h: (-h[0], h[1]))

        sigs = [h[1] for h in hits]
        loci = [h[2] for h in hits]
        disorders = [h[3] for h in hits]
        deltas = [h[4] for h in hits]
        padjs = [h[5] for h in hits]
        studies = [h[6] for h in hits]

        # one linkout per publication, deduplicated, "id|label" so hgc shows the
        # study name rather than the bare PubMed ID
        pubs = OrderedDict()
        for h in hits:
            pmid, study = h[7], h[6]
            if pmid and pmid.isdigit():
                pubs[pmid] = study
        pubField = ",".join("%s|%s" % (p, s) for p, s in pubs.items())

        # hits are already sorted by descending |delta|, so deltas[0] is the
        # strongest effect at this site and carries the direction shown
        top = deltas[0]
        opposite = [abs(d) for d in deltas if (d < 0) != (top < 0)]
        if opposite and max(opposite) >= CONFLICT_FRAC * abs(top):
            colour, direction = CONFLICT, "Conflicting"
        elif top >= 0:
            if abs(top) >= STRONG_DELTA:
                colour, direction = HYPER_STRONG, "Strong hypermethylation"
            else:
                colour, direction = HYPER_WEAK, "Weak hypermethylation"
        else:
            if abs(top) >= STRONG_DELTA:
                colour, direction = HYPO_STRONG, "Strong hypomethylation"
            else:
                colour, direction = HYPO_WEAK, "Weak hypomethylation"

        score = min(1000, len(hits))
        maxAbs = max(abs(d) for d in deltas)

        # a probe can sit in 41 signatures; the mouseOver needs a short version
        uniqDis = list(OrderedDict((d, 1) for d in disorders if d))
        if len(uniqDis) > 3:
            summary = "%s and %d more" % (", ".join(uniqDis[:3]), len(uniqDis) - 3)
        else:
            summary = ", ".join(uniqDis)

        allDisorders.update(d for d in disorders if d)
        allLoci.update(loci)
        allStudies.update(studies)

        beds.append((chrom, start, end, probe, score, ".", start, end, colour,
                     len(hits),
                     ",".join(disorders),
                     ",".join(sigs),
                     ",".join(loci),
                     ",".join(fmtDelta(d) for d in deltas),
                     ",".join(fmtPval(p) for p in padjs),
                     ",".join(studies),
                     pubField,
                     direction,
                     "%.4f" % maxAbs,
                     source,
                     summary,
                     jsonTable([[sigs[i], disorders[i], fmtDelta(deltas[i]),
                                 fmtPval(padjs[i])] for i in range(len(hits))])))

    beds.sort(key=lambda b: (b[0], b[1], b[2]))
    with open(args.outBed, "w") as out:
        for b in beds:
            out.write("\t".join(str(x) for x in b) + "\n")

    # ---- trackDb filterValues fragment ----
    if args.raOut:
        with open(args.raOut, "w") as out:
            out.write("# generated by methaDoryToBed.py, do not edit by hand\n")
            out.write("filterValues.disorders %s\n" % ",".join(sorted(allDisorders)))
            out.write("filterType.disorders multipleListOr\n")
            out.write("filterValues.loci %s\n" % ",".join(sorted(allLoci)))
            out.write("filterType.loci multipleListOr\n")
            out.write("filterValues.studies %s\n" % ",".join(sorted(allStudies)))
            out.write("filterType.studies multipleListOr\n")
            # fixed order, strongest hyper to strongest hypo, so the menu reads
            # like the colour legend. None of these may contain a comma, which
            # is the filterValues separator.
            out.write("filterValues.direction %s\n" % ",".join(DIRECTIONS))
            out.write("filterType.direction multipleListOr\n")

    # ---- per-study summary for the description page ----
    if args.studyOut:
        with open(args.studyOut, "w") as out:
            out.write("study\tpmid\tsignatures\tdisorders\tprobeRows\tuniqProbes\n")
            for study in sorted(perStudy, key=lambda s: s.lower()):
                info = perStudy[study]
                disorders, pmids = OrderedDict(), OrderedDict()
                for key in sorted(info["sigs"]):
                    m = meta.get(key)
                    if not m:
                        continue
                    if m["disorder"]:
                        disorders[m["disorder"]] = 1
                    if m["pmid"]:
                        pmids[m["pmid"]] = 1
                out.write("%s\t%s\t%d\t%s\t%d\t%d\n"
                          % (study, ",".join(pmids), len(info["sigs"]),
                             ", ".join(disorders), info["rows"],
                             len(info["probes"])))

    # ---- per gene/locus summary for the description page ----
    if args.locusOut:
        with open(args.locusOut, "w") as out:
            out.write("locus\tdisorders\tsignatures\tstudies\tprobeRows\tuniqProbes\n")
            for locus in sorted(perLocus, key=lambda s: s.lower()):
                info = perLocus[locus]
                disorders, studies = OrderedDict(), OrderedDict()
                for key in sorted(info["sigs"]):
                    studies[key.rsplit("_", 1)[1]] = 1
                    m = meta.get(key)
                    if m and m["disorder"]:
                        disorders[m["disorder"]] = 1
                out.write("%s\t%s\t%d\t%s\t%d\t%d\n"
                          % (locus, ", ".join(disorders), len(info["sigs"]),
                             ", ".join(studies), info["rows"], len(info["probes"])))

    # ---- report ----
    sys.stderr.write("input rows:                  %d\n" % nIn)
    sys.stderr.write("rows with no probe position: %d (%d distinct probes)\n"
                     % (nNoCoord, len(noCoordProbes)))
    sys.stderr.write("bed features written:        %d\n" % len(beds))
    sys.stderr.write("distinct disorders:          %d\n" % len(allDisorders))
    sys.stderr.write("distinct genes/loci:         %d\n" % len(allLoci))
    sys.stderr.write("distinct studies:            %d\n" % len(allStudies))
    if noMeta:
        sys.stderr.write("\nWARNING: %d signature labels have no row in the metadata sheet.\n"
                         "They are kept, with an empty disorder:\n" % len(noMeta))
        for label in sorted(noMeta):
            sys.stderr.write("  %-30s %d probe rows\n" % (label, noMeta[label]))
    if oddMetaRows:
        sys.stderr.write("\nWARNING: %d metadata rows name one study in the 'signature' column and a\n"
                         "different author in StudyID. The row was used as written:\n" % len(oddMetaRows))
        sys.stderr.write("  %-32s %-16s %-10s %-26s %s\n"
                         % ("signature", "StudyID", "PMID", "Signature", "Disorder"))
        for key, study, pmid, sig, disorder in oddMetaRows:
            sys.stderr.write("  %-32s %-16s %-10s %-26s %s\n" % (key, study, pmid, sig, disorder))
    if noCoordProbes:
        with open(args.outBed + ".unmapped", "w") as out:
            for p in sorted(noCoordProbes):
                out.write(p + "\n")
        sys.stderr.write("\nprobes absent from all three Illumina manifests written to %s.unmapped\n"
                         % args.outBed)


main()
