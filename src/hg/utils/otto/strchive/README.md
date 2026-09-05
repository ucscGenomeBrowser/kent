# STRchive otto job

Keeps the `strchive` track (under the `strVar` "Tandem Repeat Variation"
superTrack) in sync with STRchive's GitHub releases, on hg19, hg38 and hs1.

STRchive publishes a release one to two times a month.  Since v2.26.1 each release
ships bigBed files built for the UCSC browser
(`STRchive-disease-loci-<tag>.<assembly>.ucsc.bb`), so there is no conversion step
left on our side: `strchiveOtto.py` downloads the file and repoints the `/gbdb`
symlink at it.  The STRchive side of this arrangement is
[dashnowlab/STRchive#333](https://github.com/dashnowlab/STRchive/issues/333).

## Layout

    /hive/data/outside/otto/strchive/
        strchiveOtto.py          installed by `make install` from the kent tree
        lastRelease.txt          tag of the release currently live
        releases/v2.26.1/
            strchive.hg19.bb     the files /gbdb points at
            strchive.hg38.bb
            strchive.hs1.bb
            version.txt          "2.26.1 (2026-09-04)", shown by hgTrackUi

    /gbdb/<db>/strVar/strchive.bb          -> releases/<tag>/strchive.<db>.bb
    /gbdb/<db>/strVar/strchive.version.txt -> releases/<tag>/version.txt

for db in hg19, hg38, hs1.  One version.txt is shared by all three: a STRchive
release carries the same loci on every assembly.

Old releases are kept.  They are 45 kB each, and keeping them means a rollback is
one command.

`trackDb/human/strVar.ra` is shared by all the human assemblies.  Its bigDataUrls
use `$D`, and `hgTrackDb -strict` drops any member whose file is not there, so
STRchive shows up on all three while the hg38-only members of the superTrack stay on
hg38.  The same file carries `dataVersion /gbdb/$D/strVar/strchive.version.txt`,
which is how the release number and date reach the track description page.

hs1 is served to the browser as a curated hub rather than a native database, which
made that `dataVersion` file path print raw instead of being read -- hub tracks are
not allowed to name local files.  `hui.c:checkDataVersion` now makes an exception for
paths under `/gbdb`, which is public data either way.

## Running it

The cron entry is in `../otto.crontab`, weekly on Monday.  The script is silent when
the latest release is the one already live, and mails the otto group when it updates
or when anything fails.

    strchiveOtto.py -n              # what would happen
    strchiveOtto.py -f              # re-do the current release
    strchiveOtto.py -r v2.26.1      # roll back to an older release

A rollback only holds until the next Monday: the cron sees a newer release than the
one in `lastRelease.txt` and moves forward again.  To make it stick, comment the
line out of `otto.crontab` and install it as otto.

It refuses to publish a release whose locus count differs from the live one by more
than 25%, on the grounds that STRchive curates a few loci at a time.  Check the file
it left in `releases/<tag>/` and re-run with `--force` if the jump is real.

## Getting the update to the RR

Refreshing `/gbdb` on hgwdev is only half of it: an admin push has to copy the file
and the version file out to hgwbeta, the RR nodes and the mirrors.  There is no
push for this track yet -- `strchive.version.txt` has never been on hgdownload,
which is why the public track description page shows no data version.

`strchiveAutoPush` in this directory is the draft to hand to the sysadmins.  It
follows the pattern of `mitoMapAutoPush` and `varChatAutoPush` in
`/hive/groups/adminGit` (branch `hgwdev`) and wants a weekly `/etc/crontab` entry
shortly after the otto cron.  **Until it is installed, the otto job keeps hgwdev
current and the RR does not move.**

For that reason the new trackDb is held at alpha.  `human/strVarNew.ra` is included
`alpha` only; beta and public still get the old hg38-only `human/hg38/strVar.ra`, so
the RR keeps describing the data it actually has.  The header of `strVarNew.ra` lists
what to delete and rename when the push goes in.  See #38268.

## Adding an assembly

STRchive currently builds hg19, hg38 and T2T-chm13 (= hs1) files, and we take all
three.  If they add another: put it in `dbToAsm` in `strchiveOtto.py`, create
`/gbdb/<db>/strVar/`, and run with `--force`.  Nothing in trackDb needs to change --
`strVar.ra` is already assembly-independent -- but the assembly has to be a human one
for it to pick the file up.

## Editing

Edit and commit the copy in the kent tree, then `make install`.  Never edit the copy
under `/hive/data/outside/otto/` directly -- `ottoCompareGitVsHiveFiles.py` mails the
otto group when the two drift apart.
