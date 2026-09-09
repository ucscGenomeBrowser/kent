#!/usr/bin/env python3
""" Assemble the bTaeGut7 hub from the trackDb in the kent tree and the files that
"hubtools import igv" converted.

The trackDb is NOT generated here.  It is a normal, hand-maintainable trackDb kept at

    src/hg/makeDb/trackDb/contrib/bTaeGut7/bTaeGut7.trackDb.txt

and this script copies it into the hub as it is.  Edit that file to change a label, a
colour, a visibility or the track order; nothing here will overwrite it.  The same file
serves every assembly, because a track either has data on both haplotypes or on neither,
and here all 21 have data on both.

What this script still does is the mechanical part that a trackDb cannot express: hubtools
names its output after the file names in the IGV session, so satellome.bb arrives called
bTaeGut7v04_MT_rDNAsatellomev01bed_14.bb.  sourceTracks.tsv maps the two, and the data
files are hardlinked into the hub under the names the trackDb uses.

Usage:  makeHub.py <buildDir> <outDir>

  buildDir  output of "hubtools import igv", holding hub.txt and the data directory
  outDir    hub to write

The output is a classic hub, the same shape "hubtools splitHap" produces, so that one
trackDb.txt fits both:

  outDir/hub.txt
  outDir/genomes.txt
  outDir/<accession>/trackDb.txt
  outDir/<accession>/<data files and description pages>
"""

import sys, os, shutil
from os.path import join, dirname, abspath, isfile, basename, splitext

scriptDir = dirname(abspath(__file__))
# the trackDb and the description pages live with the other contributed GenArk tracks
tdbDir = abspath(join(scriptDir, "..", "..", "..", "trackDb", "contrib", "bTaeGut7"))

def readSourceTracks(fname):
    """ trackName -> the name hubtools gave the same track.  Two columns, tab separated,
    comments and blank lines ignored. """
    out = {}
    for line in open(fname):
        if line.startswith("#") or not line.strip():
            continue
        row = line.rstrip("\n").split("\t")
        if len(row) != 2:
            sys.exit("%s: expected two tab separated columns, got: %s" % (fname, line.rstrip()))
        out[row[0]] = row[1]
    return out

def parseStanzas(fname):
    " read a trackDb-style file into a list of dicts, one per stanza "
    stanzas, cur = [], {}
    for line in open(fname):
        line = line.rstrip("\r\n")
        if not line.strip() or line.lstrip().startswith("#"):
            if cur:
                stanzas.append(cur)
                cur = {}
            continue
        key, _, val = line.strip().partition(" ")
        cur[key] = val.strip()
    if cur:
        stanzas.append(cur)
    return stanzas

def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    buildDir, outDir = sys.argv[1], sys.argv[2]

    # the master is named for its project, the way mkGenomes.pl wants a contributed
    # trackDb; inside the hub it becomes plain trackDb.txt, which genomes.txt points at
    tdbFname = join(tdbDir, "bTaeGut7.trackDb.txt")
    if not isfile(tdbFname):
        sys.exit("no trackDb at %s" % tdbFname)
    hubHeader = join(scriptDir, "hubHeader.txt")
    if not isfile(hubHeader):
        sys.exit("no hub stanza at %s" % hubHeader)
    srcMap = readSourceTracks(join(scriptDir, "sourceTracks.tsv"))

    genStanzas = parseStanzas(join(buildDir, "hub.txt"))
    genome = next(s["genome"] for s in genStanzas if "genome" in s)
    genTracks = {s["track"]: s for s in genStanzas if "track" in s}

    # the trackDb and the conversion have to agree about which tracks exist, otherwise a
    # stanza would point at a file that was never written
    tdbTracks = [s["track"] for s in parseStanzas(tdbFname)
                 if "track" in s and "bigDataUrl" in s]
    missing = [t for t in tdbTracks if t not in srcMap]
    if missing:
        sys.exit("sourceTracks.tsv has no entry for these trackDb tracks: %s"
                 % ", ".join(missing))
    unknown = [t for t in tdbTracks if srcMap[t] not in genTracks]
    if unknown:
        sys.exit("%s/hub.txt does not have the source tracks of: %s. The IGV session has "
                 "probably changed, update sourceTracks.tsv." % (buildDir, ", ".join(unknown)))

    dataDir = join(outDir, genome)
    if not os.path.isdir(dataDir):
        os.makedirs(dataDir)

    linked = 0
    for tdb in parseStanzas(tdbFname):
        name = tdb.get("track")
        url = tdb.get("bigDataUrl", "")
        if name is None or not url or url.startswith("http"):
            continue
        genUrl = genTracks[srcMap[name]].get("bigDataUrl", "")
        if genUrl.startswith("http"):
            sys.exit("%s: the trackDb wants a local file %s but hubtools left this track "
                     "pointing at %s" % (name, url, genUrl))
        srcPath = join(buildDir, genUrl)
        dstPath = join(dataDir, basename(url))
        if splitext(genUrl)[1] != splitext(url)[1]:
            sys.exit("%s: trackDb says %s but the converted file is %s"
                     % (name, basename(url), basename(genUrl)))
        if not isfile(dstPath):
            try:
                os.link(srcPath, dstPath)
            except OSError:
                shutil.copy(srcPath, dstPath)
        linked += 1

    shutil.copy(tdbFname, join(dataDir, "trackDb.txt"))
    pages = 0
    for fname in os.listdir(tdbDir):
        if fname.endswith(".html"):
            shutil.copy(join(tdbDir, fname), join(dataDir, fname))
            pages += 1

    ofh = open(join(outDir, "genomes.txt"), "wt")
    ofh.write("genome %s\ntrackDb %s/trackDb.txt\n\n" % (genome, genome))
    ofh.close()

    hubTxt = join(outDir, "hub.txt")
    ofh = open(hubTxt, "wt")
    for line in open(hubHeader):
        if line.strip() and not line.lstrip().startswith("#"):
            ofh.write(line)
    ofh.write("genomesFile genomes.txt\n")
    ofh.close()

    descName = None
    for line in open(hubHeader):
        if line.startswith("descriptionUrl "):
            descName = line.split(" ", 1)[1].strip()
    if descName and isfile(join(tdbDir, descName)):
        shutil.copy(join(tdbDir, descName), join(outDir, descName))

    print("Wrote %s on %s: %d data files, %d description pages"
          % (hubTxt, genome, linked, pages))

main()
