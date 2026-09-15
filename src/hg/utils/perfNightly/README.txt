hgTracks start-up and pixel checks
==================================

Two builds of hgTracks, one machine, one moment: how long does each take to start up
and draw, and do they draw the same pixels?  refs #37547

    compare.sh    the engine.  Any two builds, run by hand.
    nightly.sh    the cron job.  Today's master against the last green night.
    scenarios.tsv the eight cells
    sessions/     the committed session fixtures the cells load

Run it by hand
--------------

A "build" is a directory holding both hgTracks and hgRenderTracks.  `make compile` in
hg/hgTracks produces both and installs nothing, so a working tree is a valid side:

    make -C src/hg/hgTracks -j8 compile

    ./compare.sh --a /usr/local/apache/cgi-bin --a-label genome-test \
                 --b ~/kentMergeVis/src/hg/hgTracks --b-label mergeVis

Useful options: --iters N (default 5), --scenarios FILE, --out DIR, --keep-png.
It exits 0 when every cell drew the same tracks and the same pixels, 1 otherwise.

The nightly
-----------

    30 4 * * * /hive/users/braney/perfNightly/kent/src/hg/utils/perfNightly/nightly.sh --update

Same shape as the Docent regression nightly in ../docent/tests/regress: its own clone,
--update resets it to origin/master and re-execs, a mail every night whether or not
anything failed, sixty days of logs, always exit 0.  04:30 is the quiet window --
catalogNightly is at 03:30, the docent run at 04:10 for about seven minutes, and the
build user's first pass at 05:45.

It builds master fresh in its own clone rather than reading /usr/local/apache/cgi-bin,
because cgi-alpha takes hand installs from developers as well as the build user's seven
passes a day, so some nights it is not master.

Green promotes today's binaries to be tomorrow's reference.  Red pins the reference, so
the alert survives to the morning and the commit range in the mail keeps covering the
whole regression.  A rendering change that was intended is cleared with

    nightly.sh --accept "why"

which writes a line to accepted.log.  That file and history.csv live outside the
checkout, because --update does `reset --hard` and would wipe anything in the tree.


Why it is built this way
========================

Each of these was measured, and each of them silently produces a wrong answer if it is
left out.  None is a matter of taste.

The baseline is a rolling reference BINARY, not a golden image
--------------------------------------------------------------
Pixels move for two reasons: the code changed, or the data changed.  ClinVar, GENCODE
and the GenArk hubs all update underneath us, so a committed golden PNG or a saved
render from yesterday goes red for reasons that have nothing to do with the tree.

Two binaries reading the same data at the same moment do not have that problem.  A data
change moves both renders equally and the pixels still match.  Only a code change moves
them.  That is the whole reason the reference is a binary.

The binaries are run directly, not over HTTP
--------------------------------------------
Two builds reached over HTTP do not share a udcDir, a trackDb, or an hg.conf, and each
of those differences shows up as a code tax that is not in the code.  The
bench-hgtracks-rts skill learned this on #37525: a sandbox run showed a +27% narrow-view
tax and a direct-binary run +263%, and both vanished to parity once the two builds ran
against the same config.  Here both sides get one hg.conf that includes production's, so
the only difference left is the binary.

Four things the direct path needs, each of which fails quietly
--------------------------------------------------------------
  * A CWD you own with a `trash` symlink beside it.  hgTracks writes its PNGs relative
    to the CWD, and /usr/local/apache/cgi-bin/hgt is not group-writable, so running
    there dies with "mustOpen: Can't open ./hgt/..." and returns a plausible 37 KB page
    instead of an error.

  * An `htdocs` symlink in the same place.  freetype loads
    ../htdocs/urw-fonts/n019003l.pfb by a path relative to the CWD, and without it every
    render dies with a stack dump.  Passing textFont=Bitmap avoids that, which is what
    the benchmark skill does -- but then the pixels are not the ones production draws,
    so it is no use here.

  * A cacheTrackDbDir per side.  TRACKDB_VERSION is baked into each cache dump's
    filename, so two builds whose layout differs never read each other's dumps -- and
    the side whose version is new starts with an EMPTY cache and pays a full trackDb
    parse on every cell.  Measured 2026-09-15: /data/trackDbCache held 21,919 dumps at
    version 9 and none at version 10, which is what the #37547 branch writes.  Without a
    per-side cache and a per-cell warmup, that one-time cost is reported as a permanent
    regression.

  * A cart per side.  Two builds that disagree about cart format cannot share one.  On
    #37547 the branch's cartSetVisString() deletes the legacy bare key, so the other
    build reads nothing and silently draws trackDb defaults -- a fast, WRONG render that
    no timing check can see.

Cold and warm are different measurements and both are reported
--------------------------------------------------------------
A one-shot render gives every request a fresh cart, so it measures only the cold first
load, including any one-time migration.  On #37547 a 28-cell one-shot run reported the
branch 2-7% SLOWER, while the same cells on a persistent cart showed it 43.8% FASTER --
same binaries, same config.  Reporting one number would have been reporting the opposite
of the truth.

    cold   a fresh cart every iteration: session load plus first draw
    warm   the same cart re-rendered by hgsid: what a user clicking around pays

Measured on genome-test, 2026-09-15: hg38 with a 38-row clinical session took 8.56 s on
the first hit and 1.82 s warm.  That 4.7x gap is the start-up cost this exists to watch.

The A/B order alternates
------------------------
Running A first in every iteration lets B ride on the caches A has just warmed.
Measured 2026-09-15 as a systematic 4.5% in B's favour with the SAME binary on both
sides.  Alternating the order cancels it; the residual noise floor is then about 1%.

The assertion is trackLog, plus the drawn row count
---------------------------------------------------
hgTracks writes `trackLog <n> <db> <hgsid> name:vis,...` to stderr: the exact set of
tracks it loaded and at what visibility.  Both sides must produce the same set AND the
same number of drawn rows.

A row count alone cannot see a track that came up dense instead of pack.  The trackLog
set alone is not the drawn rows either -- it names every track hgTracks looked at,
including the subtracks of a hidden composite at their own trackDb visibility, so a
default hg38 view logs 563 of them and draws about twenty rows.  Hidden tracks are not
logged at all, so there are no :0 entries to filter.  Both counts are checked.

The session fixtures are loaded, not replayed
---------------------------------------------
A cell that needs many tracks on names a file in sessions/, which compare.sh publishes
to a served directory on every run -- so what is served is provably what is committed --
and hgTracks loads it with hgS_doLoadUrl.

Replaying the file's name/value pairs as a GET does NOT work, and it fails in a way that
looks plausible.  Composite _sel state, superTrack show state, and the default-visibility
pass all live in hgSession's load path rather than in the variables.  Measured
2026-09-15 against the 71-track Clinical_SNVs_hg38 session: a flat replay into a
hideAll'd cart drew 23 of the 71 tracks and invented four the session never had.  Loading
the same file by URL reproduced the session exactly.

The GenArk cells do not use a session fixture
---------------------------------------------
Every track in a hub is named hub_<hubStatusId>_<track>, and the id is assigned per
machine.  A frozen fixture naming those tracks would address the wrong rows -- or
nothing -- on another machine or after a hubStatus change, and nothing would warn.  The
heavy GenArk cells send hgt.visAllFromCt=pack instead, which is id-independent.  Both
sides see the same hub on the same night, and the trackLog check catches any difference.

GenArk is a separate axis because its start-up is a different path: hub.txt and the hub
trackDb arrive over udc, not from MySQL.


Known limits
============

  * The udc cache is warm.  These cells measure start-up against a warm shared udcCache,
    so the real cost of opening a GenArk hub cold is not in the numbers.  Giving the
    GenArk cells a private udcDir would measure it, at the price of an absolute number
    that is much larger than what a user sees and is comparable only to itself.

  * The heavy cells send a query string of about 20 KB.  That is fine through argv, and
    it is well over Apache's 8,190-byte limit, so these scenarios cannot simply be
    re-pointed at an HTTP endpoint.

  * A cell's workload follows live trackDb.  A track added to a composite changes what
    the cell draws.  Both sides see it on the same night, so the comparison stays valid,
    but the absolute numbers in history.csv shift and the creep alarm may fire once.
