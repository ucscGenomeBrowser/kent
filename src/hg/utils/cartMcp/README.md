# cartMcp - an AI front end that composes Browser sessions, as a driver for #37838

`cartMcp.py` is an MCP server.  Point any MCP client at it (Claude Code, Claude
Desktop) and ask for a view in words:

> make me a session with a gene track, ClinVar, and this bigBed:
> https://example.org/myPeaks.bb

It finds the tracks in trackDb, works out the cart variables, writes a session
file, and hands back a `hgS_doLoadUrl` link that loads it.

It exists to answer the question Redmine #37838 is stalled on: is there a
consumer that actually needs the cart to be JSON?  So the interesting output is
not the session, it is `friction_report`: every time the server has to apply a
naming rule that is written down nowhere, guess, or give up because var=val
cannot express something, it records that with a stable category.  The counted
log is the empirical requirements list for the schema, and if the log stays
boring, that is an argument for leaving the format alone.

Nothing here changes the browser.  It only writes files and composes URLs, so
it works against today's tree and today's production site.

## Running it

Needs `hgsql` access to trackDb and hgcentral, and outbound https to
api.genome.ucsc.edu.  No third-party python; stdlib only, 3.6+.

    ./cartMcp.py --selftest      # exercise every tool, no MCP client needed

To use it from Claude Code:

    claude mcp add cartMcp -- /cluster/home/braney/kent/src/hg/utils/cartMcp/cartMcp.py

## Tools

| tool | what it does |
| --- | --- |
| `search_tracks` | trackDb search by name/label; reports composite, view and superTrack membership, and flags tracks that are on hgwdev but not on the public site |
| `describe_track` | type, container chain, children, the cart variables the #37838 catalog says are legal for that track's type, and the filters its trackDb stanza declares |
| `build_session` | compose a session from a track list, a position (gene symbol or coordinates), user bigBeds and hubs; returns a loadable URL, what it took, and the same request written as a #37838 JSON cart |
| `friction_report` | counted friction categories with what a JSON cart would do instead |

Output goes to `~/public_html/cartMcp/`: one `<name>.txt` session file per
session, `<name>.ct.txt` for any custom tracks, and `frictionLog.jsonl`.
The production track list is cached for a day in `~/.cache/cartMcp/`.

## What it found so far

Every item below is something the server hit while building real sessions, not
a hypothetical.  `friction_report` prints the current counts.

- **compositeSelMangling** - turning on one subtrack takes two variables in two
  different conventions: `clinvar=pack` plus `clinvarMain_sel=1`.
- **viewLevelMangling** - a subtrack under a view needs the three-level
  `dbSnp155Composite.variants.vis`, assembled by string concatenation.
- **superTrackShow** - a superTrack child also needs `omimContainer=show`, a
  value outside the five visibilities every other track uses.
- **explicitHides** - "just these tracks" cannot be said.  A session file is a
  whole cart, so a three-track view of hg38 emits 41 `hide` lines plus a
  `_sel=0`, and getting that list right takes three separate rules: top-level
  tracks with `visibility` set; the visible children of superTracks marked
  `superTrack on show`, which no query for top-level tracks will find; and the
  default-selected siblings inside any composite the request turns on, which
  have to be switched off with `<sibling>_sel=0` rather than `hide`.  Missing
  any one of the three silently leaves extra tracks in the view: the first
  version of this server did, and drew four tracks nobody asked for.
- **containerVisConflict** - two subtracks of one composite requested at
  different visibilities both write the composite's single visibility slot, so
  one silently overwrites the other.  Nested JSON makes that one field, written
  once, and makes the conflict something the server could answer.
- **customTrackOutOfBand** - a user's bigBed cannot ride in the session payload
  at all.  Custom tracks persist as `ctfile_<db>` pointing at a server-side
  trash file, so the file has to be attached by a second mechanism
  (`hgt.customText` on the URL) that the session format knows nothing about,
  and its visibility ends up in the track line rather than the cart.
- **hubIdUnknowable** - a hub track's visibility lives under
  `hub_<id>_<track>`, where `<id>` is a hubStatus row number the server assigns
  when it first loads the hub.  A caller cannot write that name in advance, so
  one payload cannot both attach a hub and set visibility on its tracks.
- **releaseGatedTrack** - hgwdev's trackDb contains tracks that do not exist in
  production, and `release alpha` is consumed when the table is built, so
  nothing local records it.  Telling the difference takes a call to
  api.genome.ucsc.edu (and `trackLeavesOnly=1` is the wrong form: it omits the
  container names a subtrack request has to emit).
- **noValidation** - the cart takes every name it is handed and silently drops
  what it does not recognise, so a hallucinated track name yields a 200 and a
  session quietly missing that track.  `gencodeV50` is the obvious guess for
  the GENCODE V50 track and it is wrong; the real name is `knownGene`.  This server rejects unknown names by
  checking trackDb itself, which is exactly the check a JSON input path could
  do once, centrally, and report.
- **positionNotAddressable** - resolving `BRCA1` to coordinates lives inside
  the CGIs (hgFindSpec), so this reimplements a slice of it against
  ncbiRefSeqCurated.
- **noTypeToCatalogMap** - nothing maps a trackDb `type` string to the
  catalog's config-type key, so which variables are legal for a track has to be
  guessed from the type's first word.

One more, not a cart-format problem but worth knowing for anyone generating
sessions: a session with no `pix` in it makes hgTracks answer the first request
with a JavaScript stub that calls `addPixAndReloadPage()`, so the session must
carry an image width or the first load draws nothing.

## Relation to the other #37838 tooling

- `hg/utils/cartTrackVarCatalog` - the 340-variable track-scoped catalog.
  `describe_track` imports it, so what this server offers a caller cannot drift
  from what the catalog claims.
- `hg/utils/urlCommandCatalog` - #37923, what may appear on a CGI URL.
- `hg/utils/sessionCartAudit` - both catalogs checked against 6,428 real saved
  sessions.
