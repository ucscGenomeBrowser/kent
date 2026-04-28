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

Each variant value is a saved-session reference of the form
`user/sessionName` (the same form as the `/s/<user>/<name>` short-link URL).
Both `User/Name` and the prefix `/s/User/Name` are accepted.

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

1. Save two sessions on the target server that differ only in the dimension
   you want to measure (typically: native vs. quickLifted versions of the
   same set of tracks). Each session should be saved at the position you
   want it benchmarked at.
2. Add a stanza to `cases.yaml` following the schema above.
3. Smoke-test with `--cases <new_id> --iterations 1 --warmup 0 -v` to verify
   sessions load and timings parse out.

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
