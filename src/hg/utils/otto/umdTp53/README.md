# umdTp53 — UMD TP53 Variant Database track

Builds the UMD TP53 variant track on hg19 and hg38 from the UMD p53.fr database
(https://p53.fr/download-the-database).

## Source files

- `UMD_variants_US.tsv` — one row per unique TP53 variant; row source for this
  track. Ships with hg19/hg18 coordinates only.
- `UMD_mutations_US.tsv` — one row per patient/sample. Used solely as the
  source of hg38 coordinates (joined to the variants file on `cDNA_variant`),
  since the variants file ships hg19/hg18 only.

Files use classic-Mac CR-only line terminators; the parser normalizes them.

## Layout

| Path | Purpose |
|---|---|
| `src/hg/utils/otto/umdTp53/` (this dir) | Git-tracked code |
| `/hive/data/outside/otto/umdTp53/` | Working data, logs, release artifacts |
| `/gbdb/{hg19,hg38}/bbi/umdTp53/` | Published bigBeds + trix index |

## Build flow

1. `doUmdTp53Update.sh` — cron entry point. Silent on no-op.
2. `download.sh` — HEAD the two zip URLs, compare `Last-Modified` and unzipped
   MD5 against stored stamps; download into a dated workdir only on real
   content change.
3. `umdTp53ToBed.py` — parse variants TSV, join hg38 coords from mutations
   TSV, normalize pathogenicity / variant classification, build `_mouseOver`
   HTML, emit `umdTp53.hg19.bed` and `umdTp53.hg38.bed`.
4. `checkAndLoad.sh` — sort, `bedToBigBed`, `ixIxx` for search; row-count
   guard (±10%); install symlinks into `/gbdb`.

## Coloring

Per-feature RGB encodes the UMD `Pathogenicity` column:

| Class | RGB |
|---|---|
| Pathogenic | 212,42,42 |
| Likely Pathogenic | 230,90,50 |
| Possibly pathogenic | 247,148,29 |
| VUS | 230,205,0 |
| Benign | 40,140,80 |
| Unknown (`#VALUE!`, `#N/A`, empty) | 160,160,160 |

## Install

```
cd src/hg/utils/otto/umdTp53
make install
```

Copies the scripts and `.as` to `/hive/data/outside/otto/umdTp53/`. The
working-directory copies are what cron executes; the kent source is canonical.

## Cron

Weekly is plenty — the upstream database has been static since October 2017.
Suggested entry:

```
30 4 * * 1 /hive/data/outside/otto/umdTp53/doUmdTp53Update.sh
```

The script is silent unless something actually fails or a real upstream
update is detected, so cron will not email on idle runs.

## Citation

Leroy B, Anderson M, Soussi T. *TP53 Mutations in Human Cancer: Database
Reassessment and Prospects for the Next Decade.* Hum Mutat 35: 672-688 (2014).

Tracked under Redmine #37648.
