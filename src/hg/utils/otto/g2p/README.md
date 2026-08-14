# G2P (Gene2Phenotype) otto track

Automatic monthly update of the `g2p` track on hg19 and hg38.

Track type: bigBed 9+20. Served from `/gbdb/<db>/g2p/g2p.bb`, which is a
symlink to the most recent dated build under this directory.

## Layout

- `doG2p.py`        — the worker. Downloads, validates, builds, guards, installs.
- `g2pWrapper.sh`   — cron wrapper; runs the worker and mails output to otto-group.
- `g2p.as`          — autoSql for the bed9+20 output.
- `expectedColumns.txt` — required CSV columns; build aborts if any go missing.
- `prevAllG2P.csv`  — last downloaded CSV, used for change detection (created at runtime).
- `YYYY-MM-DD/`     — per-build working dir + archive (CSV, bed, bb).

Source of truth is the kent tree (`~/kent/src/hg/utils/otto/g2p/`); the hive
copy must match it or `ottoCompareGitVsHiveFiles.py` will email otto-group.

## Data source

Full panel CSV: https://www.ebi.ac.uk/gene2phenotype/api/panel/all/download
Gene coordinates come from the HGNC bigBed (`/gbdb/<db>/hgnc/hgnc.bb`), joined
on HGNC ID.

## Behavior

1. Downloads the CSV. If byte-identical to `prevAllG2P.csv`, exits silently
   (no email).
2. Verifies all required columns are present; aborts loudly otherwise.
3. Builds a dated bigBed for each db.
4. Aborts if item count moved >10% vs the live track. Re-run with `--force`
   to approve: `./doG2p.py --force`.
5. Repoints `/gbdb/<db>/g2p/g2p.bb` at the new build.

## Manual run

    cd /hive/data/outside/otto/g2p
    ./doG2p.py            # normal run
    ./doG2p.py --force    # rebuild + bypass the item-count guard
