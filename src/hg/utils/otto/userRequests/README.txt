Method of operation:

############################################################################
###  1.
User makes request via the liftRequest.html page.

The submit button causes a call to the API with four arguments:
  fromGenome toGenome email comment
The API does an INSERT operation into the hgcentral.ottoRequest table:
"INSERT INTO %s (requestType, fromDb, toDb, email, comment, requestTime,
  doneStatus, buildDir) VALUES ( 'liftOver', '%s','%s','%s','%s',now(), 0, '')",
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
  doneStatus: 0
    buildDir: 
completeTime: NULL

The 'doneStatus' field is going to keep track:
  0 == pending, 1 == notified/in progress, 2 == complete, 3 == problems
and will affect other operations.

############################################################################
###  2.
ottoRequest.py - otto user cron job running each minute to watch the
               - ottoRequest table in hgcentral - when new entries are
               - detected (doneStatus==0) it marks the table entry as
               - pending (doneStatus=1) and sends
               - out notification emails, one to the requesting user and
               - the other to UCSC via the specification in hg.conf:
  chainFileRequestEmail=chain-file-request-group@ucsc.edu
  apiFromEmail=genome-www@soe.ucsc.edu
               - TBD: verify bounces get back to that apiFromEmail
               -      this email bounce operation might work much better here
               -      on hgwdev and it isn't from the 'apache' user

############################################################################
###  3.
ottoRequestWatch.sh - cron script running in hiram hgwdev account to watch
               - the ottoRequest table.  When new entries show
               - up (doneStatus=1) it will get the galaxy workflow
               - running by using ottoRequestAlign.sh to construct the
               - kegAlignLastz.sh script arguments

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

############################################################################
###  5.
ketAlignLastz.sh - script to start the galaxy workflow, typical call:

    kegAlignLastz.sh GCF_004115215.2_mOrnAna1.pri.v4 GCF_000260355.1_ConCri1.0 mammal mammal


############################################################################
###  6.
workflowMonitor.sh - after the galaxy WF has started, this script can
                   - check the status of the job and if it is done, then
                   - then the processing of the results will take place to
                   - construct the chain files.
