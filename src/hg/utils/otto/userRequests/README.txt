The 'status' field in the ottoRequest table maintains state:

  0 == request has been received by API (set by API)
  1 == request has been acknowledged by ottoRequest.py (set by ottoRequest.py)
  2 == galaxy jobs have been started (set by ottoRequestAlign.sh)
  3 == galaxy jobs have completed, download started (set by workflowMonitor.sh)
  4 == download from galaxy has taken place and track files have been created
         (set by ottoRequestWatch.sh)
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
ottoRequestWatch.sh - cron script running in hiram hgwdev account to watch
               - the ottoRequest table.  When new entries show
               - up (status=1) it will get the galaxy workflow
               - running by using ottoRequestAlign.sh to construct the
               - kegAlignLastz.sh script arguments and starts that process.
               - ottoRequestAlign.sh will set 'status=2' as the galaxy WF is on.
               - Uses workflowMonitor.sh to check the status of jobs that
               - are in 'status=2' state, when the galaxy run is complete,
               - will set 'status=4'
               - when it detects 'status=4' state, it will construct the
               - symlinks to get all the files ready for pushing and it
               - makes the entries in liftOverChain and quickLiftChain tables.
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
               - 'status=8' to indicate completion.  TBD: expand the email
               - message to include links to the download fles.

############################################################################
###  4.
ottoRequestAlign.sh - given an 'id' number in the ottoRequest table, this
                    - will generate the arguments to: 'kegAlignLastz.sh' 
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
###  6.
workflowMonitor.sh - after the galaxy WF has started, this script can
                   - check the status of the job and if it is done, then
                   - the processing of the results will take place to
                   - construct the chain files.
                   - can set status to:
                   - 7 == problems
                   - 3 == galaxy finished

############################################################################
###  7.
ottoRequestPush.py - cron job watching for status == 5 - will run the pushing
   procedure for the relevant assemblies.  When complete will set status = 6
   TBD: Need to figure out how to push the UCSC database files out

############################################################################
###  8.
galaxyCleanup.py - used by ottoRequestWatch.sh to release all the data
   from galaxy after everything is complete.
