# MaveMD otto job (hg38)

Weekly cron that keeps the hg38 MaveMD track in step with the MaveDB API. Redmine #37800.

MaveMD ("MAVEs for MeDicine") is not a separate database. It is a curated collection
inside MaveDB holding the score sets that carry clinical calibrations, so everything
comes from the ordinary MaveDB API, reading one pinned collection URN rather than
scanning the catalogue.

    mavemdOtto.py      entry point: lock, fetch, decide, build, check, release
    mavemdFetch.py     the cache; downloads only score sets whose fingerprint changed
    mavemdBuild.py     runs the two converters and makes the bigBeds
    mavemdLib.py       calibration choice and the colour tables
    makeMaveMdVariants.py   per-variant bed12+35 -> mavemdVar.bb
    makeMaveMdHeatmap.py    per-score-set bed12+20 -> mavemdMap.bb
    mavemdVariants.as, mavemdHeatmap.as
    tests/             unit tests, run by `make test`

Placement is not here. It is `mavedbPlace.py` in the sibling directory `../mavedb`,
shared with the MaveDB track (#38396) so that a variant lands at the same base in both.
`mavemdLib.py` puts that directory on `sys.path`, and the same relative hop works in the
kent tree and in the installed copies. **Installing the MaveDB job therefore also changes
the placement code this job runs**, so a `mavedbPlace.py` change wants a MaveMD rebuild
check before `make install` in `mavedb/`.

## What makes a release

A release key is hashed over everything a build reads:

* the fingerprint of every cached score set, which carries the score set's and the
  experiment's modification dates, the variant count, and **the list of CSV namespaces
  the score set offers**. That last one matters because a new ClinVar release arrives as
  a new namespace on a record whose modification date never moves, and we always request
  the newest one.
* the hg38 annotation tables the placement reads: the update times of `ncbiRefSeq*` and
  the name of the newest `wgEncodeGencodeAttrsV*` table. A RefSeq push moves variants and
  nothing on MaveDB's side can show that. If MySQL cannot be reached the run logs it and
  carries on without them, because a late rebuild is better than a missed week.

When the key matches the last release and no full rebuild is due, the run exits 0 with no
output, so cron sends no mail.

A full refetch and rebuild is forced on the first run of January, April, July and
October, or 90 days after the last one. That covers the one case the key cannot see,
MaveDB remapping a score set without touching its record.

Our own code is deliberately **not** in the key. Installing new code is a human act, so
pass `--force` on the run after `make install` rather than having a comment edit churn
the `/gbdb` links.

## Checks before a release

A build that fails any of these is left in `releases/<date>.tmp/`, the `/gbdb` links are
not touched, and the run exits 1:

* an empty bigBed, a dropped protein accession, or a heatmap with overlapping blocks
* more than 10% fewer variant items or heatmap cells, or 5% fewer heatmaps, than the
  previous release
* fewer than 60% of placed items using MaveDB's own mapping. This one is here because the
  cross-check cannot see a mass withdrawal of mappings: it only looks at rows carrying
  both coordinate systems, so fewer mapped rows means fewer checks, not more failures.
  Their staging API looked exactly like this in October 2026.

After a person has looked at the build,
`mavemdOtto.py --release <name> --ignoreChecks` promotes exactly that build without
fetching or building again.

## The trackDb filter menus

`filterValues` in `makeDb/trackDb/human/hg38/mavemd.ra` is written by hand, and trackDb
lives in git, so otto cannot update it. Instead the build compares the values the data
now has against the committed stanza and reports anything missing in the cron mail, for a
person to paste in. `make install` copies the stanza next to the scripts so the check
still works from the live directory.

Note that a value with a comma in it is escaped by doubling the comma
(`slNameListFromCommaEscaped`, `lib/common.c`), which the ClinVar term
"Pathogenic/Likely pathogenic/Pathogenic, low penetrance" relies on. Anything that splits
the menu on plain commas will report drift on every run.

## Running it

    make test                       # the unit tests
    make install                    # tests, byte-compile, copy to /hive/data/outside/otto/mavemd
    make validate                   # the installed copy still matches git

    mavemdOtto.py -n                                  # dry run, writes nothing
    mavemdOtto.py --outDir <dir> --force              # development run, links under <dir>/gbdb
    mavemdOtto.py --refetch                           # download everything again
    mavemdOtto.py --release <name> --ignoreChecks     # promote a build that failed a check

The script refuses to run from anywhere but the live directory unless `--outDir` is
given, so the tree copy cannot write to the live area by accident.

Only these `/gbdb/hg38/mavemd/` names are written: `mavemdVar.bb`, `mavemdMap.bb`,
`version.txt`. The track is gated to alpha (`include mavemd.ra alpha`), so for now this
feeds dev only.

Description pages are not regenerated by otto, so they carry no counts or dates. The
build date reaches the browser through `version.txt` and the `dataVersion` setting.
