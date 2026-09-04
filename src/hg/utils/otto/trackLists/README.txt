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
The script writes /usr/local/apache/htdocs/trackLists.html on hgwdev.
Pushing that to the RR needs a /root/<name>AutoPush script added to
/etc/crontab by cluster-admin; ask them for it, and use the existing lines as
the template (tipsAutoPush, thumbNailAutoPush, asmAliasAutoPush).

The page must be mode 775 in htdocs. Apache runs the SSI includes on a .html
file only when its execute bit is set (XBitHack); without it the page is served
verbatim and the reader sees the bare content with no menu bar and no
stylesheets. allTips.html is in exactly that state on the RR today, so this is
an easy mistake to repeat. trackLists.sh chmods the page after copying it.

The generated page is deliberately not in the kent tree. That matches the other
generated pages: allTips.html and thumbNailLinks.html live only in htdocs and
are not tracked in git. Only the generator is committed. (assemblyRequest.html
looks like a precedent but is not one; it is now just a redirect stub.)

Where the page should finally live is not settled. The ticket says only that it
is "autoPushed out on a cycle and linked to the mirror site", naming no path, so
the top-level htdocs location here is a choice, not a requirement, and it can be
moved. The natural reading of "the mirror site" is a link from
goldenPath/help/mirror.html, but that is an inference from Lou's wording rather
than something the ticket states.
