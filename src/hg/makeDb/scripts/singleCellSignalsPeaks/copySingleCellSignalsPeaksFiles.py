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
  <bed>/singleCellSignalsPeaks_metadata.tsv   (the track's metaDataUrl target).

Usage:
  copySingleCellSignalsPeaksFiles.py [--assembly hg38|mm10] [--dry-run]
"""
import re, os, shutil, argparse
from urllib.parse import urlparse

# Where the hub build writes manifest.tsv -- its OUTPUT dir, not its code. The build
# itself lives in the cellBrowser repo (ucsc/allTracksHub):
#   https://github.com/ucscGenomeBrowser/cellBrowser/tree/develop/ucsc/allTracksHub
# Its output dir is set there by CBHUB_OUT; keep this default in step with it (or pass
# --manifest).
HUB_BUILD = os.environ.get(
    "HUB_BUILD", "/hive/data/inside/cells/all-tracks-hub-build")
TRACK = "singleCellSignalsPeaks"

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
    ap.add_argument("--stanzas")
    ap.add_argument("--manifest", default=os.path.join(HUB_BUILD, "manifest.tsv"))
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    asm = args.assembly
    hub_composite = "cellBrowser" + asm.capitalize()
    stanzas = args.stanzas or os.path.join(HUB_BUILD, "stanzas/%s.trackDb.txt" % asm)
    beddir = "/hive/data/genomes/%s/bed/%s" % (asm, TRACK)
    rel2abs = load_relpath_to_abs(args.manifest, asm)

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
            if not (os.path.exists(dst) and os.path.getsize(dst) == os.path.getsize(src)):
                shutil.copy2(src, dst)
        copied += 1
        nbytes += os.path.getsize(src)

    meta_src = os.path.join(HUB_BUILD, "meta", "%s.metadata.tsv" % asm)
    meta_dst = os.path.join(beddir, "%s_metadata.tsv" % TRACK)
    if not args.dry_run and os.path.exists(meta_src):
        os.makedirs(beddir, exist_ok=True)
        shutil.copy2(meta_src, meta_dst)

    if total == 0:
        raise SystemExit("ERROR: no subtracks of %s found in %s. Has the hub stanza "
                         "layout changed? Copying nothing is never right here."
                         % (hub_composite, stanzas))
    print("assembly=%s composite=%s: subtracks=%d copied=%d missing=%d  ~%.1f GB%s"
          % (asm, hub_composite, total, copied, missing, nbytes / 1e9,
             "  (dry-run)" if args.dry_run else "  -> " + beddir))
    if misses:
        print("MISSING %d source files:" % len(misses))
        for r in misses[:25]:
            print("  " + r)

if __name__ == "__main__":
    main()
