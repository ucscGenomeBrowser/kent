# quickLiftBench

Benchmark suite that compares hgTracks render times for two saved sessions on
the same server. The intended pairing is a **native** session (tracks rendered
on their source assembly) against a **lifted** session (the same tracks
rendered on a different assembly via quickLift). Output TSVs are intended as
the raw numbers behind tables and figures in a quickLift performance paper.

## What it measures

For each benchmark case, two (or more) named variants — typically `native`
and `lifted` — are timed across multiple iterations. Each request loads a
saved session into a fresh cart and asks hgTracks for the per-request timing
breakdown. The session renders at the position it was saved with; the runner
does **not** override `position`, since native and quickLifted variants live
on different assemblies and the same chr:start-end is not biologically
equivalent across them. To benchmark multiple regions, save additional
session pairs and add them as separate cases.

Each response is parsed for:

- **Overall total time** — the headline number, taken from the
  `<span class='timing'>Overall total time: NNN millis</span>` footer span.
- **Per-track load and draw times** — summed across all visible tracks from
  the `printTrackTiming()` table emitted into a `<span class='trackTiming'>`
  block.
- **HTTP wall time** — measured around the request itself.

Each variant cell does `warmup` discarded requests followed by `iterations`
recorded requests. Min / median / p90 are reported.

## Usage

```
./quickLiftBench.py [--config FILE] [--cases ID,ID]
                    [--server-override NAME]
                    [--iterations N] [--warmup N]
                    [--out DIR] [--verbose] [--phases]
```

Defaults: read `cases.yaml` next to the script, no server override, all
cases, iterations and warmup from `defaults`, output to
`./results/<timestamp>/`.

Examples:

```
# Run everything against the server in each case stanza:
./quickLiftBench.py

# One case, against the sandbox, 10 iterations:
./quickLiftBench.py --cases bench1_hgwdev \
                    --server-override sandbox --iterations 10

# Quick smoke against a single existing saved session:
./quickLiftBench.py --cases smoke_session --iterations 1 --warmup 0 -v
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
    server: hgwdev          # one server for all variants in this case
    variants:
      native: User/sessionName_native     # user/sessionName
      lifted: User/sessionName_lifted
    compare:
      - [native, lifted]
```

Each variant value is one of:

- **Saved-session reference** (string `user/sessionName`, or the equivalent
  `/s/<user>/<name>` short-link form). The session's saved position and
  cart are used. Best for Mode A and Mode B cases where the native and
  lifted variants sit on different assemblies and "the same position" is
  not biologically meaningful.

- **Hub variant** (mapping with `hubUrl`, `db`, `position`, `tracks`).
  Attaches a hub at an explicit db and position, then turns each track
  on/off according to `tracks`. Used for Mode C where both variants live
  on the same assembly and differ only in track visibility. Example:

  ```yaml
  variants:
    native:
      hubUrl: https://example.org/myHub/hub.txt
      db: hs1
      position: chr22:15000000-50000000
      tracks: {modeC_native: pack, modeC_lifted: hide}
    lifted:
      hubUrl: https://example.org/myHub/hub.txt
      db: hs1
      position: chr22:15000000-50000000
      tracks: {modeC_native: hide, modeC_lifted: pack}
  ```

  The runner sends `hideTracks=1` plus the per-track vis settings so only
  the explicitly named tracks render.

The URL the runner sends per iteration is:

```
{server}/cgi-bin/hgTracks?
   hgS_doOtherUser=submit
   &hgS_otherUserName=USER
   &hgS_otherUserSessionName=NAME
   &hgt.trackImgOnly=1
   &measureTiming=1
```

Notes on URL choices:

- `hgS_doOtherUser=submit` plus the user/session name causes hgTracks to
  load the saved session into the cart (`cart.c:1715`). The session's saved
  position is used.
- `hgt.trackImgOnly=1` is the JS-redraw fast path: hgTracks emits the image
  + map and returns without rendering the rest of the page. With
  `measureTiming=1` it also emits the per-track timing block.
- A fresh `requests.Session()` per case mints a new hgsid (and thus a fresh
  cart) so cases do not contaminate each other.

## Adding a case

**Saved-session variants** (cross-assembly Mode A / Mode B):

1. Save two sessions on the target server that differ only in the dimension
   you want to measure (typically: native vs. quickLifted versions of the
   same set of tracks). Each session should be saved at the position you
   want it benchmarked at.
2. Add a stanza to `cases.yaml` using string variants of the form
   `user/sessionName`.

**Hub variants** (Mode C, same assembly + same position):

1. Build (or pick) a hub where two trackDb stanzas reference the same
   conceptual data, one with `quickLiftUrl` and one without. The included
   `testHub/buildTestHub.sh` is a working example: it generates 5000
   synthetic BED12 features on hg38, lifts them to hs1, copies the
   hg38→hs1 quickLift chain in alongside, and writes a 2-stanza hub.txt.
2. Add a stanza to `cases.yaml` using mapping variants (see schema above).

Either way, smoke-test with `--cases <new_id> --iterations 1 --warmup 0 -v`
to verify the URL works and timings parse out.

## Density sweep (`nSweep.py`)

`nSweep.py` rebuilds `testHub/` at a sequence of (N, BW_STEP) sizes and runs
the bench at each one, tagging every row with `N` and `bw_step`. The merged
`sweep.tsv` is the raw data behind the paper's "quickLift overhead vs.
density" curve.

```
./nSweep.py [--n-values 500,1000,5000,10000,20000]
            [--bw-step-values 1000]
            [--cases mode_b_bb,mode_b_bw,mode_c_hs1_bb,mode_c_hs1_bw]
            [--iterations 10] [--warmup 1]
            [--region-start 15000000] [--region-end 50000000]
            [--feature-w AUTO]
            [--hub-dest-base ~/public_html/quickLiftBench/sweep]
            [--out DIR] [--clean-builds] [--skip-existing] [--dry-run]
```

Per (N, BW_STEP) point the script:

1. Auto-picks `FEATURE_W` so N features fit in the region without overlap
   (clamped to [50, 5000]; override with `--feature-w`). Runs
   `testHub/buildTestHub.sh` with `N`, `BW_STEP`, `FEATURE_W`, and the
   region bounds into `<hub-dest-base>/N{N}_S{S}_W{W}/`.
2. Loads `cases.yaml`, filters to `--cases`, rewrites each hub variant's
   `hubUrl` from `.../testHub/...` to `.../sweep/N{N}_S{S}_W{W}/...`, and
   drops the rewritten config into the output dir.
3. Invokes `quickLiftBench.py` with that config; per-point outputs land in
   `<out>/N{N}_S{S}_W{W}/`.
4. Appends `results.tsv` to a single `sweep.tsv` with `N` and `bw_step`
   prepended to each row.

After all points run, `sweep_summary.tsv` has two sections:

- Per (N, bw_step, case, variant): n_ok, total/load/draw median + p90.
- Per (N, bw_step, case): native vs. lifted total medians and the
  `lifted/native` ratio — the headline curve.

`--skip-existing` reuses hub dirs that already contain `hub_hs1.txt`, which
is handy when iterating on bench config without rebuilding identical hubs.

## Position sweep (`posSweep.py`)

`posSweep.py` keeps the hub fixed and varies the *viewed window*, so each
row measures quickLift overhead at a specific in-window feature density.
At the default testHub (N=5000 features uniformly distributed across
chr22:15M-50M at 7kb stride), the built-in canonical positions exercise:

| name | position | in-window features at N=5000 |
| --- | --- | --- |
| `sparse` | chr22:1M-2M | 0 (1Mb outside feature region) |
| `narrow_dense` | chr22:25M-25.1M | ~14 (100kb inside region) |
| `medium` | chr22:20M-25M | ~714 (5Mb inside region) |
| `wide` | chr22:15M-50M | ~5000 (full 35Mb region) |

```
./posSweep.py [--positions name1:chr:start-end,name2:chr:start-end,...]
              [--cases mode_b_bb,mode_b_bw,mode_c_hs1_bb,mode_c_hs1_bw]
              [--config cases.yaml]
              [--iterations 10] [--warmup 1]
              [--out DIR] [--dry-run]
```

Per position the script:

1. Loads `cases.yaml`, filters to `--cases`, and rewrites every hub
   variant's `position` field with the swept position. Saved-session
   variants are left as-is (the saved session's position can't be
   overridden) and a warning is printed.
2. Invokes `quickLiftBench.py`; per-point outputs land in
   `<out>/<position_name>/`.
3. Appends `results.tsv` to a single `sweep.tsv` with `position_name` and
   `position` prepended.

`sweep_summary.tsv` mirrors the N sweep's two-section format: per
(position_name, case, variant) medians/p90, then per (position_name, case)
`lifted/native` ratio.

## Output

Two TSVs are written to `results/<YYYYMMDD-HHMMSS>/`:

- `results.tsv` — one row per (case, variant, iteration) with
  http_ms, load_ms_sum, draw_ms_sum, n_tracks, total_ms, status_code, error.
- `summary.tsv` — two sections:
  1. per (case, variant): n, n_ok, http/load_sum/draw_sum/total median and p90.
  2. per (case, compare-pair): left vs right total medians and the
     `right/left` ratio for each metric.
- `phases.tsv` (only with `--phases`) — long-form rows of every
  `<span class='timing'>label: NNN millis</span>` marker emitted by
  hgTracks (chromAliasSetup, trackDbLoad, parallel data fetch, image
  generation, cart write, etc.), one row per (case, variant, iteration,
  phase). A per-(case, variant, phase) median+p90 summary is appended.
  Useful for localizing where time is going when total medians differ.

## Regression assertions: `phase_asserts`

A case can declare assertions against the per-iteration phase timings, so
the bench acts as a tripwire for regressions instead of just emitting
numbers. When any case declares `phase_asserts`, that case's phase data is
captured automatically (no `--phases` flag needed) and assertions run after
all iterations complete. A failure prints to stderr and the script exits
non-zero.

```yaml
- id: regress_my_thing
  server: hgwdev
  variants:
    base: User/sessionName
  phase_asserts:
    - variant: base
      phase: 'Waiting for parallel \(\d+ threads for \d+ tracks\) remote data fetch'
      required: true        # span must appear in every iteration
      max_median_ms: 15000  # optional median upper bound
      min_median_ms: 1      # optional median lower bound
```

Semantics:

- `phase` is a Python regex matched against each phase label (the part
  before `:` in `<span class='timing'>label: NNN millis</span>`).
- `required: true` (default) — assert fails if the regex matches no phase
  in any iteration of the variant.
- `max_median_ms` / `min_median_ms` — optional bounds on the median across
  iterations. Per iteration, all matching phases' ms values are summed,
  then the per-iteration sums are reduced via median.
- A FAIL prints `[FAIL] case/variant /pattern/ reason` and `sys.exit(1)`.

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
- `total_ms` is the wall time inside hgTracks for the full request (cart
  load + track load + track draw + page assembly). `http_ms` adds network
  and CGI startup; treat it as a sanity check, not as the headline number.
- Each request reloads the saved session into a fresh cart, so the
  per-request work includes session unmarshaling. That is consistent
  across variants, so it cancels out in the ratio.
- For paper-quality numbers, run repeatedly across hours of the day or
  pin to a quiet host; render times on a shared dev server have noticeable
  load-dependent jitter.
