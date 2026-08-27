#!/usr/bin/env python3
"""
Regenerate the native singleCellSignalsPeaks trackDb .ra for a genome assembly
from the Cell Browser all-tracks-hub build (Redmine #37820 for hg38, #37914 for
mm10).

The native "singleCellSignalsPeaks" faceted composite is the Genome Browser
version of the hub's main signal-&-peaks composite for that assembly
(cellBrowserHg38 for hg38, cellBrowserMm10 for mm10). This script takes that
composite's stanzas from the hub build and rewrites them into a native track:
  - the composite is renamed cellBrowser<Asm> -> singleCellSignalsPeaks
  - each subtrack bigDataUrl is repointed to the local /gbdb copy
  - subtrack colors / labels / types are carried through unchanged (so the
    harmonized cell-type labels and any per-track colors come along for free)

The data files themselves are copied into
/hive/data/genomes/<asm>/bed/singleCellSignalsPeaks/<served-relpath> (see
copySingleCellSignalsPeaksFiles.py) and served via the
/gbdb/<asm>/bbi/singleCellSignalsPeaks symlink; this script only (re)writes the
trackDb .ra.

Usage:
  makeSingleCellSignalsPeaksRa.py [--assembly hg38|mm10] [--stanzas STANZAS]
                                  [--meta META] [--no-meta-check] [--out OUT]
"""
import re, os, sys, argparse
from urllib.parse import urlparse

# Where the hub build writes its stanzas and metadata -- its OUTPUT dir, not its code.
# The build itself lives in the cellBrowser repo (ucsc/allTracksHub), since it builds the
# whole Cell Browser super hub and not just this track:
#   https://github.com/ucscGenomeBrowser/cellBrowser/tree/develop/ucsc/allTracksHub
# Its output dir is set there by CBHUB_OUT; keep this default in step with it.
HUB_BUILD = os.environ.get(
    "HUB_BUILD", "/hive/data/inside/cells/all-tracks-hub-build")
TRACK = "singleCellSignalsPeaks"
GROUP = "regulation"                  # ATAC-seq signal/peaks live with the ENCODE
                                      # regulatory tracks, not under singleCell
ORG = {"hg38": "human", "mm10": "mouse"}   # trackDb organism subdir per assembly

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--assembly", default="hg38", choices=sorted(ORG))
    ap.add_argument("--stanzas",
                    help="hub stanza file (default: <build>/stanzas/<asm>.trackDb.txt)")
    ap.add_argument("--meta",
                    help="facet metadata TSV to check the .ra against (default: the "
                         "meta/<asm>.metadata.tsv of the build --stanzas came from)")
    ap.add_argument("--no-meta-check", action="store_true",
                    help="skip the .ra-vs-metadata 1:1 check. Only for when the metadata "
                         "genuinely does not exist; it is the check that catches a "
                         "partly-written stanza file.")
    ap.add_argument("--out")
    args = ap.parse_args()

    asm = args.assembly
    hub_composite = "cellBrowser" + asm.capitalize()   # cellBrowserHg38 / cellBrowserMm10
    gbdb = "/gbdb/%s/bbi/%s" % (asm, TRACK)
    stanzas = args.stanzas or os.path.join(HUB_BUILD, "stanzas/%s.trackDb.txt" % asm)
    # Locate the rest of the build relative to the stanza file rather than off HUB_BUILD,
    # so that --stanzas on its own moves the whole script to another build. Pointing only
    # the stanzas at a second build used to check them against the default build's
    # metadata and abort on a mismatch that was not really there.
    # Layout: <build>/stanzas/<asm>.trackDb.txt and <build>/meta/<asm>.metadata.tsv
    build = os.path.dirname(os.path.dirname(os.path.abspath(stanzas)))
    meta = args.meta or os.path.join(build, "meta", "%s.metadata.tsv" % asm)
    out = args.out or os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "../../trackDb/%s/%s/%s.ra" % (ORG[asm], asm, TRACK))

    header = "\n".join([
        "track " + TRACK,
        "compositeTrack faceted",
        "group " + GROUP,
        "visibility hide",
        "type bigBed 3",
        "shortLabel Single-cell ATAC-seq",
        "longLabel Single-cell ATAC-seq Peaks and Signals for UCSC Cell Browser datasets",
        "metaDataUrl %s/%s_metadata.tsv" % (gbdb, TRACK),
        "primaryKey Track",
        "subtrackUrls Dataset=https://cells.ucsc.edu/?ds=$$",
        "defaultSortField Cell_class",
        "maxCheckboxes 200",
        # One scale across every selected subtrack, so two tracks drawn at the same locus
        # can be compared directly; with per-track autoScale, tracks whose values differ
        # by orders of magnitude both drew full height. It must sit on the composite, not
        # the children: hgTracks groups by tdb->parent (wigTrack.c setMinMax), and a
        # per-subtrack setting would override this one. Limits are taken from the data in
        # the current window, not genome-wide, so an outlying region elsewhere in the
        # genome cannot flatten the view.
        "autoScale group",
    ])

    # class ordering for subtrack priority: palette line order (neurons, glia,
    # vascular, immune, ...) so same-class tracks group together in the display,
    # with the source (hub/dataset) order preserved within a class. The subtrack's
    # broad class is recovered from its color (palette is 1:1 class<->color).
    color_rank = {}
    # prefer the palette archived alongside this script (the copy of record, written by
    # build_celltype_crosswalks.py); fall back to the hub build dir
    _palf = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                         "celltype-crosswalks", "celltype-palette.tsv")
    if not os.path.isfile(_palf):
        _palf = os.path.join(build, "celltype-crosswalks", "celltype-palette.tsv")
    for _i, _l in enumerate(open(_palf)):
        _pp = _l.rstrip("\n").split("\t")
        if len(_pp) >= 2:
            color_rank[_pp[1]] = _i
    class_seq = {}                        # rank -> running counter within that class

    # a source path segment of "old" / "*.old" / "*_old" marks deprecated data
    # (e.g. cortex-atac/hub/interact.old/); the hub may keep it, but the native
    # track must not carry it.
    OLD_SEG = re.compile(r"(^|/)[^/]*(\.old|_old|\bold)($|/)", re.I)

    out_stanzas = [header]
    n = skipped_old = n_stanzas = n_parented = 0
    bad_src = []          # source track names not in the cellBrowser<Asm>_ namespace
    for s in re.split(r"\n\s*\n", open(stanzas).read().strip()):
        # Dedent: the hub stanzas are indented to show their hierarchy, and this script
        # re-indents on write. Parsing them indented broke the "track " rename below
        # without tripping any count check -- subtracks kept their cellBrowser<Asm> names
        # and no longer matched their facet metadata rows.
        lines = [l.lstrip() for l in s.splitlines()]
        n_stanzas += 1
        # Match on the parent's first token rather than the whole line. The exact-string
        # compare this replaces would have silently skipped every stanza if the hub ever
        # emitted "parent <composite> off" or changed its spacing, leaving a header-only
        # .ra and a zero exit status.
        parent = next((l for l in lines if l.strip().startswith("parent ")), "")
        ptoks = parent.split()
        if len(ptoks) >= 2:
            n_parented += 1
        if len(ptoks) < 2 or ptoks[1] != hub_composite:
            continue
        bdu = next((l for l in lines if l.strip().startswith("bigDataUrl ")), "")
        rel_check = urlparse(bdu.split(None, 1)[1].strip()).path if bdu else ""
        if OLD_SEG.search(rel_check):
            skipped_old += 1
            continue
        n += 1
        # priority groups tracks by broad class (via color), source order within
        color = next((l.split(None, 1)[1].strip() for l in lines
                      if l.strip().startswith("color ")), "")
        rank = color_rank.get(color, len(color_rank))     # unknown/uncolored last
        seq = class_seq.get(rank, 0); class_seq[rank] = seq + 1
        priority = rank * 100000 + seq
        newl = []
        for l in lines:
            if l.startswith("track "):
                # Validate the SOURCE name before slicing it. This used to chop a fixed
                # number of characters off whatever it was given, so a track not actually
                # named cellBrowser<Asm>_* came out silently mangled but well-formed
                # (BOGUS_allen_basal_ganglia_atac__dorsal -> ..._ia_atac__dorsal), which no
                # check on the output could catch.
                src_name = l.split(None, 1)[1]
                if not src_name.startswith(hub_composite + "_"):
                    bad_src.append(src_name)
                    continue
                suffix = src_name[len(hub_composite) + 1:]
                newl.append("track %s_%s" % (TRACK, suffix))
            elif l.strip().startswith("parent ") and l.split()[1] == hub_composite:
                # "off" so every subtrack is unchecked by default; the user turns
                # on individual tracks via the faceted selector
                newl.append("parent " + TRACK + " off")
                newl.append("priority " + str(priority))
            elif l.strip().startswith("bigDataUrl "):
                rel = urlparse(l.split(None, 1)[1].strip()).path.lstrip("/")
                newl.append("bigDataUrl %s/%s" % (gbdb, rel))
            else:
                newl.append(l)
        out_stanzas.append("\n".join(newl))

    # Every emitted subtrack must be "track <TRACK>_<non-empty suffix>". Checking only the
    # prefix was not enough: the rename always prepends it, so a source name that did not
    # start with the hub composite still passed, just with an empty suffix.
    if bad_src:
        sys.exit("ERROR: %d source track name(s) are not %s_* , e.g. %r. Renaming them "
                 "would silently mangle the name. Has the hub stanza layout changed?"
                 % (len(bad_src), hub_composite, bad_src[0]))
    _ok = re.compile(r"^track %s_\S+$" % re.escape(TRACK))
    bad = [x.split("\n", 1)[0] for x in out_stanzas[1:]
           if not _ok.match(x.split("\n", 1)[0])]
    if bad:
        sys.exit("ERROR: %d subtrack(s) were not renamed into the %s namespace, e.g. %r. "
                 "Has the hub stanza layout changed?" % (len(bad), TRACK, bad[0]))

    # Sanity checks: fail loudly rather than write a truncated .ra. A hub-format change
    # that stops the parent line matching would otherwise produce a header-only file and
    # exit 0, and the next trackDb load would quietly drop every subtrack.
    if n == 0:
        sys.exit("ERROR: no subtracks matched composite '%s' in %s "
                 "(%d stanzas, %d with a parent line). Has the hub stanza format "
                 "changed?" % (hub_composite, stanzas, n_stanzas, n_parented))
    # The facet metadata is the parallel artifact: build_stanzas writes one row per
    # subtrack of this composite, so the counts must agree once the old-dir skips are
    # added back. A mismatch means the .ra and the metadata disagree, which shows up in
    # the browser as subtracks with no facet row (or facet rows with no track).
    #
    # Missing metadata is fatal, not a warning. This count is the only check that sees a
    # partly-written stanza file: the n == 0 check above catches losing every subtrack,
    # but a stanza file holding 22 of 925 subtracks passes it and writes a .ra that is
    # 903 tracks short. Warning and continuing put that hole straight back.
    if args.no_meta_check:
        sys.stderr.write("WARNING: --no-meta-check given; the .ra was not checked against "
                         "%s. A short stanza file would not have been noticed.\n" % meta)
    elif not os.path.isfile(meta):
        sys.exit("ERROR: no facet metadata at %s, so the .ra cannot be checked against it. "
                 "Pass --meta to point at the right file, or --no-meta-check to skip the "
                 "check on purpose." % meta)
    else:
        with open(meta) as fh:
            meta_rows = sum(1 for _ in fh) - 1          # minus the header
        if meta_rows != n + skipped_old:
            sys.exit("ERROR: %s has %d rows but %d subtracks were kept (+%d old-dir "
                     "skipped); the .ra and the facet metadata must match 1:1"
                     % (meta, meta_rows, n, skipped_old))

    # Indent subtracks under the composite, as the trackDb .ra files in the tree do
    # (chainNet, encode3): the container sits flush left and each level below it is
    # indented one step, with every line of the stanza moving together.
    def indent(stanza, width=4):
        lines = stanza.split("\n")
        if not any(l.startswith("parent ") for l in lines):
            return stanza
        pad = " " * width
        return "\n".join(pad + l if l.strip() else l for l in lines)

    with open(os.path.abspath(out), "w") as fh:
        fh.write("\n\n".join(indent(s) for s in out_stanzas) + "\n")
    print("wrote %s: %d subtracks (assembly=%s, composite=%s, group=%s; skipped %d old-dir)" % (
        out, n, asm, hub_composite, GROUP, skipped_old))

if __name__ == "__main__":
    main()
