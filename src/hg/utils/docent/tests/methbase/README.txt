Docent tests for MethBase on the RR
-----------------------------------

Ten scripts, run by hand against genome.ucsc.edu:

    make test                  # all ten
    make test T=mbSignal       # just one
    make preflight             # the hub URLs and the server, no browser

    make test TARGET=hgwdev-demo9      # the same ten, against another server

TARGET is in ../docentTest.mk and is described in ../README.txt.  These ten are worth
redirecting at a branch build, because both hubs are reachable from any of our machines
and nine of the ten are hg38 and mm39 only.  What changes on another server is the
trackDb underneath: on 2026-09-14 all ten passed against the demo9 sandbox in 4m39s,
against 5m17s for the same ten on the RR, and mbMouse had to be given an mm39 trackDb
there first -- until it was, the mm39 page was the "can not find any trackDb tables"
error and the script died at step 2 with no #hgt.hideAll to click.

What "MethBase on the RR" means
-------------------------------

Not the `Methbase` composite you can see on hgwdev.  That one -- a faceted composite of
19,448 trackDb rows under the `dnaMethylation` superTrack, with a metaDataUrl and a colors
json in /gbdb/hg38/methBase2/ -- is on genome-test only.  hgTrackUi?g=Methbase answers
"Can't find Methbase in track database hg38" on both genome.ucsc.edu and hgwbeta as of
2026-09-14, and `select tableName from trackDb where tableName like '%ethbase%'` on
genome-mysql.soe.ucsc.edu returns nothing for hg38 or mm39.

What a user of the RR has today is two public hubs, both in hgcentral.hubPublic:

    MethBase2         http://smithlab.usc.edu/trackdata/methylation/hub.txt
                      32 assemblies, 33,270 tracks on hg38, updated by the Smith lab
    MethBase (Legacy) https://hgdownload.soe.ucsc.edu/hubs/methbase/v1/hub.txt
                      605 profiles, frozen, served by us

Nine scripts here drive MethBase2 and one drives the legacy hub.  If the native composite
ships to the RR, these do not cover it and a second batch is owed.

Why these are not in ../regress
--------------------------------

../regress is one script per fixed bug, named for its ticket, and each carries a `proof:`
key saying whether it has ever been watched to fail for its own reason.  These are not
that: no ticket, no bug, and nothing here has been seen red for a reason that was then
fixed.  They also point at the RR rather than at genome-test, which is what ../regress and
its nightly assume throughout.  So they are a directory of their own, they carry no
`proof:` key, and they are NOT in the nightly.  Wiring them in is a separate decision --
nightly.sh runs out of its own clone and labels every flip `genome-test`.

The whole run took 5m17s on 2026-09-14.  mbPublicHub is the single most expensive script
-- the Public Hubs search alone is about fifty seconds -- and the rest are ten to twenty-five
seconds each.  Every one of them fetches data from smithlab.usc.edu at render time, so the
wall clock here depends on someone else's web server.

What each one covers
--------------------

  mbPublicHub   Connect through My Data -> Track Hubs -> Public Hubs, the only route a
                user who never types a URL has.  The only script that touches
                hgHubConnect, so the only one that says hubPublic still lists MethBase2.
  mbDefaults    The exact 22 rows a cleared cart plus a connect draws, and nothing else
                (plus the ruler, which `exact:` counts).
                The canary: see below.
  mbSignal      The four default rows really drew their data, checked in the pixels, at
                the SNRPN imprinted locus.
  mbDetails     A hypomethylated region clicked through to hgc: the item's coordinates,
                its size, and the study metadata the hub's own html page carries.
  mbContainer   The Human methylome studies container page -- 511 study composites, 824 kB
                of HTML, the single most expensive thing the RR does with MethBase.
  mbMatrix      One study's page: five views, the experiment x view matrix, and the three
                data types the hub leaves behind a hidden view.
  mbSubtracks   Deselect one experiment in the matrix, Submit, and check the cart kept it.
                The only script here that changes MethBase's state.
  mbMouse       The hub on mm39: a different container, a different default study, and an
                experiment with no HMR file.
  mbLegacy      The legacy hub on hgdownload still attaches and draws its ten rows.
  mbZoomOut     Chromosome scale, where the zoom levels of the remote files answer instead
                of the primary data.

Three things worth knowing before writing an eleventh
------------------------------------------------------

**A hub track's cart name cannot be written by `track:`.**  Every id, every cart variable
and every row on a hub page carries a per-hub `hub_<n>_` prefix -- MethBase2 is 8415 on
the RR's central today and the legacy hub is 5486122 -- and `track:` has no way to build
it, because docent derives track names from hubApi's listing for the assembly, which knows
nothing about a hub connected in this session.  So nothing here uses `track:`, and no
script names a hub number.  What works instead:

  * `rows:`, `noRows:`, `color:`, `click: {track:}` and `mouseover:` all match a row by
    SUFFIX, so `SRP007400_SRX081759_HMR` finds `hub_8415_SRP007400_SRX081759_HMR`.
  * `a.trackLink[data-track$="..."]` in the track group listing at the bottom of the
    browser page is a suffix selector onto the container's hgTrackUi page.  That is how
    four of these scripts reach a settings page.
  * the matrix buttons on a composite's page are named for the SUBGROUP tag
    (`#btn_minus_A00000_all_left_bottom`, `#btn_plus_v3pmd_all_left_top`), not for the
    track, so they are id-free too.
  * a visibility can therefore only be changed through the matrix checkboxes and Submit
    (mbSubtracks).  A view's own dropdown is a `select`, and Docent has no verb that
    drives one, so no script here turns on AMR, PMD or CpG reads.

**A remote hub's dead file is invisible to every text check.**  hgTracks catches the
abort and paints the row as a 240,240,180 bar with the message inside the png.  Same row,
same name, same height, same map boxes.  Four scripts here assert `color:` for that reason
-- it is the rm38310 lesson, except that here the thing that breaks is not our code but a
URL on someone else's web server.  Any new script that draws a MethBase row should carry
`not: "240,240,180"` on it.

**Three scripts assert the whole set, and only one of them can rot.**  Three of
MethBase2's 511 hg38 composites are not `visibility hide`, so a connect draws 22 rows, and
mbDefaults says exactly those with `exact: true`.  A hub update that adds a default-on
study, or an experiment to one of the two, turns that script red.  That is the point -- it
is the script whose subject IS the default set -- but it is why the other seven hg38
scripts name only the rows they care about, so the same update does not make the whole
directory shout.  Read the `drawn:` list in the failure before changing anything: it says
what the hub now comes up with.

mbMouse and mbLegacy use `exact: true` as well, on safer ground.  mbMouse's set is one
mm39 study whose shape is the assertion (four HMR rows for five experiments), and mbLegacy
is over a hub that is frozen by definition.
