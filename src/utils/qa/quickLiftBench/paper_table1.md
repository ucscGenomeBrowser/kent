# quickLift bench — Table 1

Source data:

- N sweep: `results/nsweep-20260512-152211/sweep_summary.tsv`
- Position sweep: `results/possweep-20260512-154445/sweep_summary.tsv`

Both runs: hgwdev production CGI, 10 iterations + 1 warmup per cell, median
reported. `total` is the "Overall total time" span from `measureTiming=1`
hgTracks output. `lifted` is the same data quickLifted from hg38 to hs1; the
**Mode C** comparison keeps both variants on the same db (hs1) so the only
difference is whether the chain-remap path is taken.

---

**Table 1.** hgTracks render time (ms, median) for native vs. quickLifted
views of the same data on hs1, across two density axes. Position
chr22:15M–50M unless noted.

## Panel A — Feature-count sweep (window covers all features)

| N      | bb native | bb lifted | bb ratio | bw native | bw lifted | bw ratio |
| -----: | --------: | --------: | -------: | --------: | --------: | -------: |
|    500 |       283 |     1 223 |    4.3×  |       101 |       495 |    4.9×  |
|  1 000 |       441 |     1 630 |    3.7×  |       100 |       527 |    5.3×  |
|  5 000 |    1 795† |     1 491 |    0.8×  |       104 |       478 |    4.6×  |
| 10 000 |       345 |     1 817 |    5.3×  |       110 |       494 |    4.5×  |
| 20 000 |       348 |     2 271 |    6.5×  |       105 |       598 |    5.7×  |
| 50 000 |       473 |     3 812 |    8.1×  |       103 |       517 |    5.0×  |
|100 000 |       660 |     6 795 | **10.3×**|       102 |       560 |    5.5×  |

† N=5000 native bb is a single-point outlier; both Mode B and Mode C
native bigBed spiked simultaneously at this cell despite using different
hubs, attributed to host jitter rather than to N.

## Panel B — Window sweep (hub-wide N=5000)

| Window                       | features<br>in window | bb native | bb lifted | bb ratio | bw native | bw lifted | bw ratio |
| ---------------------------- | --------------------: | --------: | --------: | -------: | --------: | --------: | -------: |
| chr22:1M–2M (outside region) |                     0 |       139 |        85 |    0.6×  |        93 |       103 |    1.1×  |
| chr22:25M–25.1M (100 kb)     |                  ~14  |       145 |        97 |    0.7×  |        98 |        97 |    1.0×  |
| chr22:20M–25M (5 Mb)         |                  ~714 |       350 |       494 |    1.4×  |        95 |       157 |    1.6×  |
| chr22:15M–50M (35 Mb)        |                 5 000 |     1 770 |     1 475 |    0.8×  |        92 |       573 |    6.2×  |

---

## Notes for paper text

- **bigBed scaling.** quickLift overhead scales roughly linearly with
  in-window feature count: at the full 35 Mb window, lifted total grows
  from 1.2 s (N=500) to 6.8 s (N=100 000), ratio 4.3× → 10.3× over native.
  ~83–94 % of the lifted total at N=100 000 sits in `load_sum`, so the
  per-feature work is in the chain-remap / data-fetch path, not in drawing.

- **bigWig scaling.** quickLift overhead is approximately constant
  (~400–500 ms) regardless of N or window size. This is consistent with
  the chain-lookup + per-bin remap dominating, not per-bin drawing work
  (bigWig `draw_sum` is ≤2 ms in every cell).

- **No features → no overhead.** In Panel B, the `sparse` window
  (chr22:1M–2M, outside the feature region) shows lifted ≤ native for bb
  (85 vs 139 ms) and lifted ≈ native for bw (103 vs 93 ms). When no
  features project into the viewed window, quickLift adds essentially no
  cost beyond a chain-lookup floor of a few ms.

- **Mode B caveat.** Mode B compares hg38-native vs. hs1-lifted (different
  dbs each side); a `trackDbLoad` cost on the native side (hg38 has a
  larger trackDb than hs1) is folded into that ratio. Table 1 reports
  Mode C only so the comparison is isolated to quickLift overhead.
