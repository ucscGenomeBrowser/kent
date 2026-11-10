The 'status' field in the ottoRequest table maintains state:

  0 == request has been received by API (set by API)
  1 == request has been acknowledged by ottoRequest.py (set by ottoRequest.py)
  2 == alignment has been started, galaxy jobs (lastz) or a local cluster
         run (minimap2) (set by ottoRequestAlign.sh)
  3 == galaxy jobs have completed, download started (set by workflowMonitor.sh)
         lastz only, minimap2 requests go from 2 to 4
  4 == alignment done and track files have been created, for lastz after
         the download from galaxy (set by ottoRequestWatch.sh)
  5 == symlinks for tracks are in place, ready to push files
         (set by ottoRequestWatch.sh)
  6 == push of files is complete (set by ottoRequestPush.py)
  7 == error condition - some error has taken place, set by any script
  8 == final email notification has been sent, galaxy workflow
       has been deleted, process is complete (set by ottoRequestWatch.sh)

Method of operation:

############################################################################
###  1.
User makes request via the liftRequest.html page.

The submit button causes a call to the API with four arguments:
  fromGenome toGenome email comment
The API does an INSERT operation into the hgcentral.ottoRequest table:
"INSERT INTO %s (requestType, fromDb, toDb, email, comment, requestTime,
  status, buildDir) VALUES ( 'liftOver', '%s','%s','%s','%s',now(), 0, '')",
              ottoTable,  fromGenome, toGenome, email, comment);

For example:
*************************** 1. row ***************************
          id: 1
 requestType: liftOver
      fromDb: GCF_000260355.1
        toDb: GCF_004115215.2
       email: nullmodel@gmail.com
     comment: testing the galaxy pipeline, from: star-nosed mole (GCF_000260355.1), to: platypus (Pmale09 v4 2020) (GCF_004115215.2)
 requestTime: 2026-04-23 15:20:19
      status: 0
    buildDir: 
completeTime: NULL

############################################################################
###  2.
### cron entry in the otto user:
# 1,8,15,22,29,36,43,50,57 * * * * /hive/data/outside/otto/liftRequest/ottoRequest.py
###
ottoRequest.py - otto user cron job running each minute to watch the
               - ottoRequest table in hgcentral.  Watches for two types of
               - entries: liftRequest and assembly - when new entries are
               - detected (status==0) it marks the table entry as
               - pending (status=1) and sends
               - out notification emails, one to the requesting user and
               - Bcc to either chain-file-request-group@ucsc.edu for liftRequest
               - or Bcc genark-request-group@ucsc.edu for assembly requests.
               - The From and Return-To addresses are genome-www@soe.ucsc.edu
               - and I believe that will receive bounces from bad user addresses

############################################################################
###  3.
#9,20,31,42,53 * * * * ~/kent/src/hg/utils/otto/userRequests/ottoRequestWatch.sh
###
ottoRequestWatch.sh - cron script running in hiram hgwdev account to watch
               - the ottoRequest table.
               - phase 0: when new entries show up (status=1) it first
               - checks whether both directions have already been built
               - (a lastz.<other> link, or a miniMap2.<other> link whose
               - build has its fb.*Link.txt file) and are registered in
               - liftOverChain and quickLiftChain, if so it goes
               - straight to 'status=5' for the push.
               - phase 1: otherwise it uses ottoRequestAlign.sh to start
               - the alignment, lastz in galaxy or minimap2 on the local
               - cluster, decided there by mash distance.
               - ottoRequestAlign.sh will set 'status=2' as the alignment
               - is started.
               - phase 2: for lastz, uses workflowMonitor.sh to check the
               - status of jobs that are in 'status=2' state, when the
               - galaxy run is complete, will set 'status=4'
               - for minimap2 (buildDir named miniMap2*) it checks the
               - ottoMiniMap2.pid/.exit files in buildDir:
               -   still running - nothing to do
               -   pid gone without an .exit file, or .exit not 0 -
               -     'status=7', any interruption to the cluster run or
               -     the local machines is recovered manually
               -   .exit 0 and both directions' fb.*Link.txt files -
               -     'status=4'
               - phase 3: when it detects 'status=4' state, for lastz it
               - will construct the symlinks to get all the files ready
               - for pushing and it makes the entries in liftOverChain
               - and quickLiftChain tables.  For minimap2 those were
               - already made by pairMiniMap2.sh -liftTableLoad, only the
               - trackDb step is run, for both directions.
               - and then it sets 'status=5'
               - the 'status=5' states are detected by the 
               - ottoRequestPush.py cron job - which does the business of
               - getting the GenArk assembly hub.txt files built and everything
               - pushed out to hgdownload by using the 'make' commands built
               - into the source tree doc/*AsmHub/ directories.
               - TBD: add to ottoRequestPush.py the ability to push the
               -      UCSC database browser files.
               - Finally, watches for 'status=6' which was set by
               - ottoRequestPush.py upon push completion, now it will
               - use galaxyCleanup.py to release the galaxy history and WF
               - data, it will send out confirmation finished email and sets
               - 'status=8' to indicate completion.  The email ends with
               - notes on the kind of alignment: from version.txt for
               - minimap2, the .yaml file for lastz in galaxy, or the DEF
               - file for an existing in-house lastz alignment.

############################################################################
###  4.
ottoRequestAlign.sh - given an 'id' number in the ottoRequest table, this
                    - will start the alignment, lastz in galaxy via
                    - 'kegAlignLastz.sh', or minimap2 on the local cluster
                    - via 'pairMiniMap2.sh'.
                    - The choice is made by mashDistance.pl, with the same
                    - thresholds doMiniMap2.pl uses (AssemblyDivergence.pm):
                    - mash distance under 0.15 is minimap2, otherwise lastz.
                    - A mashDistance.pl failure sets 'status=7'.
                    - minimap2: the build directory is
                    -   <trackData or bed>/miniMap2<Query>.<date>
                    - (the newest existing one is reused), it writes
                    - ottoMiniMap2.sh there and runs it in the background:
                    -   pairMiniMap2.sh -liftTableLoad <target> <query>
                    - leaving ottoMiniMap2.pid, ottoMiniMap2.log and,
                    - when it ends, ottoMiniMap2.exit for ottoRequestWatch.sh
                    - At most maxMiniMap2 (2) minimap2 requests run at once,
                    - others stay at 'status=1' and are tried again on
                    - the next cron tick.
                    - lastz: generates the arguments to 'kegAlignLastz.sh'
                    - to get the alignment started in galaxy.
                    - Uses the hgcentraltest.genark table and the file
                    - dbDb.name.clade.tsv to determine full assembly ID
                    - names and 'clades' for the kegAlignLastz.sh script:
                    - primate - mammal - other - and this will decide which
                    - assembly will be target and query by checking their
                    - respective N50 sizes.  Also uses the file:
          dbDb.name.clade.tsv - to map the UCSC database names into
                          - GenArk 'clades' to make the selection:
                          - primate - mammal - other
                    - can set status to:
                    - 2 == in progress
                    - 7 == problems

############################################################################
###  5.
kegAlignLastz.sh - script to start the galaxy workflow, typical call:

    kegAlignLastz.sh GCF_004115215.2_mOrnAna1.pri.v4 GCF_000260355.1_ConCri1.0 mammal mammal

    This script starts the work in the target directory, and just does
    the job of getting the galaxy WF running.  The monitoring of the WF
    and the download of the results takes place in the other scripts.

############################################################################
###  5b.
pairMiniMap2.sh - in ~/kent/src/hg/utils/automation/ - runs doMiniMap2.pl
    for the primary target -> query alignment, then doMiniMap2.pl -swap
    for the query -> target direction, on the hgwdev parasol cluster:

    pairMiniMap2.sh -liftTableLoad GCA_018852615.3 hg19

    Continues in the newest existing miniMap2<Query>.<date> directory,
    doMiniMap2.pl skips the steps that are already done there.  With
    -liftTableLoad it also makes the liftOver/quickLift symlinks and the
    liftOverChain/quickLiftChain table entries.  Each direction leaves a
    dateless miniMap2.<other> symlink to its build directory.
    Exits non-zero on any failure, an interrupted cluster run stops
    doMiniMap2.pl with an error rather than being started over.

############################################################################
###  6.
workflowMonitor.sh - (lastz only) after the galaxy WF has started, this script can
                   - check the status of the job and if it is done, then
                   - the processing of the results will take place to
                   - construct the chain files.
                   - can set status to:
                   - 7 == problems
                   - 3 == galaxy finished

############################################################################
###  7.
### cron entry in hiram crontab:
# 4,26,46 * * * * ~/kent/src/hg/utils/otto/userRequests/ottoRequestPush.py
###
ottoRequestPush.py - cron job watching for status == 5 - will run the pushing
   procedure for the relevant assemblies.  When complete will set status = 6
   GenArk assemblies are pushed by the clade make chain, which picks up the
   hub build liftOver/ and quickLift/ directories for lastz and minimap2
   alike.  For a UCSC database target the .over.chain.gz is pushed from
   /hive/data/genomes/<db>/bed/liftOver/ (falls back to the lastz.<query>
   build directory for older lastz builds).
   TBD: Need to figure out how to push the UCSC database files out

############################################################################
###  8.
galaxyCleanup.py - used by ottoRequestWatch.sh to release all the data
   from galaxy after everything is complete.
