#!/usr/bin/env python3
"""
Regenerate human/hg38/singleCellSignalsPeaks.ra from the Cell Browser
all-tracks-hub build (Redmine #37820).

The native hg38 "singleCellSignalsPeaks" faceted composite is the browser
version of the hub's main hg38 signal-&-peaks composite (cellBrowserHg38). This
script takes that composite's stanzas from the hub build and rewrites them into
a native track:
  - the composite is renamed cellBrowserHg38 -> singleCellSignalsPeaks
  - each subtrack bigDataUrl is repointed to the local /gbdb copy
  - subtrack colors / labels / types are carried through unchanged (so the
    SEA-AD subclass colors and harmonized cell-type labels come along for free)

The data files themselves are copied into
/hive/data/genomes/hg38/bed/singleCellSignalsPeaks/<served-relpath> and served
via the /gbdb/hg38/bbi/singleCellSignalsPeaks symlink (see the makeDoc); this
script only (re)writes the trackDb .ra.

Usage:
  makeSingleCellSignalsPeaksRa.py [--stanzas STANZAS] [--out OUT]
"""
import re, os, argparse
from urllib.parse import urlparse

HUB_BUILD = "/hive/users/mspeir/claude/cell-browser/all-tracks-hub-build"
GBDB = "/gbdb/hg38/bbi/singleCellSignalsPeaks"
HUB_COMPOSITE = "cellBrowserHg38"     # main hg38 signal-&-peaks faceted composite
TRACK = "singleCellSignalsPeaks"
GROUP = "regulation"                  # ATAC-seq signal/peaks live with the ENCODE
                                      # regulatory tracks, not under singleCell

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--stanzas", default=os.path.join(HUB_BUILD, "stanzas/hg38.trackDb.txt"))
    ap.add_argument("--out", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "../../trackDb/human/hg38/singleCellSignalsPeaks.ra"))
    args = ap.parse_args()

    header = "\n".join([
        "track " + TRACK,
        "compositeTrack faceted",
        "group " + GROUP,
        "visibility hide",
        "type bigBed 3",
        "shortLabel Cell Browser signal & peaks",
        "longLabel Signal and peak tracks from UCSC Cell Browser datasets",
        "metaDataUrl %s/%s_metadata.tsv" % (GBDB, TRACK),
        "primaryKey Track",
        "subtrackUrls Dataset=https://cells.ucsc.edu/?ds=$$",
        "defaultSortField Dataset",
        "maxCheckboxes 200",
    ])

    out = [header]
    n = 0
    for s in re.split(r"\n\s*\n", open(args.stanzas).read().strip()):
        lines = s.splitlines()
        parent = next((l for l in lines if l.startswith("parent ")), "")
        if parent.strip() != "parent " + HUB_COMPOSITE:
            continue
        n += 1
        newl = []
        for l in lines:
            if l.startswith("track "):
                suffix = l.split(None, 1)[1][len(HUB_COMPOSITE) + 1:]
                newl.append("track %s_%s" % (TRACK, suffix))
            elif l.strip() == "parent " + HUB_COMPOSITE:
                newl.append("parent " + TRACK)
            elif l.strip().startswith("bigDataUrl "):
                rel = urlparse(l.split(None, 1)[1].strip()).path.lstrip("/")
                newl.append("bigDataUrl %s/%s" % (GBDB, rel))
            else:
                newl.append(l)
        out.append("\n".join(newl))

    with open(os.path.abspath(args.out), "w") as fh:
        fh.write("\n\n".join(out) + "\n")
    print("wrote %s: %d subtracks (group=%s)" % (args.out, n, GROUP))

if __name__ == "__main__":
    main()
