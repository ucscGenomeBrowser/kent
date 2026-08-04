# Docent — a language for authoring guided tours of the Genome Browser

A **docent** leads a tour and explains what you are looking at. A Docent script is
that tour, written down: an ordered list of high-level verbs (`go`, `hide`,
`track`, `mouseover`, `convert`, `drag`, `shot`) that drive a real browser against a
real server.

`docent.js` renders one script into two things at once:

- a **silent mp4** of the whole tour, and
- a named **PNG still** at every `shot:` marker.

So a published figure is literally a frame of the tour, and the two can never drift
apart. The surface syntax is YAML — so ordinary editors highlight it and no one has to
learn a new parser — but the language is the verb vocabulary layered on top, not the
serialization. Scripts are named `<base>.docent.yaml` (a bare `<base>.docent` works too).

The verbs deliberately encode *browser mechanics* rather than selectors: `convert:`
knows that the Hide-defaults checkbox reverts when the Assembly menu reloads,
`track: {clinvar: pack}` asks trackDb which containers and checkboxes that implies, and
`mouseover:` knows that a lifted track's DOM id gains a per-run `hub_<n>_` prefix. The author writes
intent; the renderer deals with the UI.

## Run

```
PLAYWRIGHT_BROWSERS_PATH=~/pwrec/browsers NODE_PATH=~/pwrec/node_modules \
  node docent.js AP1.docent.yaml
```

Needs `playwright`, `js-yaml`, and `ffmpeg`. At UCSC these live in a shared install at
`~braney/pwrec` (`pwrec/browsers` for Chromium, `pwrec/node_modules` for the modules) —
point the two variables above anywhere you have them.

Outputs, relative to the script's own directory:

- mp4 → `../<base>.mp4` (override with `mp4:` or a second argument)
- stills → `stills/<base>/<name>.png` (override with `stills:`)

`docent.mk` in this directory has the make rules — include it from a project that keeps
a set of scripts and it rebuilds only the ones whose source changed. See the usage
comment at the top of that file.

## Top of file (all optional)

```yaml
target: genome-test     # or rr, hgwdev, hgwbeta, hgwdev-<user>, or a full https://.../cgi-bin
db: hg38                # source assembly
position: chr7:155.8M   # starting position (tracked from then on; `go:` sets it too)
reset: true             # cartReset first (clean cart + fresh quickLift hub)
size: [1000, 760]       # viewport
pix: 850                # browser image width
pace: 1.2               # seconds to dwell after each step
shotHold: 2.2           # extra seconds the video pauses at a shot
trackAnim: false        # skip the visible dropdown gesture on `track:` (nav only)
fast: false             # true = figures only: no dwells, no cursor animation, no mp4
pinMouseovers: false    # record every mouseover for `pinShot:` (see below)
mp4: ../myname.mp4      # override output paths if you want
stills: stills/AP1
```

`target:` defaults to genome-test, so a script that forgets to say where it runs will
not quietly hit someone's personal sandbox.

## Steps

`steps:` is an ordered list. Each item is a bare verb or a one-key map.

| Step | What it does |
|------|--------------|
| `gateway` | Open hgGateway for `db`. |
| `go` | Click **GO** with the default position (no typing). |
| `go: chr7:155799529-155812871` | Go straight to a position in hgTracks. |
| `goShow: BRCA1` | **Demonstrates the position change through the UI** (vs. `go:` which navs there directly): the cursor glides to the **position box** and the term is **typed on screen**, then the page finishes the job — **Search** on hgTracks, the arrow on hgGateway — so one verb covers either page. Takes a **gene name** or a **position** (or anything else the box accepts: HGVS, an accession). Bare string, or map form `goShow: {gene: SHH, shot: source}` / `goShow: {position: "chr7:155,799,529-155,812,871", shot: source}` — quote coords in the map form (unquoted commas split a `{..}` flow map). |
| `goShow: {gene: TP53, pick: "NR_176326"}` | A **gene name** goes through the browser's own **suggestion menu**, the way a user does it: Docent waits out the hgSuggest ajax, then clicks the matching row, so you land on the gene (not the search-results page) and the video shows the dropdown. Default match is the exact gene symbol; `pick:` selects a specific row by a substring of its text when the term is ambiguous (a `pick:` that matches nothing warns and falls back to the plain gene match). If nothing ever matches, the typed term is submitted as-is — that may land on the **search-results page**, which is legal, and a `click:` can take it from there. |
| `hide: all` | Click **Hide all**. |
| `track: {mane: pack}` | Set a track's visibility (`hide/dense/squish/pack/full`). The mouse visibly glides to that track's control dropdown, opens it, and picks the mode (then the state is applied). |
| `track: {dbSnp155Common: pack}` | A subtrack by name: trackDb's view and composite above it come along, so this is the whole "common dbSNP" config. (`dbSnp155` is a *different*, off-by-default subtrack — all variants — not an alias for the composite.) |
| `track: {clinvar: pack, clinvarCnv: hide}` | Composite: the container's mode reaches its subtracks, so name only the deviations. Docent adds the containers above and the `_sel` checkbox from trackDb — see **Track names come from trackDb**. |
| `track: {varsInPubs: hideKids, pubtator: pack}` | **superTrack: show one member only.** `hideKids` is not a visibility — it means *hide everything under this container*, so the child named alongside it is left the only one drawn. A superTrack needs this because, unlike a composite, its own mode does **not** reach its children: each comes up at its own trackDb visibility, so `{varsInPubs: show}` draws all eight of its members and an earlier `hide: all` does not stick. The expansion skips any child the step names itself and is sent in a round of its own **after** the rest — a subtrack hide travelling in the same request as its container can be dropped by the cart (#37953), so the container goes on first and the hides follow. Works on a composite or view too, where it deselects (`_sel=0`) rather than just hiding. `hideKids` alone (no child named) simply empties the container. |
| `track: {anyTrackName: dense}` | Any track by its **trackDb** name. A name trackDb doesn't have (hub, custom track, quickLift target) is sent literally as `name=mode`. |
| `mouseover: {track: dbSnp155Common, item: rs28406051}` | Hover a **named** item to raise its tooltip. `item:` matches the item's map box (its `&i=<name>` HREF or its TITLE) and hovers that rectangle's center — so it lands on the **correct row** even when items are stacked, which a bare coordinate can't do. It then waits for **that item's own tooltip**, not merely for a tooltip: the browser shows one 500 ms after `mouseenter` and hides it 500 ms after `mouseleave` (`hg/js/utils.js`), so the cursor gliding in crosses other items and one of THEIR tooltips can still be on screen on arrival — which is how an Alignment Differences mismatch got pinned as its neighbour's "identical". The expected text comes from the item's own map box, rendered the way the tooltip renders it (the attribute holds markup and undecoded entities), and `DOCENT_ROWS=1` warns if it never appears. `title: "SHH"` matches the TITLE text only; `value: "..."` matches a JSON `mouseOver` span (wig/dense tracks). |
| `mouseover: {track: mane, at: chr7:155805900}` | Hover by **position** when you don't need a specific item: genomic coord (`at:`), a fraction across the view (`frac: 0.5`), or a raw pixel (`x: 400`); the y is forced to the middle of that track's row (cannot disambiguate stacked items). Optional on any `mouseover`: `hold: 2.5` (seconds to dwell) and `shot: tip_mane` (capture the image **plus** the tooltip in one still). |
| `mouseover: {track: dbSnp155Common, item: rs28406051, pin: true}` | `pin: true` **records** that tooltip (its text + position) so a later `pinShot:` can show several mouseovers open together in one figure. Nothing is added to the recorded page, so the **mp4 is unaffected** (it still shows only the transient native tooltip). Set `pinMouseovers: true` at the top of the file to record every mouseover by default (`pin: false` opts one out). Records accumulate within a view and are cleared on the next nav. |
| `pinShot: all_tips` | Write `<name>.png` with **all recorded (pinned) tooltips open at once**. Rendered on a throwaway page that shares the session (same cart/view) — never on the recorded page — so it never appears in the mp4. Consumes the recorded set (clears it). Place it after the `mouseover` steps whose tooltips you want shown together, before any nav/zoom. |
| `convert: {to: GCA_018466845.2, quicklift: true, hideDefaults: true}` | View→Convert, then **type the target into the page's own "Search for target genome" bar** and click the suggestion (`search:` overrides what is typed, `pick:` disambiguates the menu); the Assembly dropdown is checked afterwards and only opened by hand if the search didn't land there. QuickLift on, **re-checks Hide-defaults** (it reverts when the Assembly menu reloads), Submit. `to:` accepts an accession or a label fragment (`2257.pat`, matched against the dropdown text). |
| `convert: {to: ..., shot: convert_filled}` | `shot:` inside `convert:` captures the Convert page, which no other verb can reach. A bare name is the **filled-in page just before Submit**. The map form names up to three moments: `shot: {opened: a, filled: b, result: c}` — `opened` as the page comes up, `filled` ready to Submit, `result` the conversion-result page (whose coordinate link `open: lift` clicks). These are viewport stills, so they show the page from the top. |
| `hub: https://example.org/hub.txt` | **Quick, silent** attach of a track hub by URL (`hgTracks?hubUrl=...`): connects the hub so its tracks are available at their hub-declared visibility. Follow with `track:` to turn specific ones on. Map form `hub: {url: ..., db: hg38, position: chr7:...}` overrides the db/position (default: current `db` + last position). |
| `addHub: https://example.org/hub.txt` | **Demonstrates the attach through the UI** (for the figure/video): opens My Data → Track Hubs, clicks the **Connected Hubs** tab, types the URL into the box, and clicks **Add Hub** — cursor glides and the URL is typed on screen. Then, on the "Hub Connect Successful" page, it **clicks the `Open:` link for `db`** so the demo ends on the browser with the hub loaded. Map form `addHub: {url: ..., db: hg38, shot: loaded}` sets which assembly to open and captures the still on that tracks view. Use `hub:` instead when you just need the hub attached without showing the steps. |
| `addCustomTrack: <text-or-url>` | **Demonstrates loading a custom track via the UI**: opens My Data → Custom Tracks, types the track data (or a data URL) into the paste box, clicks **Submit**, then clicks through to the browser (**Go to first annotation**). Bare string is the data or URL; map form `addCustomTrack: {data: "track ...\nchr7 ...", db: hg38, goto: first, shot: loaded}` (use `url:` for a data URL, `goto: current` to land on **Return to current position** instead). Data is inserted literally (tabs/newlines preserved). In YAML, a multi-line track uses a block scalar: `addCustomTrack: |` then the indented lines. |
| `addPublicHub: GTEx` | **Demonstrates connecting a PUBLIC hub via the UI**: opens My Data → Track Hubs, the **Public Hubs** tab, types the search terms, clicks **Search Public Hubs**, then clicks **Connect** on the matching hub row, and finally **clicks the `Open:` link for `db`** to land on the browser. Bare string is the search term (also used to match the row). A search usually returns several hubs, so use the map form `addPublicHub: {search: "GTEx", match: "GTEx Analysis Hub", db: hg38, shot: loaded}` to pick the exact hub by a substring of its row text (`match:` defaults to `search:`) and the assembly to open. If no row matches it **won't connect** (warns and stops) rather than pick the wrong hub. |
| `drag: chr7:155,805,900-155,806,950` | Emulates the **Shift+drag-select** gesture: the cursor sweeps across the selection (a visible selection box is drawn) and the browser's own drag-select dialog is raised, then a button is clicked. The argument is one genomic region, `chrom:start-end`; a bare range **zooms**. For any other action, or to pass other keys, put the region under `range:` and quote it — unquoted commas split a `{..}` flow map: `drag: {range: "chr7:155,805,900-155,806,950", shot: dragselect, then: highlight}`. Endpoints that are not genomic coordinates are given as a fraction (`fromFrac:`/`toFrac:`) or raw px (`fromX:`/`toX:`) instead. Optional `track:` picks the row the box is drawn over; default is the top of the image. `shot: dragselect` captures the open dialog. `then:` = `zoom` (default → **Zoom In**) \| `highlight` (→ Single Highlight) \| `cancel` (Escape, view unchanged). (A real button-held drag would just pan, so the dialog is driven directly.) |
| `open: lift` | Click the returned coordinate link → the lifted view. |
| `zoom: out` / `zoom: in` | One zoom step (2×). |
| `shot: source` | Write `<name>.png` **and** pause the video here. On a tracks page the still is the track image (`#imgTbl`), plus any open tooltip/dialog. On any other page (an hgc detail page, an external page a link led to) it is the **viewport only — the top of the page**, never the whole scrolling document. |

Escape hatches for anything the verbs don't cover: `goto: <url>`, `click: <sel>`,
`hover: <sel>`, `wait: <sel>`, `sleep: <ms>`.

## Speed

A full run is a real browser against a real server, and most of its wall clock is the
pacing that makes the *video* watchable, not work. Measured on BP1 (15 steps, 4 stills):
**64 s**, of which ~40 s is dwells, cursor animation and dropdown theatrics, ~23 s is page
loads, and ~1 s is the mp4 transcode.

So when you are iterating on the **figures**, skip the video:

    make FAST=1 BP1        # or DOCENT_FAST=1, or `fast: true` in the script

FAST drops the dwells (`pace`, `shotHold`, mouseover holds), moves the cursor in one jump,
skips the dropdown open/highlight, and records no video, so there is no mp4 to transcode
either — **24 s** instead of 64 s for BP1. The stills carry the same content; a couple of
things about them are incidental rather than identical, so build normally for the figures
you publish:

- the drawn cursor rests wherever the last real action left it, which is not where the
  skipped animation would have left it;
- a `pinShot:` crop is the bounding box of the image plus the pinned tooltips, so it can
  come out a few pixels taller or shorter.

`DOCENT_TIME=1` prints where the time actually went, slowest step first — worth a look
before assuming a script is slow for a reason you can fix.

Two more things that help:

- `make -j6` builds scenarios in parallel. Each run has its own browser, context and cart,
  so they don't interfere; the trackDb cache is written via a temp file and renamed, so
  concurrent runs can't read a half-written one.
- The trackDb cache holds the *derived* index (a few MB), not hubApi's reply (~30 MB for
  hg38), so it is quick to re-read on every run. It refreshes daily; delete
  `$TMPDIR/docent-tdb-*` to force it.

## Gotchas

The renderer lints for one YAML trap before it opens a browser: in a flow map a colon
needs a trailing space, so `{item:name5568747}` parses as the single key
`"item:name5568747"` and the intended `item:` argument is silently dropped — the verb
then quietly falls back to a default. Any argument key containing a `:` gets a warning.

Quote genomic coordinates inside a `{...}` flow map. An unquoted comma in
`chr7:155,805,900` splits the map.

`goShow:` with a gene name can't just wait for the suggestion menu to appear and click a
row. The box shows **Recent** positions immediately, so an early click picks a
previously-visited position instead of the gene, and when the hgSuggest results arrive the
menu is re-rendered, which invalidates any row picked before that. Docent therefore polls
until a row actually matches the term *and* the menu has stopped changing, ranks real hits
above Recent rows, and addresses the row by index at click time.

## Track names come from trackDb

Name tracks exactly as trackDb does — `mane`, `clinvar`, `clinvarCnv`, `dbSnp155Common`.
Docent keeps **no table** of per-track cart variables; it fetches the trackDb of the
server it is driving (hubApi `/list/tracks`, cached in `$TMPDIR` for a day) and derives
what a `track:` step has to send:

- **containers above the track** come along — a composite or view gets the mode you asked
  for, a superTrack gets `show`;
- **the subtrack checkbox** (`<name>_sel`) is stated explicitly, 1 for a visible mode and
  0 for `hide`, because for a composite child the checkbox is what actually decides;
- **nothing is pushed downward**: a container's visibility already reaches its selected
  children (`clinvar: pack` draws the SNVs, the CNVs and the submitted interpretations),
  so a script only names its *deviations* from trackDb — e.g.
  `track: {clinvar: pack, clinvarCnv: hide}`;
- **`hide` never propagates upward** — hiding one subtrack must not turn its composite off.

A step that names both a composite and something under it is applied in **two requests**,
container first. hgTracks reshapes a composite when its container visibility changes, and
that wipes per-subtrack overrides arriving in the same request (`clinvar=pack&clinvarCnv=hide`
alone leaves `clinvarCnv_sel=1` and the CNV row still drawn).

Names trackDb doesn't know — attached hubs, custom tracks, and everything on a quickLift
target (a hub genome, which is not in the listing at all) — fall back to a literal
`name=mode`, which is all Docent can honestly do for them.

Set `DOCENT_ROWS=1` to log the rows hgTracks actually drew after each step (and the
suggestion menus a `goShow:`/`convert:` search saw). That answers
"why is that subtrack still there" and "why is my `mouseover:` track not shown" directly,
instead of by guessing:

    DOCENT_ROWS=1 make BP1
    track: clinvar=pack
      rows: ruler, mane, clinvarMain, clinvarCnv, clinvarSubLolly
    track: clinvarCnv=hide clinvarCnv_sel=0
      rows: ruler, mane, clinvarMain, clinvarSubLolly

It does the same for `mouseover:` — which map boxes matched the name, where the cursor went,
the tooltip that was expected from the box, and the one that actually came up:

    item "4.3.157828209.157828210" in quickLiftChain: 1 map box(es) match (1 in row),
      centers x=[288] -> hovering (288,216), expecting tip "mismatch A->C"
    tip at (288,216): "mismatch A->C"

**After a Convert/quickLift** the target's tracks are served from a hub, so their DOM
ids gain a dynamic `hub_<n>_` prefix (e.g. `hub_191568_quickLiftChain`) that changes
every run. `mouseover:` resolves the track by **suffix**, so you still just write the
plain name — e.g. `track: quickLiftChain` (the Alignment Differences track) hits
`hub_191568_quickLiftChain` automatically.

## Where this is headed

Today a step reaches the browser as CGI variables — `hideTracks=1&clinvar=pack&...` —
which is why the verbs have to know each composite's parameter set by heart. Once the
cart can be handed a structured blob (#37838, "Store cart contents as JSON"), a Docent
step can name the state it wants instead of assembling a pile of `<track>_sel=1`
variables, and the shortcut table above mostly goes away. Docent is meant to be an early
consumer of that work.

The same reason argues for the tour, not the video, being the durable artifact: a script
that describes a state and a route through it can just as well drive a live in-browser
tutorial as an mp4.

## Scripts in the wild

The Current Protocols quickLift paper keeps six of these (`BP1`, `AP1`–`AP3`, `SP1`,
`SP2`) in its own repo, under `currentProtocols/figures/scripts/`, and builds them with
the `docent.mk` rules here. `BP1.docent.yaml` is a good one to read first: gateway → hide
all → set two tracks → drag-select and zoom → Convert with quickLift → open the lifted
view → pin two tooltips into one figure → click through to hgc and out to dbSNP.
