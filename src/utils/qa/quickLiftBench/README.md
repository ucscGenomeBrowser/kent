# quickLiftBench

Benchmark suite that compares hgTracks render times for quickLifted tracks
against their non-lifted counterparts. Output TSVs are intended as the raw
numbers behind tables and figures in a quickLift performance paper.

## What it measures

For each benchmark case, two (or more) named variants — typically `native`
and `lifted` — are timed at multiple genomic positions across multiple
iterations. Each request goes to hgTracks with `?measureTiming=1`; the
response is parsed for:

- per-track `loadTime` and `drawTime` from `printTrackTiming()` in
  `hg/hgTracks/hgTracks.c`
- phase timings from `<span class='timing'>` markers (chromAliasSetup, etc.)
- HTTP wall time around the request

Each (variant, position) cell does `warmup` discarded requests followed by
`iterations` recorded requests. Min / median / p90 are reported.

## Three comparison modes (one schema)

- **Mode A — same hub, source vs dest db.** One bigBed referenced by two
  trackDb stanzas: native on the source assembly, quickLift'd on the
  destination. Same data, two render paths.
- **Mode B — track pair on each assembly.** Two distinct trackDb stanzas
  holding equivalent data, one native on assembly A, one quickLift'd on
  assembly B.
- **Mode C — lift on/off, same trackDb.** Side-by-side reference hub with
  two stanzas pointing at the same bigBed file; one with
  `quickLiftUrl`/`quickLiftDb`, one without. (quickLift activation is gated
  on the trackDb setting, with no cart override, so the hub-side toggle is
  the cleanest way to compare.)

## Usage

```
./quickLiftBench.py [--config FILE] [--cases ID,ID]
                    [--server-override NAME]
                    [--iterations N] [--warmup N]
                    [--out DIR] [--verbose]
```

Defaults: read `cases.yaml` next to the script, no server override, all
cases, iterations and warmup from `defaults`, output to
`./results/<timestamp>/`.

Examples:

```
# Run everything against hgwdev with the defaults from cases.yaml:
./quickLiftBench.py

# One case, against the sandbox, 10 iterations:
./quickLiftBench.py --cases example_modeA_bigBed \
                    --server-override sandbox --iterations 10

# Quick smoke test:
./quickLiftBench.py --cases example_modeA_bigBed --iterations 1 --warmup 0 -v
```

## Config schema

```yaml
defaults:
  iterations: 5
  warmup: 1
  timeout: 60
  servers:
    hgwdev: https://hgwdev.gi.ucsc.edu
    sandbox: https://hgwdev-braney.gi.ucsc.edu
    beta:   https://hgwbeta.soe.ucsc.edu
    rr:     https://genome.ucsc.edu

cases:
  - id: case_id
    description: "..."
    positions:
      - {label: sparse, value: chr1:1000000-2000000}
      - {label: dense,  value: chr19:50000000-51000000}
    variants:
      native: {server: hgwdev, db: hg19, hubUrl: ..., track: trackName}
      lifted: {server: hgwdev, db: hg38, hubUrl: ..., track: trackName_qL}
    compare:
      - [native, lifted]
```

Each variant URL is built as:

```
{server}/cgi-bin/hgTracks?db=DB&position=POS
   &hideTracks=1&TRACK=full
   &hubUrl=...
   &hgt.trackImgOnly=1&hgt.reset=1&measureTiming=1
```

`hideTracks=1` plus the named track at `=full` isolates the single track.
`hgt.reset=1` resets cart state per request, so cases do not contaminate each
other. A fresh `requests.Session()` is also used per case to mint a new
hgsid.

## Adding a case

1. Pick a track that exists both as a native annotation (or on its source
   assembly) and as a quickLift'd target. For Mode C, build (or point to) a
   side-by-side hub.
2. Pick at least two positions: one sparse (low item count after lift) and
   one dense. Position labels show up in `summary.tsv`.
3. Add a stanza to `cases.yaml` following the schema above. List variant
   pairs to compare under `compare`.
4. Smoke-test with `--cases <new_id> --iterations 1 --warmup 0 -v` to verify
   the URL renders and the per-track timing parses out.

## Output

Two TSVs are written to `results/<YYYYMMDD-HHMMSS>/`:

- `results.tsv` — one row per (case, variant, position, iteration) with
  http_ms, load_ms, draw_ms, total_ms, status_code, error.
- `summary.tsv` — two sections:
  1. per (case, position, variant): n, n_ok, http/load/draw/total median
     and p90.
  2. per (case, position, compare-pair): left vs right medians and the
     `right/left` ratio for each metric.

A short pairwise table is also printed to stderr at the end of a run.

## Dependencies

```
pip install requests pyyaml
```

## Notes

- The script does not parallelize requests against a single server.
  quickLift renders are single-threaded per request; parallel requests would
  measure contention rather than work.
- If hgTracks returns the bot-block page or an `errAbort`, the row is
  written with `error` set and `*_ms` empty rather than aborting the run.
- Timing is wall time inside hgTracks for `load_ms` / `draw_ms`. HTTP wall
  time also includes network and CGI startup; treat it as a sanity check,
  not as the headline number.
- For paper-quality numbers, run repeatedly across hours of the day or
  pin to a quiet host; render times on a shared dev server have noticeable
  load-dependent jitter.
