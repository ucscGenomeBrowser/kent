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
`track: {clinvar: pack}` knows the composite's clean parameter set, and `mouseover:`
knows that a lifted track's DOM id gains a per-run `hub_<n>_` prefix. The author writes
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
reset: true             # cartReset first (clean cart + fresh quickLift hub)
size: [1000, 760]       # viewport
pix: 850                # browser image width
pace: 1.2               # seconds to dwell after each step
shotHold: 2.2           # extra seconds the video pauses at a shot
trackAnim: false        # skip the visible dropdown gesture on `track:` (nav only)
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
| `hide: all` | Click **Hide all**. |
| `track: {mane: pack}` | Set a track's visibility (`hide/dense/squish/pack/full`). The mouse visibly glides to that track's control dropdown, opens it, and picks the mode (then the state is applied). |
| `track: {dbSnp155: pack}` | Composite: expands to the clean dbSNP-common config. |
| `track: {clinvar: pack}` | Composite: ClinVar clean config. |
| `track: {anyTrackName: dense}` | Any track by its tdb name → `name=mode`. |
| `mouseover: {track: dbSnp155, item: rs28406051}` | Hover a **named** item to raise its tooltip. `item:` matches the item's map box (its `&i=<name>` HREF or its TITLE) and hovers that rectangle's center — so it lands on the **correct row** even when items are stacked, which a bare coordinate can't do. `title: "SHH"` matches the TITLE text only; `value: "..."` matches a JSON `mouseOver` span (wig/dense tracks). |
| `mouseover: {track: mane, at: chr7:155805900}` | Hover by **position** when you don't need a specific item: genomic coord (`at:`), a fraction across the view (`frac: 0.5`), or a raw pixel (`x: 400`); the y is forced to the middle of that track's row (cannot disambiguate stacked items). Optional on any `mouseover`: `hold: 2.5` (seconds to dwell) and `shot: tip_mane` (capture the image **plus** the tooltip in one still). |
| `mouseover: {track: dbSnp155, item: rs28406051, pin: true}` | `pin: true` **records** that tooltip (its text + position) so a later `pinShot:` can show several mouseovers open together in one figure. Nothing is added to the recorded page, so the **mp4 is unaffected** (it still shows only the transient native tooltip). Set `pinMouseovers: true` at the top of the file to record every mouseover by default (`pin: false` opts one out). Records accumulate within a view and are cleared on the next nav. |
| `pinShot: all_tips` | Write `<name>.png` with **all recorded (pinned) tooltips open at once**. Rendered on a throwaway page that shares the session (same cart/view) — never on the recorded page — so it never appears in the mp4. Consumes the recorded set (clears it). Place it after the `mouseover` steps whose tooltips you want shown together, before any nav/zoom. |
| `convert: {to: 2257.pat, quicklift: true, hideDefaults: true}` | View→Convert, pick target, QuickLift on, **re-checks Hide-defaults** (it reverts when the Assembly menu reloads), Submit. `to:` accepts an accession (`GCA_018466845.2`) or a label fragment (`2257.pat`, matched against the dropdown text). |
| `hub: https://example.org/hub.txt` | **Quick, silent** attach of a track hub by URL (`hgTracks?hubUrl=...`): connects the hub so its tracks are available at their hub-declared visibility. Follow with `track:` to turn specific ones on. Map form `hub: {url: ..., db: hg38, position: chr7:...}` overrides the db/position (default: current `db` + last position). |
| `addHub: https://example.org/hub.txt` | **Demonstrates the attach through the UI** (for the figure/video): opens My Data → Track Hubs, clicks the **Connected Hubs** tab, types the URL into the box, and clicks **Add Hub** — cursor glides and the URL is typed on screen. Then, on the "Hub Connect Successful" page, it **clicks the `Open:` link for `db`** so the demo ends on the browser with the hub loaded. Map form `addHub: {url: ..., db: hg38, shot: loaded}` sets which assembly to open and captures the still on that tracks view. Use `hub:` instead when you just need the hub attached without showing the steps. |
| `addCustomTrack: <text-or-url>` | **Demonstrates loading a custom track via the UI**: opens My Data → Custom Tracks, types the track data (or a data URL) into the paste box, clicks **Submit**, then clicks through to the browser (**Go to first annotation**). Bare string is the data or URL; map form `addCustomTrack: {data: "track ...\nchr7 ...", db: hg38, goto: first, shot: loaded}` (use `url:` for a data URL, `goto: current` to land on **Return to current position** instead). Data is inserted literally (tabs/newlines preserved). In YAML, a multi-line track uses a block scalar: `addCustomTrack: |` then the indented lines. |
| `addPublicHub: GTEx` | **Demonstrates connecting a PUBLIC hub via the UI**: opens My Data → Track Hubs, the **Public Hubs** tab, types the search terms, clicks **Search Public Hubs**, then clicks **Connect** on the matching hub row, and finally **clicks the `Open:` link for `db`** to land on the browser. Bare string is the search term (also used to match the row). A search usually returns several hubs, so use the map form `addPublicHub: {search: "GTEx", match: "GTEx Analysis Hub", db: hg38, shot: loaded}` to pick the exact hub by a substring of its row text (`match:` defaults to `search:`) and the assembly to open. If no row matches it **won't connect** (warns and stops) rather than pick the wrong hub. |
| `drag: {from: "chr7:155,805,900", to: "chr7:155,806,950", then: zoom}` | Emulates the **Shift+drag-select** gesture (quote the coords — unquoted commas split a `{..}` flow map): the cursor sweeps across the selection (a visible selection box is drawn) and the browser's own drag-select dialog is raised, then a button is clicked. Endpoints as genomic coord (`from:`/`to:`), fraction (`fromFrac:`/`toFrac:`) or raw px (`fromX:`/`toX:`). Optional `track:` picks the row the box is drawn over; default is the top of the image. `shot: dragselect` captures the open dialog. `then:` = `zoom` (default → **Zoom In**) \| `highlight` (→ Single Highlight) \| `cancel` (Escape, view unchanged). (A real button-held drag would just pan, so the dialog is driven directly.) |
| `open: lift` | Click the returned coordinate link → the lifted view. |
| `zoom: out` / `zoom: in` | One zoom step (2×). |
| `shot: source` | Write `<name>.png` (the track image) **and** pause the video here. |

Escape hatches for anything the verbs don't cover: `goto: <url>`, `click: <sel>`,
`hover: <sel>`, `wait: <sel>`, `sleep: <ms>`.

## Gotchas

The renderer lints for one YAML trap before it opens a browser: in a flow map a colon
needs a trailing space, so `{item:name5568747}` parses as the single key
`"item:name5568747"` and the intended `item:` argument is silently dropped — the verb
then quietly falls back to a default. Any argument key containing a `:` gets a warning.

Quote genomic coordinates inside a `{...}` flow map. An unquoted comma in
`chr7:155,805,900` splits the map.

## Track-name shortcuts

`mane`, `dbSnp155`, `clinvar` expand to their known-good composite parameters so you
don't hand-write them. Any other name falls through to `name=mode`. Add more shortcuts
in the `TRACKS` table in `docent.js`.

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
