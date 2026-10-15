# MaveDB otto job (hg38)

Rebuilds the hg38 MaveDB track from the live MaveDB API (api.mavedb.org): score set
heatmaps, a per-variant track and the target sequence alignments. Redmine #38396.
Status: in development, not installed, no cron entry, no AutoPush.

The cron entry point is `mavedbOtto.py`: update the cache, and if the score sets changed (or
a quarterly full rebuild is due) build `releases/YYYY-MM-DD/`, check it, and move the
`/gbdb/hg38/maveDB/` links (new file names only) to it. Silent when nothing changed.

## Files

| file | role |
|---|---|
| `mavedbOtto.py` | Cron entry point: fetch, decide, build a release, release checks, `/gbdb` relink, summary mail |
| `mavedbFetch.py` | Catalogue walk, human filter, fingerprints, paced download of score set records and variant CSVs into a cache |
| `mavedbPlace.py` | Placement rules (offsets, residue and genome checks) and the coordinate code shared with MaveMD (`makeDb/scripts/mavemd`) |
| `mavedbMaps.py` | Heatmap items (`style heatmap` bigBed, Jonathan Casper's 2025 layout) |
| `mavedbVariants.py` | One item per placed variant |
| `mavedbAlign.py` | Target sequence alignments (bigPsl), anchored at the placed variants' locus |
| `mavedbBuild.py` | Places every cached score set and writes `mavedbMaps.bb`, `mavedbVariants.bb`, `mavedbAlignDna.bb`, `mavedbAlignAa.bb`, `build.tsv`, `report.txt` |
| `mavedbMaps.as`, `mavedbVariants.as` | autoSql |
| `tests/testMavedbPlace.py` | Unit tests of the placement rules on synthetic proteins (no database) |
| `tests/testMavedbFetch.py` | Unit tests of the catalogue filter and fingerprints (no network) |
| `tests/testMavedbOtto.py` | Unit tests of the release decisions and checks |
| `makefile` | `make test`; `make install` copies the scripts, `.as` files and `bigPsl.as` to the live directory |

Still to come: `mavedbAutoPush` draft, cron entry.

## Otto run

    mavedbOtto.py                 # cron: silent unless a release was made or it failed
    mavedbOtto.py -n              # what would happen (downloads the catalogue only)
    mavedbOtto.py --force         # build a release even if nothing changed
    mavedbOtto.py --refetch       # download every variant file again
    mavedbOtto.py --force --ignoreChecks   # release a build that failed its checks, once checked
    mavedbOtto.py --outDir dir    # development run; links go to dir/gbdb

A release is stopped, with the /gbdb links unchanged and the build left in
`releases/<date>.tmp/`, when a bigBed is empty, a heatmap item failed the field check, or
heatmap or variant items drop more than 10% (shown score sets more than 5%) from the last
release. Cache problems (catalogue count mismatch, more than 5% of downloads failing, five
429s in a row) and the genome cross-check stop the run before the build. Each release keeps
`report.txt`, `build.tsv`, `catalogue.tsv`, `stats.json`, `build.log` and `version.txt`;
`lastRelease.txt` names the live one.

## Running a development build

    mavedbFetch.py cacheDir
    mavedbBuild.py cacheDir/catalogue.json cacheDir/variants outDir --scoreSets cacheDir/scoreSets

`mavedbFetch.py` writes the human score sets' search records (`catalogue.json`), their
records (`scoreSets/`) and variant CSVs (`variants/`) into the cache, fetching only score
sets whose fingerprint changed (`--refetch` for all, `--adopt` to seed from an earlier
download). Run it with the cron python (`/usr/bin/python3`, 3.9) and never
inside the live otto directory.

    make test

## Placement in one paragraph

One offset per score set target, reference position = experiment position + offset, tested
against the wild-type residues the experiment states. MaveDB's own offset is used if it fits
exactly or with one differing position (a construct carrying a known change); otherwise our
search over the gene's curated RefSeq proteins finds the one offset that fits; a run of
added residues at one end of the construct (a tag) is tolerated when at least 10 other
positions match and outnumber it. Every placed variant must match the reference protein and
the genome codon. Nothing is aligned. The reasons for each rule are in the docstrings of
`mavedbPlace.py` and in Redmine #38396.
