trackLists - RM #37781

Builds one page answering the three questions mirror sites keep asking:
which tracks we cannot pass on, which tracks update themselves, and which
tracks were contributed by someone outside UCSC.

  collect.py           gathers all three lists   -> collected.json
  mkPage.py            renders collected.json    -> HTML
  trackLists.sh  what cron runs

Why list 1 uses more than one query
-----------------------------------
No single trackDb setting marks every restricted track:

  * tableBrowser off is the usual marker, but OMIM does not use it. OMIM sets
    "tableBrowser noGenome ..." with a noGenomeReason naming OMIM's
    distribution terms, so a query for "off" alone silently misses it.
  * Not all noGenome is about licensing. CRISPR and JASPAR set it because a
    genome-wide query times out. The reason text is what separates them.
  * The convention of putting restricted files under an underscore directory
    (/gbdb/hg38/varFreqs/_topmed/ and friends) is real but partial: decipher,
    mexbb, spliceAI, cosmicRegions and hgmd are restricted and are not under one.

So collect.py runs every test it can and unions the results, recording on each
row which tests fired. It also checks the download server both directions:
a MySQL track that exists here but is missing from hgdownload is almost
certainly restricted, and a file we call restricted that hgdownload still
serves is a bug worth mailing about.

Where the otto assembly counts come from
----------------------------------------
The Assemblies column of list 2 is written down in the OTTO table in collect.py,
one list per job, not worked out from trackDb. Counting the assemblies that have
a table matching the keyword counts tables nothing has touched in years: 84
assemblies have an ncbiRefSeq table but ottoNcbiRefSeq.sh runs four, and 61 have
a grcIncidentDb table but runUpdate.sh works through ten. The column exists to
tell someone running a mirror what will drift out from under them, so it has to
come from the job. Adding a job, or changing the assemblies one builds, means
editing that list.

None in place of a list means the job picks its own assemblies at run time.
UniProt is the only one: doUniprot walks dbDb and the GenArk assembly list and
decides from the protein counts, so there is nothing fixed to write down, and
the trackDb count is the best answer available.

A run reports the two ways a list and trackDb can disagree. An assembly named in
OTTO with no matching table goes to stderr, so cron mails it: either the list or
the keyword is out of date. A table on an assembly the job does not rebuild goes
to the log instead, because those are leftovers, there are 51 of them behind GRC
Incident alone, and nothing is wrong.

An otto job whose command matches no keyword is reported both ways: named on the
page under "Jobs we have not described yet", and in the cron mail. It used to be
dropped silently, which is how STRchive was missing from the page for a month.

What the weekly cron mails
--------------------------
Nothing, on a good week. Both scripts write their progress to stdout and anything
needing a person to stderr; trackLists.sh sends stdout to lastRun.log and leaves
stderr alone, so a message from this cron line means one of: a restricted file is
reachable on hgdownload, an assembly list disagrees with trackDb, an otto job has
no description, an assembly is too thinly published for the download test, or the
run failed. The mail used to be the entire progress log every Thursday with the
three reachable files buried in the middle, which is the same as no mail at all.
Do not add an unredirected echo to trackLists.sh.

Careful with the public page
----------------------------
The hgdownload cross-check names restricted files that are currently
reachable. That must never appear on a page anyone can read, so mkPage.py
omits it unless --internal is passed. trackLists.sh writes the public
variant to htdocs and keeps the internal one in this directory.

Speed
-----
The GenArk crawl walks /gbdb/genark and takes more than ten minutes, so it is
cached in cache/contrib.txt and re-run only when the cache is over a week old
(--refresh-contrib forces it). The crawl writes to a temp file and renames,
because a crawl cut short mid-write leaves a shorter list that still looks
plausible. hgdownload directory listings are cached for a day. A run that hits
warm caches takes a couple of minutes; a cold run with the crawl takes fifteen
or so.

Publishing
----------
The script writes goldenPath/help/mirrorTracks.html under htdocs on hgwdev,
next to mirror.html, which is the page that links to it. Pushing that to the RR
needs a /root/<name>AutoPush script added to /etc/crontab by cluster-admin; ask
them for it, and use the existing lines as the template (tipsAutoPush,
thumbNailAutoPush, asmAliasAutoPush).

The otto job is still called trackLists while the page it writes is called
mirrorTracks.html. The page was renamed after review; the job was not, since it
is referred to by path in otto.crontab.

The page must be mode 775 in htdocs. Apache runs the SSI includes on a .html
file only when its execute bit is set (XBitHack); without it the page is served
verbatim and the reader sees the bare content with no menu bar and no
stylesheets. allTips.html is in exactly that state on the RR today, so this is
an easy mistake to repeat. trackLists.sh chmods the page after copying it.

The generated page is deliberately not in the kent tree. That matches the other
generated pages: allTips.html and thumbNailLinks.html live only in htdocs and
are not tracked in git. Only the generator is committed. (assemblyRequest.html
looks like a precedent but is not one; it is now just a redirect stub.)

"The mirror site" in the ticket meant goldenPath/help/mirror.html, confirmed by
Lou on 2026-09-04, and the page is linked from there. It also keeps its link from
the licensing page, src/hg/htdocs/license/index.html, which is where a reader who
wants the exact list of what we cannot hand on is most likely to start.
