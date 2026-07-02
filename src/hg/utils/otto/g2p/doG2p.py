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


def confidence_to_color(confidence):
    """Map a confidence string to an RGB color string for UCSC BED itemRgb."""
    color_map = {
        "definitive": "39,103,73",   # dark green
        "strong": "56,161,105",      # green
        "moderate": "104,211,145",   # light green
        "limited": "252,129,129",    # pink
        "disputed": "229,62,62",     # red
        "refuted": "155,44,44",      # dark red
    }
    return color_map.get(confidence.lower(), "0,0,0")  # default black


def load_g2p(file_path):
    """Load G2P CSV into a dict keyed by HGNC ID (each value is a list of rows)."""
    g2p_map = {}
    numOfRows = 0
    with open(file_path, newline="", encoding="utf-8") as csvfile:
        reader = csv.DictReader(csvfile)
        for row in reader:
            numOfRows += 1
            hgnc_id = row["hgnc id"].strip()
            g2p_map.setdefault(hgnc_id, []).append(row)
    print("Number of rows in file: %s" % numOfRows)
    return g2p_map


def load_coordinates(db, hgnc_ids):
    """Build a dict of gene coordinates for the given HGNC IDs from the HGNC bigBed.

    One bigBedToBed pass over the whole track (~49k rows) instead of one
    bigBedNamedItems subprocess per HGNC ID. The bigBed name field is
    "HGNC:<id>"; the G2P CSV stores the bare numeric id, so we key on that.
    """
    wanted = set(hgnc_ids)
    coord_map = {}
    hgncBB = "/gbdb/%s/hgnc/hgnc.bb" % db
    for line in bash("bigBedToBed %s stdout" % hgncBB).split("\n"):
        if not line.strip():
            continue
        fields = line.split("\t")[:8]
        name = fields[3]                       # e.g. "HGNC:36036"
        id = name.split("HGNC:")[-1]
        if id in wanted:
            coord_map.setdefault(id, []).append(fields)
    return coord_map


def join_and_write(g2p_data, coords, output_file):
    """Join G2P records and HGNC coordinates into BED 9+20 and write to output_file."""
    with open(output_file, "w", newline="", encoding="utf-8") as out:
        writer = csv.writer(out, delimiter="\t")
        for hgnc_id, rows in g2p_data.items():
            for row in rows:
                for coord in coords.get(hgnc_id, []):
                    # BED 9 fields
                    chrom       = coord[0]
                    chromStart  = coord[1]
                    chromEnd    = coord[2]
                    name        = row["gene symbol"]
                    score       = coord[4]
                    strand      = coord[5]
                    thickStart  = coord[6]
                    thickEnd    = coord[7]
                    rgb         = confidence_to_color(row["confidence"])

                    # G2P 20 fields
                    g2p_id      = row["g2p id"]
                    gene_mim    = row["gene mim"]
                    hgnc_id_val = row["hgnc id"]
                    prev_symbols = row["previous gene symbols"].replace(";", ",")
                    disease_name = row["disease name"]
                    disease_mim = row["disease mim"]
                    disease_MONDO = row["disease MONDO"]
                    allelic_req = row["allelic requirement"]
                    cross_mod   = row["cross cutting modifier"]
                    confidence  = row["confidence"]
                    var_conseq  = row["variant consequence"]
                    var_types   = row["variant types"]
                    mol_mech    = row["molecular mechanism"]
                    mol_mech_cat = row["molecular mechanism categorisation"]
                    mol_mech_ev = row["molecular mechanism evidence"]
                    phenotypes  = row["phenotypes"].replace(";", ",")
                    publications = row["publications"].replace(";", ",")
                    panel       = row["panel"]
                    comments    = row["comments"]
                    date_review = row["date of last review"]

                    writer.writerow([
                        chrom, chromStart, chromEnd, name, score, strand, thickStart, thickEnd,
                        rgb, g2p_id, gene_mim, hgnc_id_val, prev_symbols, disease_name, disease_mim,
                        disease_MONDO, allelic_req, cross_mod, confidence, var_conseq, var_types,
                        mol_mech, mol_mech_cat, mol_mech_ev, phenotypes, publications, panel,
                        comments, date_review,
                    ])


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

    g2p_data = load_g2p(NEW_CSV)
    hgnc_ids = list(g2p_data.keys())
    print("Number of HGNC IDs found: %s" % len(hgnc_ids))

    coordsByDb = {db: load_coordinates(db, hgnc_ids) for db in DBS}
    for db in DBS:
        print("Loaded %s %s HGNC IDs" % (len(coordsByDb[db]), db))

    builtBb = {}
    for db in DBS:
        bedFile = "%s/%s_g2p_all.bed" % (buildDir, db)
        bbFile = "%s/%s_g2p.bb" % (buildDir, db)
        twoBit = "/gbdb/%s/%s.2bit" % (db, db)
        join_and_write(g2p_data, coordsByDb[db], bedFile)
        print("Wrote %s" % bedFile)
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
