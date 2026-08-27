#!/usr/bin/env python3
"""
Copy the source data files for the native singleCellSignalsPeaks track into the
genome's bed directory, mirroring each file's served relative path (Redmine
#37820 for hg38, #37914 for mm10).

For the given assembly it reads the hub build's main signal-&-peaks composite
(cellBrowser<Asm>) stanzas, and for every subtrack copies the source file
(resolved from the hub manifest by served relative path) to
  /hive/data/genomes/<asm>/bed/singleCellSignalsPeaks/<served-relpath>
The served subpath is preserved on purpose: some peak-file basenames repeat
across datasets, so a flat directory would clobber them, and keeping the subpath
lets the /gbdb/<asm>/bbi/singleCellSignalsPeaks symlink resolve every bigDataUrl.

It also copies the composite's facet metadata to
  <bed>/singleCellSignalsPeaks_metadata.tsv   (the track's metaDataUrl target),
and writes the cell-class facet colors to
  <bed>/singleCellSignalsPeaks_colors.json    (the track's colorSettingsUrl target).

Usage:
  copySingleCellSignalsPeaksFiles.py [--assembly hg38|mm10] [--stanzas STANZAS]
                                     [--manifest MANIFEST] [--dry-run]
"""
import re, os, json, shutil, argparse
from urllib.parse import urlparse

# Where the hub build writes manifest.tsv -- its OUTPUT dir, not its code. The build
# itself lives in the cellBrowser repo (ucsc/allTracksHub):
#   https://github.com/ucscGenomeBrowser/cellBrowser/tree/develop/ucsc/allTracksHub
# Its output dir is set there by CBHUB_OUT; keep this default in step with it (or pass
# --manifest).
HUB_BUILD = os.environ.get(
    "HUB_BUILD", "/hive/data/inside/cells/all-tracks-hub-build")
TRACK = "singleCellSignalsPeaks"

def copy_atomic(src, dst):
    """Copy src to dst without ever leaving a half-written dst in place.

    The bed dir is served live -- /gbdb/<asm>/bbi/singleCellSignalsPeaks is a symlink
    straight to it -- so copying onto a file the browser may be reading hands out a
    truncated bigBed for as long as the copy takes. Write a temp file beside the
    destination and rename it, which is atomic within one filesystem.
    """
    tmp = "%s.tmp%d" % (dst, os.getpid())
    try:
        shutil.copy2(src, tmp)
        os.replace(tmp, dst)
    except BaseException:
        if os.path.exists(tmp):
            os.remove(tmp)
        raise

def write_text_atomic(text, dst):
    """Write text to dst without ever leaving a half-written dst in place.

    Same reason as copy_atomic: the bed dir is served live, and the faceted UI fetches
    this file on every hgTrackUi page load, so a partial write hands out unparseable JSON
    for as long as the write takes.
    """
    tmp = "%s.tmp%d" % (dst, os.getpid())
    try:
        with open(tmp, "w") as fh:
            fh.write(text)
        os.replace(tmp, dst)
    except BaseException:
        if os.path.exists(tmp):
            os.remove(tmp)
        raise

def up_to_date(src, dst):
    """True if dst already holds this copy of src, so it can be skipped.

    Size alone is not enough: a rebuilt file often lands on the same size, and
    skipping it then serves last month's data forever. shutil.copy2 carries the
    mtime across, so a dst older than its source means the source moved on.
    """
    if not os.path.exists(dst):
        return False
    return (os.path.getsize(dst) == os.path.getsize(src)
            and os.path.getmtime(dst) >= os.path.getmtime(src))

def palette_file(build):
    """Path to the cell class -> color palette TSV (class<TAB>R,G,B, one per line).

    Same resolution order as makeSingleCellSignalsPeaksRa.py: prefer the copy of record
    archived alongside these scripts (written by build_celltype_crosswalks.py), fall back
    to the hub build dir. The two scripts must agree on the palette, or the swatches in
    the faceted selector would not match the colors the subtracks are drawn in.
    """
    pal = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                       "celltype-crosswalks", "celltype-palette.tsv")
    if not os.path.isfile(pal):
        pal = os.path.join(build, "celltype-crosswalks", "celltype-palette.tsv")
    return pal

def colors_json(pal):
    """Render the palette as the faceted UI's colorSettingsUrl JSON.

    facetedComposite.js wants {facetName: {facetValue: cssColor}} and draws a swatch
    beside each checkbox of any facet named in it. The facet name must be the metadata
    column ("Cell_class") and each key must be the column value verbatim -- the lookup
    is an exact string match, so a case or spacing difference silently drops the swatch.
    The whole palette is emitted, not just the classes this assembly uses: extra keys are
    ignored by the JS, and it keeps hg38 and mm10 sharing one class->color mapping.
    """
    colors = {}
    for line in open(pal):
        f = line.rstrip("\n").split("\t")
        if len(f) < 2 or not f[0].strip():
            continue
        rgb = [int(x) for x in f[1].split(",")]
        colors[f[0]] = "#%02X%02X%02X" % tuple(rgb)
    if not colors:
        raise SystemExit("ERROR: no class/color rows read from %s, so the facet color "
                         "swatches cannot be written." % pal)
    return json.dumps({"Cell_class": colors}, indent=4, sort_keys=True) + "\n"

def load_relpath_to_abs(manifest, asm):
    m = {}
    with open(manifest) as fh:
        hdr = fh.readline().rstrip("\n").split("\t")
        ai, ui, asmi = hdr.index("abs_path"), hdr.index("track_url"), hdr.index("assembly")
        for line in fh:
            f = line.rstrip("\n").split("\t")
            if len(f) <= max(ai, ui, asmi) or f[asmi] != asm:
                continue
            m[urlparse(f[ui]).path.lstrip("/")] = f[ai]
    return m

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--assembly", default="hg38", choices=["hg38", "mm10"])
    ap.add_argument("--stanzas",
                    help="hub stanza file (default: <build>/stanzas/<asm>.trackDb.txt)")
    ap.add_argument("--manifest",
                    help="hub manifest TSV (default: the manifest.tsv of the build "
                         "--stanzas came from)")
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    asm = args.assembly
    hub_composite = "cellBrowser" + asm.capitalize()
    stanzas = args.stanzas or os.path.join(HUB_BUILD, "stanzas/%s.trackDb.txt" % asm)
    # Locate the rest of the build relative to the stanza file rather than off HUB_BUILD, so
    # that --stanzas on its own moves the whole script to another build -- the way it already
    # does in makeSingleCellSignalsPeaksRa.py. Keying these off HUB_BUILD instead meant
    # --stanzas read one build's stanzas and then resolved every file against the default
    # build's manifest, and copied the default build's facet metadata on top.
    # Layout: <build>/manifest.tsv, <build>/stanzas/<asm>.trackDb.txt,
    #         <build>/meta/<asm>.metadata.tsv
    build = os.path.dirname(os.path.dirname(os.path.abspath(stanzas)))
    manifest = args.manifest or os.path.join(build, "manifest.tsv")
    beddir = "/hive/data/genomes/%s/bed/%s" % (asm, TRACK)
    rel2abs = load_relpath_to_abs(manifest, asm)

    copied = missing = total = 0
    nbytes = 0
    misses = []
    for s in re.split(r"\n\s*\n", open(stanzas).read().strip()):
        # Dedent, and match the parent's first token rather than the whole line. The hub
        # stanzas are indented to show their hierarchy and each child says
        # "parent <composite> off", so an anchored whole-line compare matched nothing and
        # this copied zero files without complaining.
        lines = [l.lstrip() for l in s.splitlines()]
        parent = next((l for l in lines if l.startswith("parent ")), "").split()
        if len(parent) < 2 or parent[1] != hub_composite:
            continue
        bdu = next((l for l in lines if l.strip().startswith("bigDataUrl ")), None)
        if not bdu:
            continue
        total += 1
        rel = urlparse(bdu.split(None, 1)[1].strip()).path.lstrip("/")
        src = rel2abs.get(rel)
        if not src or not os.path.exists(src):
            missing += 1
            misses.append(rel)
            continue
        dst = os.path.join(beddir, rel)
        if not args.dry_run:
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            if not up_to_date(src, dst):
                copy_atomic(src, dst)
        copied += 1
        nbytes += os.path.getsize(src)

    # Bail out before the metadata copy, not after. Copying zero data files and then
    # refreshing the facet metadata anyway leaves the bed dir describing tracks whose files
    # were never written -- exactly the inconsistency this check exists to prevent.
    if total == 0:
        raise SystemExit("ERROR: no subtracks of %s found in %s. Has the hub stanza "
                         "layout changed? Copying nothing is never right here."
                         % (hub_composite, stanzas))

    # Finding every subtrack and then resolving none of them to a file is the same
    # silent failure one step further in: the manifest and the stanzas are describing
    # different builds. Copying nothing is still never right here.
    if copied == 0:
        raise SystemExit("ERROR: %d subtracks of %s found in %s, but not one of their "
                         "source files could be resolved from %s. Do the stanzas and the "
                         "manifest come from the same build?"
                         % (total, hub_composite, stanzas, manifest))

    # Missing metadata is fatal, the same way it is in makeSingleCellSignalsPeaksRa.py.
    # Skipping it quietly leaves the bed dir advertising the previous build's facets
    # against this build's data files, which is the mismatch nobody would go looking for.
    meta_src = os.path.join(build, "meta", "%s.metadata.tsv" % asm)
    meta_dst = os.path.join(beddir, "%s_metadata.tsv" % TRACK)
    if not os.path.isfile(meta_src):
        raise SystemExit("ERROR: no facet metadata at %s, so %s cannot be refreshed. The "
                         "track's metaDataUrl would keep pointing at the previous build's "
                         "facets." % (meta_src, meta_dst))
    if not args.dry_run:
        os.makedirs(beddir, exist_ok=True)
        copy_atomic(meta_src, meta_dst)

    # Facet color swatches: the track's colorSettingsUrl target. Derived from the same
    # class->color palette the subtrack "color" lines come from, so a class has one color
    # in the selector and in the track display. A missing palette is fatal for the same
    # reason a missing metadata file is: the .ra names this file, and quietly leaving the
    # previous build's copy in place would show swatches that no longer match the tracks.
    pal = palette_file(build)
    if not os.path.isfile(pal):
        raise SystemExit("ERROR: no cell class palette at %s, so the facet color swatches "
                         "cannot be written. The track's colorSettingsUrl would keep "
                         "pointing at the previous build's colors." % pal)
    colors_dst = os.path.join(beddir, "%s_colors.json" % TRACK)
    if not args.dry_run:
        write_text_atomic(colors_json(pal), colors_dst)

    print("assembly=%s composite=%s: subtracks=%d copied=%d missing=%d  ~%.1f GB%s"
          % (asm, hub_composite, total, copied, missing, nbytes / 1e9,
             "  (dry-run)" if args.dry_run else "  -> " + beddir))
    if misses:
        print("MISSING %d source files:" % len(misses))
        for r in misses[:25]:
            print("  " + r)

if __name__ == "__main__":
    main()
