#!/usr/bin/env python3
"""
Otto update for the Gene2Phenotype (G2P) track on hg19 and hg38.

Originally g2pWrangle.py by Jairo. Converted to an otto worker.

DO NOT EDIT THE HIVE COPY DIRECTLY. The source of truth is the kent tree:
    ~/kent/src/hg/utils/otto/g2p/doG2p.py
Edit + commit there, then copy to /hive/data/outside/otto/g2p/ (the
ottoCompareGitVsHiveFiles.py checker emails otto-group if they diverge).

What it does, once a month:
  1. Download the full G2P panel CSV.
  2. No-op (silent) if the download is byte-identical to last run's copy.
  3. Sanity check: required columns must all be present, else abort loudly.
  4. For hg19 and hg38: join G2P records to gene coords from the HGNC bigBed
     track and build a bed9+20 bigBed in a dated working directory.
  5. Guard: abort if item count moved >10% vs the live track (unless --force).
  6. Atomically repoint /gbdb/<db>/g2p/g2p.bb at the new dated bigBed.

The dated working directories double as the archive of past builds.
"""

import argparse
import csv
import subprocess
import sys
from datetime import datetime
from pathlib import Path

WORKDIR = "/hive/data/outside/otto/g2p"
DBS = ["hg19", "hg38"]
DOWNLOAD_URL = "https://www.ebi.ac.uk/gene2phenotype/api/panel/all/download"
AS_FILE = WORKDIR + "/g2p.as"
EXPECTED_COLUMNS_FILE = WORKDIR + "/expectedColumns.txt"
NEW_CSV = WORKDIR + "/AllG2P.csv"
PREV_CSV = WORKDIR + "/prevAllG2P.csv"
GBDB_BB = "/gbdb/%s/g2p/g2p.bb"          # live symlink, per-db
COUNT_TOLERANCE = 0.10                    # 10% item-count change requires --force

parser = argparse.ArgumentParser(description="Build and update the G2P track.")
parser.add_argument("--force", action="store_true",
                    help="Rebuild even if the download is unchanged, and bypass "
                         "the >10%% item-count safety check.")
args = parser.parse_args()


def bash(cmd):
    """Run cmd in a bash subprocess, returning stdout; raise on non-zero exit."""
    try:
        out = subprocess.run(cmd, check=True, shell=True, stdout=subprocess.PIPE,
                             universal_newlines=True, stderr=subprocess.STDOUT)
        return out.stdout
    except subprocess.CalledProcessError as e:
        raise RuntimeError("command '{}' returned error (code {}): {}".format(
            e.cmd, e.returncode, e.output))


def download(url, outFile):
    """Download the G2P panel CSV."""
    bash("curl -sSf -L -o %s '%s'" % (outFile, url))


def md5(path):
    return bash("md5sum %s" % path).split()[0]


def updateNeeded():
    """Download the CSV; return True if it differs from last run (or --force)."""
    download(DOWNLOAD_URL, NEW_CSV)
    if args.force:
        return True
    if not Path(PREV_CSV).exists():
        return True
    return md5(NEW_CSV) != md5(PREV_CSV)


def validateColumns(csvFile):
    """Abort if any required column is missing from the CSV header."""
    with open(EXPECTED_COLUMNS_FILE) as f:
        required = [line.strip() for line in f if line.strip()]
    with open(csvFile, newline="", encoding="utf-8") as f:
        header = next(csv.reader(f))
    header = [h.strip() for h in header]
    missing = [c for c in required if c not in header]
    if missing:
        sys.exit("ERROR: G2P CSV is missing expected column(s): %s\n"
                 "The source format may have changed; check %s" % (missing, csvFile))


# Confidence value -> itemRgb color. Unrecognized values fall back to DEFAULT_COLOR
# (black) and are counted/logged by joinAndWrite so a source change is visible.
CONFIDENCE_COLORS = {
    "definitive": "39,103,73",   # dark green
    "strong": "56,161,105",      # green
    "moderate": "104,211,145",   # light green
    "limited": "252,129,129",    # pink
    "disputed": "229,62,62",     # red
    "refuted": "155,44,44",      # dark red
}
DEFAULT_COLOR = "0,0,0"          # black, for unrecognized confidence values


def normalizeConfidence(confidence):
    """Fold a confidence string to its lookup form, so that case and stray
    whitespace do not make one value look like several."""
    return confidence.lower().strip()


def confidenceToColor(confidence):
    """Return the itemRgb color for a confidence string, or None if unrecognized."""
    return CONFIDENCE_COLORS.get(normalizeConfidence(confidence))


def loadG2p(filePath):
    """Load G2P CSV into a dict keyed by HGNC ID (each value is a list of rows)."""
    g2pMap = {}
    numOfRows = 0
    with open(filePath, newline="", encoding="utf-8") as csvfile:
        reader = csv.DictReader(csvfile)
        for row in reader:
            numOfRows += 1
            hgncId = row["hgnc id"].strip()
            g2pMap.setdefault(hgncId, []).append(row)
    print("Number of rows in file: %s" % numOfRows)
    return g2pMap


def loadCoordinates(db, hgncIds):
    """Build a dict of gene coordinates for the given HGNC IDs from the HGNC bigBed.

    One bigBedToBed pass over the whole track (~49k rows) instead of one
    bigBedNamedItems subprocess per HGNC ID. The bigBed name field is
    "HGNC:<id>"; the G2P CSV stores the bare numeric id, so we key on that.
    """
    wanted = set(hgncIds)
    coordMap = {}
    hgncBB = "/gbdb/%s/hgnc/hgnc.bb" % db
    for line in bash("bigBedToBed %s stdout" % hgncBB).split("\n"):
        if not line.strip():
            continue
        fields = line.split("\t")[:8]
        name = fields[3]                       # e.g. "HGNC:36036"
        hgncId = name.split("HGNC:")[-1]
        if hgncId in wanted:
            coordMap.setdefault(hgncId, []).append(fields)
    return coordMap


def joinAndWrite(g2pData, coords, outputFile):
    """Join G2P records and HGNC coordinates into BED 9+20 and write to outputFile.

    Returns a stats dict, both counts in G2P records so they are comparable:
      "unmatched"         -> count of G2P records whose HGNC ID had no coordinate
                             match in this assembly's HGNC track (they are skipped).
      "unknownConfidence" -> {normalized confidence value: (count of records, one
                             example of the value as it appeared in the CSV)} for
                             values not in CONFIDENCE_COLORS (colored black).
    """
    unmatched = 0
    unknownConfidence = {}
    with open(outputFile, "w", newline="", encoding="utf-8") as out:
        writer = csv.writer(out, delimiter="\t")
        for hgncId, rows in g2pData.items():
            matches = coords.get(hgncId, [])
            if not matches:
                unmatched += len(rows)
                continue
            for row in rows:
                # Counted once per G2P record, not once per output line: an HGNC ID
                # can carry several coordinate rows, which would inflate the tally.
                rgb = confidenceToColor(row["confidence"])
                if rgb is None:
                    # Tally on the folded value so case and stray whitespace do not split
                    # one unknown value into several, but keep a raw example alongside it:
                    # the folded form is not what is in the CSV, so it is not what someone
                    # reading the log would grep for.
                    key = normalizeConfidence(row["confidence"])
                    count, example = unknownConfidence.get(key, (0, row["confidence"]))
                    unknownConfidence[key] = (count + 1, example)
                    rgb = DEFAULT_COLOR

                # G2P 20 fields
                g2pId       = row["g2p id"]
                geneMim     = row["gene mim"]
                hgncIdVal   = row["hgnc id"]
                prevSymbols = row["previous gene symbols"].replace(";", ",")
                diseaseName = row["disease name"]
                diseaseMim  = row["disease mim"]
                diseaseMondo = row["disease MONDO"]
                allelicReq  = row["allelic requirement"]
                crossMod    = row["cross cutting modifier"]
                confidence  = row["confidence"]
                varConseq   = row["variant consequence"]
                varTypes    = row["variant types"]
                molMech     = row["molecular mechanism"]
                molMechCat  = row["molecular mechanism categorisation"]
                molMechEv   = row["molecular mechanism evidence"]
                phenotypes  = row["phenotypes"].replace(";", ",")
                publications = row["publications"].replace(";", ",")
                panel       = row["panel"]
                comments    = row["comments"]
                dateReview  = row["date of last review"]

                for coord in matches:
                    # BED 9 fields
                    chrom       = coord[0]
                    chromStart  = coord[1]
                    chromEnd    = coord[2]
                    name        = row["gene symbol"]
                    score       = coord[4]
                    strand      = coord[5]
                    thickStart  = coord[6]
                    thickEnd    = coord[7]

                    writer.writerow([
                        chrom, chromStart, chromEnd, name, score, strand, thickStart, thickEnd,
                        rgb, g2pId, geneMim, hgncIdVal, prevSymbols, diseaseName, diseaseMim,
                        diseaseMondo, allelicReq, crossMod, confidence, varConseq, varTypes,
                        molMech, molMechCat, molMechEv, phenotypes, publications, panel,
                        comments, dateReview,
                    ])
    return {"unmatched": unmatched, "unknownConfidence": unknownConfidence}


def itemCount(bb):
    line = bash('bigBedInfo %s | grep "itemCount"' % bb)
    return int(line.rstrip().split("itemCount:")[1].replace(",", "").strip())


def checkItemCount(db, newBb):
    """Abort if the item count moved more than COUNT_TOLERANCE vs the live track."""
    liveBb = GBDB_BB % db
    if not Path(liveBb).exists():
        print("%s: no live bigBed yet, skipping item-count check" % db)
        return
    old = itemCount(liveBb)
    new = itemCount(newBb)
    print("%s item count: live=%d new=%d" % (db, old, new))
    if abs(new - old) > COUNT_TOLERANCE * max(new, old):
        msg = "WARNING: %s item count changed >%.0f%% (live=%d new=%d)" % (
            db, COUNT_TOLERANCE * 100, old, new)
        if args.force:
            print(msg + " (continuing due to --force)")
        else:
            sys.exit(msg + "\nRun ./doG2p.py --force if you approve this change.")


def install(db, newBb):
    """Atomically repoint /gbdb/<db>/g2p/g2p.bb at the freshly built bigBed."""
    liveBb = GBDB_BB % db
    bash("mkdir -p %s" % str(Path(liveBb).parent))
    bash("rm -f %s" % liveBb)
    bash("ln -s %s %s" % (newBb, liveBb))
    print("Installed %s -> %s" % (liveBb, newBb))


def main():
    if not updateNeeded():
        # Silent no-op: nothing new from G2P this run.
        return

    validateColumns(NEW_CSV)

    date = str(datetime.now()).split(" ")[0]
    buildDir = "%s/%s" % (WORKDIR, date)
    bash("mkdir -p %s" % buildDir)
    bash("cp %s %s/AllG2P.csv" % (NEW_CSV, buildDir))

    g2pData = loadG2p(NEW_CSV)
    hgncIds = list(g2pData.keys())
    print("Number of HGNC IDs found: %s" % len(hgncIds))

    coordsByDb = {db: loadCoordinates(db, hgncIds) for db in DBS}
    for db in DBS:
        print("Loaded %s %s HGNC IDs" % (len(coordsByDb[db]), db))

    builtBb = {}
    for db in DBS:
        bedFile = "%s/%s_g2p_all.bed" % (buildDir, db)
        bbFile = "%s/%s_g2p.bb" % (buildDir, db)
        twoBit = "/gbdb/%s/%s.2bit" % (db, db)
        stats = joinAndWrite(g2pData, coordsByDb[db], bedFile)
        print("Wrote %s" % bedFile)
        if stats["unmatched"]:
            print("%s: %d G2P record(s) had no HGNC coordinate match and were skipped"
                  % (db, stats["unmatched"]))
        for conf, (n, example) in sorted(stats["unknownConfidence"].items()):
            print("%s: unrecognized confidence value %r on %d record(s); colored black"
                  % (db, example, n))
        bash("bedToBigBed -type=bed9+20 -tab -sort "
             "-as=%s -sizesIs2Bit -extraIndex=name,g2p_id,gene_mim,hgnc_id %s %s %s"
             % (AS_FILE, bedFile, twoBit, bbFile))
        print("Built %s" % bbFile)
        builtBb[db] = bbFile

    # Safety check before swapping anything live.
    for db in DBS:
        checkItemCount(db, builtBb[db])

    for db in DBS:
        install(db, builtBb[db])

    bash("mv %s %s" % (NEW_CSV, PREV_CSV))
    print("G2P updated %s" % date)


if __name__ == "__main__":
    main()
