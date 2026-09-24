Saved sessions for the regression scripts, as text files.  refs #38252

A named session lives in one machine's hgcentral, so a script that loads one with
`loadSession: {user, name}` runs only on that machine: hgwbeta and the RR answer
"Could not find session".  A text file loaded by URL runs anywhere, and a QA reader can
load the same state with one link:

    hgTracks?hgS_doLoadUrl=submit&hgS_loadUrlName=<raw GitHub URL of the file>

Scripts load these by their raw GitHub URL,

    https://raw.githubusercontent.com/ucscGenomeBrowser/kent/master/src/hg/utils/docent/tests/regress/sessionFiles/<name>.txt

so a new or changed file works once it is pushed, which is also when the nightly run
sees the script.

To make one: load the saved session on genome-test in a fresh cart, then use hgSession's
own "save to file" (hgS_doSaveLocal, compression none), and drop the hgS_ lines, which
are the load and save requests themselves.  Name the file after the session it came from.

Two kinds of session cannot become a file this way:

  * A session holding a custom track stores it as a file and a customTrash table on the
    server that made it.  Put the track's source in its own file here, and replace the
    session's ctfile_<db> line with  hgt.customText <raw URL of that file>.  Never commit a
    track line that carries a password in a URL, even a commented-out one.
  * A lifted (quickLift) session names its quickLift hub by a path on the server that made
    it, and re-pointing it at a copy of the hub does not work (#38046).  Build the lifted
    state in the script with steps instead.

RM_35326_bug.txt         Gerardo/RM_35326_bug, for rm35580
RM_36805_TOGA_hangs.txt  Gerardo/RM_36805_TOGA_hangs, for rm36805
quickLift_CT_hub.txt     Gerardo/quickLift_CT_hub, for rm36340
quickLift_CT_hub.ct.txt  its 43 custom tracks, less two commented-out ones with a password
